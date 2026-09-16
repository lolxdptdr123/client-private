#pragma once
#include <WinSock2.h>

int __stdcall WSASendHook(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

void WSA_SignalShutdown();

void AutoBlock_LagStart(int durationMs);
void AutoBlock_LagFlush();
void AutoBlock_LagTick();
bool AutoBlock_LagActive();

void Backtrack_LagStart(int durationMs);
void Backtrack_LagFlush();
void Backtrack_LagTick();
bool Backtrack_LagActive();

void Blink_OutStart();
void Blink_OutFlush();
bool Blink_OutActive();
void Blink_InStart();
void Blink_InFlush();
bool Blink_InActive();

void LagRange_Start();
void LagRange_Flush();
void LagRange_SetDelay(int delayMs);
void LagRange_Tick();
bool LagRange_Active();

void PingFix_Set(bool on);
bool PingFix_Active();