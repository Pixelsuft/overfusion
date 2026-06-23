#include "timer_fix.hpp"
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/plugbase.hpp"
#include <spdlog/spdlog.h>

// For some reason these timers get reset when game loads state, so let's manually remember them
// FIXME: seems to be it's unstable during loading a state from a different scene and may crash

ost::expected<void, std::string> timer_fix::save(std::vector<IntPair>& data) {
    auto& cfg = conf::get();
    if (!cfg.allow_timers_fix)
        return {};
    ASS(cfg.tm_fix_event_entry_offset != 0);
    ASS(cfg.tm_fix_event_entry_type_offset != 0);
    void* gStats = plug::get().get_prop(plug::PtrProp::PStats);
    ASS(gStats != nullptr);
    // spdlog::debug("gStats before: {}", gStats);
    size_t eventPtr = *reinterpret_cast<size_t*>(reinterpret_cast<size_t>(gStats) + 0x80);
    if (eventPtr == 0) {
        spdlog::error("eventPtr is nullptr WTF");
        return ost::unexpected<std::string>("failed to save timers - eventPtr was nullptr");
    }
    while (*reinterpret_cast<short*>(eventPtr) != 0) {
        uint8_t nEvents = *reinterpret_cast<uint8_t*>(eventPtr + 2);
        uint8_t nConds = *reinterpret_cast<uint8_t*>(eventPtr + 3);
        int totalEntries = static_cast<int>(nConds) + static_cast<int>(nEvents);
        uint8_t* currentEntry =
            reinterpret_cast<uint8_t*>(eventPtr) + cfg.tm_fix_event_entry_offset;
        int type = *reinterpret_cast<int*>(currentEntry + cfg.tm_fix_event_entry_type_offset);
        if (type != -0xa0001 && type != -0x90001) {
            for (int i = 0; i < totalEntries; i++) {
                uint16_t entrySize = *reinterpret_cast<uint16_t*>(currentEntry);
                int32_t objectID = *reinterpret_cast<int32_t*>(currentEntry + 2);
                uint8_t paramCount = *reinterpret_cast<uint8_t*>(currentEntry + 0xC);
                if (paramCount > 0) {
                    uint8_t* paramPtr = currentEntry + (objectID < 0 ? 0x10 : 0x0E);
                    for (int p = 0; p < static_cast<int>(paramCount); p++) {
                        uint16_t pSize = *reinterpret_cast<uint16_t*>(paramPtr);
                        uint16_t pType = *reinterpret_cast<uint16_t*>(paramPtr + 2);
                        if (pType == 0x0D) {
                            int intervalValue = *reinterpret_cast<int*>(paramPtr + 4);
                            int timerValue = *reinterpret_cast<int*>(paramPtr + 8);
                            data.push_back(IntPair(intervalValue, timerValue));
                            // spdlog::debug("saved {} / {}", timerValue, intervalValue);
                        }
                        paramPtr += pSize;
                    }
                }
                currentEntry += entrySize;
            }
        }
        eventPtr -= static_cast<size_t>(*reinterpret_cast<short*>(eventPtr));
    }
    spdlog::debug("timers fix size: {}", data.size());
    return {};
}

ost::expected<void, std::string> timer_fix::load(std::vector<IntPair> data) {
    auto& cfg = conf::get();
    if (!cfg.allow_timers_fix) {
        if (!data.empty())
            spdlog::warn("Not fixing timers");
        return {};
    }
    if (data.empty())
        return {};
    auto it = data.begin();
    void* gStats = plug::get().get_prop(plug::PtrProp::PStats);
    ASS(gStats != nullptr);
    // spdlog::debug("gStats before: {}", gStats);
    size_t eventPtr = *reinterpret_cast<size_t*>(reinterpret_cast<size_t>(gStats) + 0x80);
    if (eventPtr == 0) {
        spdlog::error("eventPtr is nullptr WTF");
        return ost::unexpected<std::string>("Failed to save timers - eventPtr was nullptr");
    }
    while (*reinterpret_cast<short*>(eventPtr) != 0) {
        uint8_t nEvents = *reinterpret_cast<uint8_t*>(eventPtr + 2);
        uint8_t nConds = *reinterpret_cast<uint8_t*>(eventPtr + 3);
        int totalEntries = static_cast<int>(nConds) + static_cast<int>(nEvents);
        uint8_t* currentEntry =
            reinterpret_cast<uint8_t*>(eventPtr) + cfg.tm_fix_event_entry_offset;
        int type = *reinterpret_cast<int*>(currentEntry + cfg.tm_fix_event_entry_type_offset);
        if (type != -0xa0001 && type != -0x90001) {
            for (int i = 0; i < totalEntries; i++) {
                uint16_t entrySize = *reinterpret_cast<uint16_t*>(currentEntry);
                int32_t objectID = *reinterpret_cast<int32_t*>(currentEntry + 2);
                uint8_t paramCount = *reinterpret_cast<uint8_t*>(currentEntry + 0xC);
                if (paramCount > 0) {
                    uint8_t* paramPtr = currentEntry + (objectID < 0 ? 0x10 : 0x0E);
                    for (int p = 0; p < static_cast<int>(paramCount); p++) {
                        uint16_t pSize = *reinterpret_cast<uint16_t*>(paramPtr);
                        uint16_t pType = *reinterpret_cast<uint16_t*>(paramPtr + 2);
                        if (pType == 0x0D) {
                            int intervalValue = *reinterpret_cast<int*>(paramPtr + 4);
                            int* timerValue = reinterpret_cast<int*>(paramPtr + 8);
                            if (it == data.end())
                                return ost::unexpected<std::string>(
                                    "WTF not enough data to fix timers");
                            if (intervalValue != it->first)
                                return ost::unexpected<std::string>("Fixing timers gone wrong");
                            *timerValue = it->second;
                            it++;
                        }
                        paramPtr += pSize;
                    }
                }
                currentEntry += entrySize;
            }
        }
        eventPtr -= static_cast<size_t>(*reinterpret_cast<short*>(eventPtr));
    }
    ASS(it == data.end());
    return {};
}
