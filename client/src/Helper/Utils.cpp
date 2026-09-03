#include "pch.h"
#include "Utils.h"
#include "../Cheat/Modules/Misc/Overlay.h"

#include <gl/GL.h>
#include <random>

// Buffer statique pour éviter le dangling pointer sur retour de stack
static char s_windowTitleBuf[256];

const char* GetWindowTitle(const char* windowClass)
{
    s_windowTitleBuf[0] = '\0';
    HWND hWnd = FindWindowA(windowClass, nullptr);
    if (hWnd)
        GetWindowTextA(hWnd, s_windowTitleBuf, sizeof(s_windowTitleBuf));
    return s_windowTitleBuf;
}

const char* GetWindowTitle(HWND hWnd)
{
    s_windowTitleBuf[0] = '\0';
    if (hWnd)
        GetWindowTextA(hWnd, s_windowTitleBuf, sizeof(s_windowTitleBuf));
    return s_windowTitleBuf;
}

bool IsBadReadPtr(void* p)
{
    MEMORY_BASIC_INFORMATION mbi = { 0 };
    if (::VirtualQuery(p, &mbi, sizeof(mbi)))
    {
        const DWORD mask = (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
        bool b = !(mbi.Protect & mask);
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) b = true;
        return b;
    }
    return true;
}

Vec4 Multiply(const Vec4& vec, const std::vector<float>& mat)
{
    if (mat.size() != 16)
        return Vec4();

    return Vec4(
        vec.x * mat[0] + vec.y * mat[4] + vec.z * mat[8] + vec.w * mat[12],
        vec.x * mat[1] + vec.y * mat[5] + vec.z * mat[9] + vec.w * mat[13],
        vec.x * mat[2] + vec.y * mat[6] + vec.z * mat[10] + vec.w * mat[14],
        vec.x * mat[3] + vec.y * mat[7] + vec.z * mat[11] + vec.w * mat[15]
    );
}

bool WorldToScreen(Vec3 worldPos, Vec2& screen, std::vector<float>& modelView, std::vector<float>& projection, int width, int height)
{
    if (projection.size() != 16 || modelView.size() != 16)
        return false;

    const auto clipSpacePos = Multiply(Multiply(Vec4(worldPos.x, worldPos.y, worldPos.z, 1.0f), modelView), projection);
    if (clipSpacePos.w <= 0.05f)
        return false;
    const auto ndcSpacePos = Vec3(clipSpacePos.x / clipSpacePos.w, clipSpacePos.y / clipSpacePos.w, clipSpacePos.z / clipSpacePos.w);

    screen.x = width * ((ndcSpacePos.x + 1.0f) / 2.0f);
    screen.y = height * ((1.0f - ndcSpacePos.y) / 2.0f);
    return true;
}

Vec3 FromHSB(float h, float s, float v)
{
    float _r, _g, _b;
    float _h = h == 1.0f ? 0 : h * 6.0f;
    auto  i = static_cast<int>(trunc(_h));
    float f = _h - i;

    const float p = v * (1.f - s);
    const float q = v * (1.f - s * f);
    const float t = v * (1.f - s * (1.f - f));

    switch (i) {
    case 0: _r = v; _g = t; _b = p; break;
    case 1: _r = q; _g = v; _b = p; break;
    case 2: _r = p; _g = v; _b = t; break;
    case 3: _r = p; _g = q; _b = v; break;
    case 4: _r = t; _g = p; _b = v; break;
    default:_r = v; _g = p; _b = q; break;
    }

    return Vec3((unsigned char)(_r * 255), (unsigned char)(_g * 255), (unsigned char)(_b * 255));
}

int RandomInteger(const int min, const int max)
{
    std::random_device rdd;
    std::mt19937 rnd(rdd());
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(rnd);
}

void sendClick(HWND window, int delay, bool isRight)
{
    POINT p;
    GetCursorPos(&p);
    SendMessageW(window, isRight ? 0x204 : 0x201, MK_LBUTTON, MAKELPARAM(0, 0));
    Sleep(delay);
    SendMessageW(window, isRight ? 0x205 : 0x202, MK_LBUTTON, MAKELPARAM(0, 0));
    Sleep(delay);
}

static BOOL CALLBACK EnumGameWindowProc(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[256]{};
    GetWindowTextW(hwnd, title, 256);
    wchar_t cls[64]{};
    GetClassNameW(hwnd, cls, 64);

    const bool lunarTitle = wcsstr(title, L"Lunar") != nullptr;
    const bool cbTitle = wcsstr(title, L"CheatBreaker") != nullptr || wcsstr(title, L"Cheatbreaker") != nullptr;
    const bool glClass = _wcsicmp(cls, L"LWJGL") == 0 || _wcsicmp(cls, L"GLFW30") == 0;
    if (!lunarTitle && !cbTitle && !glClass)
        return TRUE;

    *reinterpret_cast<HWND*>(lParam) = hwnd;
    return FALSE;
}

static bool SameWindowTree(HWND a, HWND b)
{
    if (!a || !b)
        return false;
    if (a == b)
        return true;
    HWND ra = GetAncestor(a, GA_ROOT);
    HWND rb = GetAncestor(b, GA_ROOT);
    return ra && rb && ra == rb;
}

HWND FindLunarWindow()
{
    if (Overlay::gameHwnd && IsWindow(Overlay::gameHwnd))
        return Overlay::gameHwnd;

    HWND h = FindWindowW(nullptr, L"Lunar Client 1.8.9");
    if (!h) h = FindWindowW(nullptr, L"Lunar Client 1.7.10");
    if (!h) h = FindWindowW(nullptr, L"CheatBreaker");
    if (!h) EnumWindows(EnumGameWindowProc, reinterpret_cast<LPARAM>(&h));
    if (!h) h = FindWindowW(L"LWJGL", nullptr);
    if (!h) h = FindWindowW(L"GLFW30", nullptr);
    return h;
}

bool IsGameWindowFocused()
{
    HWND fg = GetForegroundWindow();
    if (!fg)
        return false;

    if (SameWindowTree(fg, Overlay::gameHwnd))
        return true;

    HWND lunar = FindLunarWindow();
    if (SameWindowTree(fg, lunar))
        return true;

    if (Overlay::gameHwnd && IsWindow(Overlay::gameHwnd)) {
        DWORD pidFg = 0, pidGame = 0;
        GetWindowThreadProcessId(fg, &pidFg);
        GetWindowThreadProcessId(Overlay::gameHwnd, &pidGame);
        if (pidFg && pidFg == pidGame)
            return true;
    }
    return false;
}