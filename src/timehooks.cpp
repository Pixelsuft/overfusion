#define WIN32_LEAN_AND_MEAN
#include "timehooks.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "mem.hpp"
#include "state.hpp"
#include "ui.hpp"
#include <Windows.h>
#include <map>
#include <mmsystem.h>
#include <spdlog/spdlog.h>
#include <timeapi.h>

struct my_timeb {
    __time32_t time;
    unsigned short millitm;
    short timezone;
    short dstflag;
};

struct UserTimer {
    TIMERPROC cb;
    UINT_PTR event;
    UINT elapse;
    UINT counter;

    UserTimer() : event(0), elapse(0), counter(0), cb(nullptr) {}
    UserTimer(UINT_PTR event, UINT elapse, TIMERPROC cb)
        : event(event), elapse(elapse), cb(cb), counter(0) {}
};

struct MMTimer {
    LPTIMECALLBACK cb;
    DWORD_PTR data;
    UINT elapse;
    UINT counter;
    UINT flags;

    MMTimer() : cb(nullptr), data(0), elapse(0), counter(0), flags(0) {}
    MMTimer(LPTIMECALLBACK cb, DWORD_PTR data, UINT elapse, UINT flags)
        : cb(cb), data(data), elapse(elapse), flags(flags), counter(0) {}
};

namespace timehooks {
static std::map<std::pair<HWND, UINT_PTR>, UserTimer> user_timers;
static std::map<MMRESULT, MMTimer> mm_timers;
static MMRESULT mm_timer_counter;
} // namespace timehooks

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
    ENSURE(FileTimeToSystemTime(&ft, lpSystemTime));
}

static BOOL SetLocalTimeH(const SYSTEMTIME* lpSystemTime) { return FALSE; }

BOOL(WINAPI* QueryPerformanceFrequencyO)(LARGE_INTEGER* lpFrequency) = QueryPerformanceFrequency;
static BOOL WINAPI QueryPerformanceFrequencyH(LARGE_INTEGER* lpFrequency) {
    if (ui::is_processing())
        return QueryPerformanceFrequencyO(lpFrequency);
    lpFrequency->QuadPart = 1000000;
    return TRUE;
}

