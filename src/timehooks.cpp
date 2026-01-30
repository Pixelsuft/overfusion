#define WIN32_LEAN_AND_MEAN
#include "timehooks.hpp"
#include "ass.hpp"
#include "mem.hpp"
#include "state.hpp"
#include "ui.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>
#include <timeapi.h>

static MMRESULT WINAPI timeGetSystemTimeH(LPMMTIME pmmt, UINT cbmmt) {
    if (pmmt == NULL || cbmmt < sizeof(MMTIME))
        return TIMERR_NOCANDO;
    DWORD currentTime = static_cast<DWORD>(state::get_time(state::TimeOffset::Startup));
    pmmt->wType = TIME_MS;
    pmmt->u.ms = currentTime;
    return TIMERR_NOERROR;
}

static DWORD(WINAPI* timeGetTimeO)();
static DWORD WINAPI timeGetTimeH() {
    // Only temporary
    // return timeGetTimeO();
    // FIXME
    return static_cast<DWORD>(state::get_time(state::TimeOffset::Startup));
}

static DWORD WINAPI GetTickCountH() {
    return static_cast<DWORD>(state::get_time(state::TimeOffset::Startup));
}

static VOID GetLocalTimeH(LPSYSTEMTIME lpSystemTime) {
    auto ms = state::get_time(state::TimeOffset::Local);
    ULARGE_INTEGER ull;
    ull.QuadPart = (ms + 11644473600000ULL) * 10000ULL;
    FILETIME ft;
    ft.dwLowDateTime = ull.LowPart;
    ft.dwHighDateTime = ull.HighPart;
    ASS(FileTimeToSystemTime(&ft, lpSystemTime));
}

static UINT_PTR(WINAPI* SetTimerO)(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse,
                                   TIMERPROC lpTimerFunc);
static UINT_PTR WINAPI SetTimerH(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse,
                                 TIMERPROC lpTimerFunc) {
    if (nIDEvent == 0xb) {
        spdlog::warn("Detected use of SetTimer for frame sync, expect problems");
    }
    // TODO: more research
    // spdlog::debug("SetTimer: {} {}", nIDEvent, uElapse);
    return SetTimerO(hWnd, nIDEvent, uElapse, lpTimerFunc);
}

static BOOL(WINAPI* QueryPerformanceFrequencyO)(LARGE_INTEGER* lpFrequency);
static BOOL WINAPI QueryPerformanceFrequencyH(LARGE_INTEGER* lpFrequency) {
    if (ui::processing)
        return QueryPerformanceFrequencyO(lpFrequency);
    lpFrequency->QuadPart = 1000000;
    return TRUE;
}

static BOOL(WINAPI* QueryPerformanceCounterO)(LARGE_INTEGER* lpFrequency);
static BOOL WINAPI QueryPerformanceCounterH(LARGE_INTEGER* lpFrequency) {
    if (ui::processing)
        return QueryPerformanceCounterO(lpFrequency);
    lpFrequency->QuadPart = state::get_time(state::TimeOffset::None) * 1000 +
                            state::get_time(state::TimeOffset::Reminder);
    return TRUE;
}

static VOID GetSystemTimeAsFileTimeH(LPFILETIME lpSystemTimeAsFileTime) {
    if (lpSystemTimeAsFileTime == NULL)
        return;
    // TODO: is this right?
    long long timeMs = state::get_time(state::TimeOffset::System);
    long long time100ns = timeMs * 10000LL;
    long long remainderUs = state::get_time(state::TimeOffset::Reminder);
    long long remainder100ns = remainderUs * 10LL;
    long long totalTime100ns = time100ns + remainder100ns;
    lpSystemTimeAsFileTime->dwLowDateTime = static_cast<DWORD>(totalTime100ns & 0xFFFFFFFF);
    lpSystemTimeAsFileTime->dwHighDateTime = static_cast<DWORD>(totalTime100ns >> 32);
}

static MMRESULT(WINAPI* timeSetEventO)(UINT uDelay, UINT uResolution, void* lpTimeProc,
                                       DWORD_PTR dwUser, UINT fuEvent);
static MMRESULT WINAPI timeSetEventH(UINT uDelay, UINT uResolution, void* lpTimeProc,
                                     DWORD_PTR dwUser, UINT fuEvent) {
    auto ret = timeSetEventO(uDelay, uResolution, lpTimeProc, dwUser, fuEvent);
    // spdlog::debug("timeSetEvent: {} {} {}", uDelay, uResolution, ret);
    return ret;
}

static MMRESULT(WINAPI* timeKillEventO)(UINT uTimerID);
static MMRESULT WINAPI timeKillEventH(UINT uTimerID) {
    // spdlog::debug("timeKillEvent: {}", uTimerID);
    return timeKillEventO(uTimerID);
}

void timehooks::init() {
    HOOK_ONLY("winmm.dll", timeGetSystemTime);
    HOOK_AUTO("winmm.dll", timeGetTime);
    HOOK_AUTO("winmm.dll", timeSetEvent);
    HOOK_AUTO("winmm.dll", timeKillEvent);
    HOOK_AUTO("user32.dll", SetTimer);
    HOOK_AUTO("kernel32.dll", QueryPerformanceFrequency);
    HOOK_ONLY("kernel32.dll", GetTickCount);
    HOOK_ONLY("kernel32.dll", GetLocalTime);
}

void timehooks::update_init() {
    HOOK_AUTO("kernel32.dll", QueryPerformanceCounter);
    // This breaks fontembed.mfx if hooked earlier
    HOOK_ONLY("kernel32.dll", GetSystemTimeAsFileTime);
}
