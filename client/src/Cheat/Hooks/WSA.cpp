#include "pch.h"
#include "WSA.h"

#include "../Modules/Module.h"
#include "../Modules/Settings.h"
#include "../../Helper/Communication.h"
#include "../../Helper/HookFunction.h"
#include "../../../vendors/minhook/MinHook.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>
#include <cstdint>

int(__stdcall* g_origWSASend)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD,
    LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

static std::mutex        g_wsaMutex;
static std::atomic<bool> g_wsa_shutdown{ false };
static std::atomic<bool> g_pingFix{ false };

void PingFix_Set(bool on) {
    g_pingFix.store(on, std::memory_order_relaxed);
}

bool PingFix_Active() {
    return g_pingFix.load(std::memory_order_relaxed);
}

static int ReadVarInt(const uint8_t*& p, const uint8_t* end, bool& ok) {
    ok = false;
    int value = 0;
    int shift = 0;
    while (p < end && shift <= 35) {
        const uint8_t b = *p++;
        value |= (int)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            ok = true;
            return value;
        }
        shift += 7;
    }
    return 0;
}

static bool PacketIdIsPing(int id) {
    return id == 0x00 || id == 0x0F || id == 0x6A;
}

static bool BytesArePingPackets(const uint8_t* data, size_t n) {
    if (!data || n == 0 || n > 2048)
        return false;
    const uint8_t* p = data;
    const uint8_t* end = data + n;
    bool any = false;
    while (p < end) {
        bool ok = false;
        const int pktLen = ReadVarInt(p, end, ok);
        if (!ok || pktLen <= 0 || pktLen > 64 || p + pktLen > end)
            return false;
        const uint8_t* frameEnd = p + pktLen;
        const int packetId = ReadVarInt(p, frameEnd, ok);
        if (!ok || !PacketIdIsPing(packetId))
            return false;
        if ((frameEnd - p) > 16)
            return false;
        p = frameEnd;
        any = true;
    }
    return any;
}

static bool BuffersArePing(LPWSABUF lpBuffers, DWORD dwBufferCount) {
    if (!g_pingFix.load(std::memory_order_relaxed) || !lpBuffers || dwBufferCount == 0)
        return false;
    size_t total = 0;
    for (DWORD i = 0; i < dwBufferCount; i++)
        total += lpBuffers[i].len;
    if (total == 0 || total > 2048)
        return false;
    std::vector<uint8_t> raw(total);
    size_t o = 0;
    for (DWORD i = 0; i < dwBufferCount; i++) {
        if (lpBuffers[i].buf && lpBuffers[i].len) {
            memcpy(raw.data() + o, lpBuffers[i].buf, lpBuffers[i].len);
            o += lpBuffers[i].len;
        }
    }
    return BytesArePingPackets(raw.data(), o);
}

static bool SendBytesArePing(const char* buf, int len) {
    if (!g_pingFix.load(std::memory_order_relaxed) || !buf || len <= 0)
        return false;
    return BytesArePingPackets(reinterpret_cast<const uint8_t*>(buf), (size_t)len);
}

struct QueuedSend {
    SOCKET s = INVALID_SOCKET;
    DWORD  flags = 0;
    ULONGLONG queuedAt = 0;
    std::vector<std::vector<char>> chunks;
};

static void FlushQueueLocked(std::vector<QueuedSend>& q);

static std::atomic<bool> g_abLag{ false };
static ULONGLONG         g_abLagUntil = 0;
static std::vector<QueuedSend> g_abQueue;

static void FlushLagLocked() {
    g_abLag.store(false, std::memory_order_relaxed);
    g_abLagUntil = 0;
    if (!g_origWSASend) {
        g_abQueue.clear();
        return;
    }
    for (auto& q : g_abQueue) {
        if (q.s == INVALID_SOCKET || q.chunks.empty()) continue;
        std::vector<WSABUF> bufs(q.chunks.size());
        for (size_t i = 0; i < q.chunks.size(); i++) {
            bufs[i].buf = q.chunks[i].empty() ? nullptr : q.chunks[i].data();
            bufs[i].len = (ULONG)q.chunks[i].size();
        }
        DWORD sent = 0;
        g_origWSASend(q.s, bufs.data(), (DWORD)bufs.size(), &sent, q.flags, nullptr, nullptr);
    }
    g_abQueue.clear();
}