BOOL(WINAPI* QueryPerformanceCounterO)(LARGE_INTEGER* lpFrequency) = QueryPerformanceCounter;
static BOOL WINAPI QueryPerformanceCounterH(LARGE_INTEGER* lpFrequency) {
    if (ui::is_processing())
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

static time_t __cdecl timeH(time_t* tloc) {
    if (tloc)
        *tloc = state::get_time(state::TimeOffset::System) / 1000;
    return state::get_time(state::TimeOffset::System) / 1000;
}

static void __cdecl _ftimeH(struct my_timeb* timeptr) {
    if (timeptr) {
        timeptr->time = static_cast<__time32_t>(state::get_time(state::TimeOffset::System) / 1000);
        timeptr->millitm =
            static_cast<unsigned short>(state::get_time(state::TimeOffset::System) % 1000);
        long long diff_ms =
            state::get_time(state::TimeOffset::System) - state::get_time(state::TimeOffset::Local);
        timeptr->timezone = static_cast<short>(diff_ms / 60000);
        timeptr->dstflag = 0; // TODO: mb set it from state
    }
}

static UINT_PTR WINAPI SetTimerH(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse,
                                 TIMERPROC lpTimerFunc) {
    if (nIDEvent == 0xb)
        spdlog::warn("Detected use of SetTimer for frame sync, expect problems");
    if (uElapse < USER_TIMER_MINIMUM)
        uElapse = USER_TIMER_MINIMUM;
    else if (uElapse > USER_TIMER_MAXIMUM)
        uElapse = USER_TIMER_MAXIMUM;
    // spdlog::debug("SetTimer: {} {} {}", nIDEvent, uElapse, reinterpret_cast<void*>(lpTimerFunc));
    // Who needs thread safety, lul
    for (auto& timer : timehooks::user_timers) {
        if ((hWnd == nullptr || timer.first.first == hWnd) && nIDEvent == timer.second.event) {
            timer.second.counter = 0;
            timer.second.elapse = uElapse;
            timer.second.cb = lpTimerFunc;
            return timer.first.second;
        }
    }
    timehooks::user_timers[{hWnd, nIDEvent}] = UserTimer(nIDEvent, uElapse, lpTimerFunc);
    return nIDEvent;
}

static BOOL WINAPI KillTimerH(HWND hWnd, UINT_PTR uIDEvent) {
    auto it = timehooks::user_timers.find({hWnd, uIDEvent});
    if (it == timehooks::user_timers.end())
        return FALSE;
    // spdlog::debug("KillTimer: {}", uIDEvent);
    timehooks::user_timers.erase(it);
    return TRUE;
}

static MMRESULT WINAPI timeSetEventH(UINT uDelay, UINT uResolution, LPTIMECALLBACK lpTimeProc,
                                     DWORD_PTR dwUser, UINT fuEvent) {
    // Who needs thread safety, lul
    while ((timehooks::mm_timers.find(timehooks::mm_timer_counter) != timehooks::mm_timers.end()) ||
           timehooks::mm_timer_counter == 0)
        timehooks::mm_timer_counter++;
    timehooks::mm_timers[timehooks::mm_timer_counter] =
        MMTimer(lpTimeProc, dwUser, uDelay, fuEvent);
    // spdlog::debug("timeSetEvent: {} {} -> {}", uDelay, uResolution, mm_timer_counter);
    return timehooks::mm_timer_counter++;
}

static MMRESULT WINAPI timeKillEventH(UINT uTimerID) {
    // spdlog::debug("timeKillEvent: {}", uTimerID);
    auto it = timehooks::mm_timers.find(uTimerID);
    if (it == timehooks::mm_timers.end())
        return MMSYSERR_INVALPARAM;
    timehooks::mm_timers.erase(it);
    return TIMERR_NOERROR;
}

void timehooks::init() {
    auto& cfg = conf::get();
    if (cfg.boxed_mode)
        return;
    mm_timer_counter = 1;
    HOOK_ONLY("winmm.dll", timeGetSystemTime);
    HOOK_AUTO("winmm.dll", timeGetTime);
    HOOK_AUTO("kernel32.dll", QueryPerformanceFrequency);
    HOOK_ONLY("kernel32.dll", GetTickCount);
    HOOK_ONLY("kernel32.dll", SetLocalTime);
    HOOK_ONLY("msvcrt.dll", _ftime);
    HOOK_ONLY("msvcrt.dll", time);
    if (cfg.emulate_user_timers) {
        HOOK_ONLY("user32.dll", SetTimer);
        HOOK_ONLY("user32.dll", KillTimer);
    }
    if (cfg.emulate_mm_timers) {
        HOOK_ONLY("winmm.dll", timeSetEvent);
        HOOK_ONLY("winmm.dll", timeKillEvent);
    }
}

void timehooks::update_init() {
    auto& cfg = conf::get();
    if (cfg.boxed_mode)
        return;
    // FIXME: breaks IWBTB admin mod for some reason
    HOOK_AUTO("kernel32.dll", QueryPerformanceCounter);
    // This breaks fontembed.mfx if hooked earlier
    HOOK_ONLY("kernel32.dll", GetSystemTimeAsFileTime);
    // This breaks Nvidia driver if hooked earlier
    // FIXME: This also crashes FNAF (WTF?)
    // HOOK_ONLY("kernel32.dll", GetLocalTime);
    // TODO: GetSystemTime?
}

void timehooks::update(int dt) {
    for (auto& timer : user_timers) {
        timer.second.counter += static_cast<UINT>(dt);
        while (timer.second.counter >= timer.second.elapse) {
            timer.second.counter -= timer.second.elapse;
            state::set_temp_time_offset(-static_cast<int>(timer.second.counter));
            // FIXME
            if (timer.second.cb && 0)
                timer.second.cb(timer.first.first, WM_TIMER, timer.second.event, GetTickCountH());
            state::set_temp_time_offset(0);
        }
    }
    for (auto& timer : mm_timers) {
        timer.second.counter += static_cast<UINT>(dt);
        while (timer.second.counter >= timer.second.elapse) {
            timer.second.counter -= timer.second.elapse;
            state::set_temp_time_offset(-static_cast<int>(timer.second.counter));
            if (timer.second.cb)
                timer.second.cb(timer.first, WM_TIMER, timer.second.data, 0, 0);
            state::set_temp_time_offset(0);
            // TODO: support one-shot timers
        }
    }
}
