#pragma once
#include <Windows.h>

namespace lock {
class CriticalSection {
private:
    CRITICAL_SECTION section;

public:
    inline CriticalSection() { InitializeCriticalSection(&section); }
    inline ~CriticalSection() { DeleteCriticalSection(&section); }
    inline CRITICAL_SECTION& get() { return section; }
};

class CSLock {
private:
    CRITICAL_SECTION* m_cs;

public:
    inline CSLock(CriticalSection& cs) : m_cs(&cs.get()) { EnterCriticalSection(m_cs); }
    inline void unlock() {
        if (m_cs) {
            LeaveCriticalSection(m_cs);
            m_cs = nullptr;
        }
    }
    inline ~CSLock() { unlock(); }
};
} // namespace lock