void AutoBlock_LagFlush() {
    std::lock_guard<std::mutex> lock(g_wsaMutex);
    FlushLagLocked();
}

void AutoBlock_LagStart(int durationMs) {
    std::lock_guard<std::mutex> lock(g_wsaMutex);
    FlushLagLocked();
    if (durationMs <= 0) return;
    g_abLagUntil = GetTickCount64() + (ULONGLONG)durationMs;
    g_abLag.store(true, std::memory_order_relaxed);
}

void AutoBlock_LagTick() {
    if (!g_abLag.load(std::memory_order_relaxed)) return;
    if (GetTickCount64() < g_abLagUntil) return;
    AutoBlock_LagFlush();
}

bool AutoBlock_LagActive() {
    return g_abLag.load(std::memory_order_relaxed);
}

static std::atomic<bool> g_btLag{ false };
static std::vector<QueuedSend> g_btQueue;

void Backtrack_LagFlush() {
    std::lock_guard<std::mutex> lock(g_wsaMutex);
    g_btLag.store(false, std::memory_order_relaxed);
    FlushQueueLocked(g_btQueue);
}

void Backtrack_LagStart(int durationMs) {
    (void)durationMs;
    g_btLag.store(true, std::memory_order_relaxed);
}

void Backtrack_LagTick() {}

bool Backtrack_LagActive() {
    return g_btLag.load(std::memory_order_relaxed);
}

static std::atomic<bool> g_blinkOut{ false };
static std::atomic<bool> g_blinkIn{ false };
static std::vector<QueuedSend> g_blinkQueue;

static void EnqueueSendLocked(std::vector<QueuedSend>& q, SOCKET s, LPWSABUF lpBuffers,
    DWORD dwBufferCount, DWORD dwFlags, LPDWORD lpNumberOfBytesSent)
{
    QueuedSend item;
    item.s = s;
    item.flags = dwFlags;
    item.queuedAt = GetTickCount64();
    DWORD total = 0;
    item.chunks.reserve(dwBufferCount);
    for (DWORD i = 0; i < dwBufferCount; i++) {
        ULONG n = lpBuffers[i].len;
        const char* p = lpBuffers[i].buf;
        if (p && n)
            item.chunks.emplace_back(p, p + n);
        else
            item.chunks.emplace_back();
        total += n;
    }
    q.push_back(std::move(item));
    if (lpNumberOfBytesSent)
        *lpNumberOfBytesSent = total;
}

static void FlushQueueLocked(std::vector<QueuedSend>& q) {
    if (!g_origWSASend) {
        q.clear();
        return;
    }
    for (auto& item : q) {
        if (item.s == INVALID_SOCKET || item.chunks.empty()) continue;
        std::vector<WSABUF> bufs(item.chunks.size());
        for (size_t i = 0; i < item.chunks.size(); i++) {
            bufs[i].buf = item.chunks[i].empty() ? nullptr : item.chunks[i].data();
            bufs[i].len = (ULONG)item.chunks[i].size();
        }
        DWORD sent = 0;
        g_origWSASend(item.s, bufs.data(), (DWORD)bufs.size(), &sent, item.flags, nullptr, nullptr);
    }
    q.clear();
}

void Blink_OutFlush() {
    std::lock_guard<std::mutex> lock(g_wsaMutex);
    g_blinkOut.store(false, std::memory_order_relaxed);
    FlushQueueLocked(g_blinkQueue);
}

void Blink_OutStart() {
    g_blinkOut.store(true, std::memory_order_relaxed);
}

bool Blink_OutActive() {
    return g_blinkOut.load(std::memory_order_relaxed);
}

void Blink_InFlush() {
    g_blinkIn.store(false, std::memory_order_relaxed);
}

void Blink_InStart() {
    g_blinkIn.store(true, std::memory_order_relaxed);
}

bool Blink_InActive() {
    return g_blinkIn.load(std::memory_order_relaxed);
}

static std::atomic<bool> g_lrLag{ false };
static std::atomic<int>  g_lrDelayMs{ 0 };
static std::vector<QueuedSend> g_lrQueue;

static void FlushLagRangeLocked() {
    g_lrLag.store(false, std::memory_order_relaxed);
    FlushQueueLocked(g_lrQueue);
}

void LagRange_Flush() {
    std::lock_guard<std::mutex> lock(g_wsaMutex);
    FlushLagRangeLocked();
}

void LagRange_Start() {
    g_lrLag.store(true, std::memory_order_relaxed);
}

void LagRange_SetDelay(int delayMs) {
    g_lrDelayMs.store((std::max)(0, delayMs), std::memory_order_relaxed);
}

void LagRange_Tick() {
    const int delay = g_lrDelayMs.load(std::memory_order_relaxed);
    if (delay <= 0) return;
    if (!g_lrLag.load(std::memory_order_relaxed)) return;
    const ULONGLONG cutoff = GetTickCount64() - (ULONGLONG)delay;
    std::lock_guard<std::mutex> lock(g_wsaMutex);
    if (!g_origWSASend) {
        g_lrQueue.clear();
        return;
    }
    size_t i = 0;
    for (; i < g_lrQueue.size(); i++) {
        if (g_lrQueue[i].queuedAt > cutoff) break;
        auto& item = g_lrQueue[i];
        if (item.s == INVALID_SOCKET || item.chunks.empty()) continue;
        std::vector<WSABUF> bufs(item.chunks.size());
        for (size_t b = 0; b < item.chunks.size(); b++) {
            bufs[b].buf = item.chunks[b].empty() ? nullptr : item.chunks[b].data();
            bufs[b].len = (ULONG)item.chunks[b].size();
        }
        DWORD sent = 0;
        g_origWSASend(item.s, bufs.data(), (DWORD)bufs.size(), &sent, item.flags, nullptr, nullptr);
    }
    if (i > 0)
        g_lrQueue.erase(g_lrQueue.begin(), g_lrQueue.begin() + (std::ptrdiff_t)i);
}

bool LagRange_Active() {
    return g_lrLag.load(std::memory_order_relaxed);
}

void WSA_SignalShutdown() {
    g_wsa_shutdown = true;
    Blink_InFlush();
    std::lock_guard<std::mutex> lock(g_wsaMutex);
    g_blinkOut.store(false, std::memory_order_relaxed);
    g_btLag.store(false, std::memory_order_relaxed);
    FlushQueueLocked(g_blinkQueue);
    FlushQueueLocked(g_btQueue);
    FlushLagRangeLocked();
    FlushLagLocked();
}

int __stdcall WSASendHook(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    if (!g_wsa_shutdown.load(std::memory_order_acquire))
    {
        std::lock_guard<std::mutex> lock(g_wsaMutex);
        if (!g_wsa_shutdown.load(std::memory_order_relaxed))
        {
            for (const auto& mod : Modules::GetRegisteredModules())
                mod->OnReceiveData();

            const bool pingBypass = BuffersArePing(lpBuffers, dwBufferCount);
            if (!pingBypass)
            {
            bool lag = g_abLag.load(std::memory_order_relaxed);
            if (lag && GetTickCount64() >= g_abLagUntil)
                FlushLagLocked();
            lag = g_abLag.load(std::memory_order_relaxed);

            if (lag && !lpOverlapped && !lpCompletionRoutine && lpBuffers && dwBufferCount > 0) {
                if (g_abQueue.size() >= 256) {
                    FlushLagLocked();
                } else {
                    EnqueueSendLocked(g_abQueue, s, lpBuffers, dwBufferCount, dwFlags, lpNumberOfBytesSent);
                    return 0;
                }
            }

            const bool blinkOut = g_blinkOut.load(std::memory_order_relaxed);
            if (blinkOut && lpBuffers && dwBufferCount > 0) {
                if (g_blinkQueue.size() >= 512) {
                    FlushQueueLocked(g_blinkQueue);
                } else if (!lpOverlapped && !lpCompletionRoutine) {
                    EnqueueSendLocked(g_blinkQueue, s, lpBuffers, dwBufferCount, dwFlags, lpNumberOfBytesSent);
                    return 0;
                } else {
                    WSASetLastError(WSAEWOULDBLOCK);
                    return SOCKET_ERROR;
                }
            }

            const bool lrLag = g_lrLag.load(std::memory_order_relaxed);
            if (lrLag && lpBuffers && dwBufferCount > 0) {
                const int delay = g_lrDelayMs.load(std::memory_order_relaxed);
                if (delay > 0) {
                    const ULONGLONG cutoff = GetTickCount64() - (ULONGLONG)delay;
                    size_t i = 0;
                    for (; i < g_lrQueue.size(); i++) {
                        if (g_lrQueue[i].queuedAt > cutoff) break;
                        auto& item = g_lrQueue[i];
                        if (item.s == INVALID_SOCKET || item.chunks.empty() || !g_origWSASend) continue;
                        std::vector<WSABUF> bufs(item.chunks.size());
                        for (size_t b = 0; b < item.chunks.size(); b++) {
                            bufs[b].buf = item.chunks[b].empty() ? nullptr : item.chunks[b].data();
                            bufs[b].len = (ULONG)item.chunks[b].size();
                        }
                        DWORD sent = 0;
                        g_origWSASend(item.s, bufs.data(), (DWORD)bufs.size(), &sent, item.flags, nullptr, nullptr);
                    }
                    if (i > 0)
                        g_lrQueue.erase(g_lrQueue.begin(), g_lrQueue.begin() + (std::ptrdiff_t)i);
                }
                if (g_lrQueue.size() >= 512) {
                    FlushLagRangeLocked();
                } else if (!lpOverlapped && !lpCompletionRoutine) {
                    EnqueueSendLocked(g_lrQueue, s, lpBuffers, dwBufferCount, dwFlags, lpNumberOfBytesSent);
                    return 0;
                } else {
                    WSASetLastError(WSAEWOULDBLOCK);
                    return SOCKET_ERROR;
                }
            }

            const bool btLag = g_btLag.load(std::memory_order_relaxed);
            if (btLag && lpBuffers && dwBufferCount > 0) {
                if (g_btQueue.size() >= 512) {
                    FlushQueueLocked(g_btQueue);
                } else if (!lpOverlapped && !lpCompletionRoutine) {
                    EnqueueSendLocked(g_btQueue, s, lpBuffers, dwBufferCount, dwFlags, lpNumberOfBytesSent);
                    return 0;
                } else {
                    WSASetLastError(WSAEWOULDBLOCK);
                    return SOCKET_ERROR;
                }
            }
            }
        }
    }

    return g_origWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent,
        dwFlags, lpOverlapped, lpCompletionRoutine);
}

static int(__stdcall* g_origWSARecv)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD,
    LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
static int(__stdcall* g_origRecv)(SOCKET, char*, int, int);
static int(__stdcall* g_origSend)(SOCKET, const char*, int, int);

int __stdcall WSARecvHook(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    if (!g_wsa_shutdown.load(std::memory_order_acquire) && g_blinkIn.load(std::memory_order_relaxed)) {
        if (lpNumberOfBytesRecvd)
            *lpNumberOfBytesRecvd = 0;
        WSASetLastError(WSAEWOULDBLOCK);
        return SOCKET_ERROR;
    }
    return g_origWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags,
        lpOverlapped, lpCompletionRoutine);
}

int __stdcall RecvHook(SOCKET s, char* buf, int len, int flags) {
    if (!g_wsa_shutdown.load(std::memory_order_acquire) && g_blinkIn.load(std::memory_order_relaxed)) {
        WSASetLastError(WSAEWOULDBLOCK);
        return SOCKET_ERROR;
    }
    return g_origRecv(s, buf, len, flags);
}

int __stdcall SendHook(SOCKET s, const char* buf, int len, int flags) {
    if (SendBytesArePing(buf, len))
        return g_origSend(s, buf, len, flags);
    if (!g_wsa_shutdown.load(std::memory_order_acquire)
        && (g_blinkOut.load(std::memory_order_relaxed)
            || g_lrLag.load(std::memory_order_relaxed)
            || g_btLag.load(std::memory_order_relaxed))
        && !g_abLag.load(std::memory_order_relaxed)) {
        WSASetLastError(WSAEWOULDBLOCK);
        return SOCKET_ERROR;
    }
    return g_origSend(s, buf, len, flags);
}

static HookFunction hookFunc([]() {
    MH_CreateHookApi(L"Ws2_32.dll", "WSASend", WSASendHook, (void**)&g_origWSASend);
    MH_CreateHookApi(L"Ws2_32.dll", "WSARecv", WSARecvHook, (void**)&g_origWSARecv);
    MH_CreateHookApi(L"Ws2_32.dll", "recv", RecvHook, (void**)&g_origRecv);
    MH_CreateHookApi(L"Ws2_32.dll", "send", SendHook, (void**)&g_origSend);
    MH_EnableHook(MH_ALL_HOOKS);
    });
