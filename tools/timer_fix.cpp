#include "timer_fix.hpp"
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/plugbase.hpp"
#include "../src/log.hpp"

// For some reason these timers get reset when game loads state, so let's manually remember them
// FIXME: seems to be it's unstable during loading a state from a different scene and may crash

of::expected<void, std::string> timer_fix::save(std::vector<IntPair>& data) {
    auto& cfg = conf::get();
    if (!cfg.allow_timers_fix)
        return {};
    ASS(cfg.tm_fix_event_entry_offset != 0);
    ASS(cfg.tm_fix_event_entry_type_offset != 0);
    void* gStats = plug::get().get_prop(plug::PtrProp::PStats);
    ASS(gStats != nullptr);
    // of::debug("gStats before: {}", gStats);
    size_t eventGroup = *reinterpret_cast<size_t*>(reinterpret_cast<size_t>(gStats) + 0x80);
    if (eventGroup == 0) {
        of::error("eventGroup is nullptr WTF");
        return of::unexpected<std::string>("failed to save timers - eventGroup was nullptr");
    }
    while (*reinterpret_cast<short*>(eventGroup) != 0) {
        uint8_t* cond = reinterpret_cast<uint8_t*>(eventGroup) + cfg.tm_fix_event_entry_offset;
        int type = *reinterpret_cast<int*>(eventGroup + cfg.tm_fix_event_entry_type_offset);
        if (type != -0xa0001 && type != -0x90001) {
            uint8_t nEvents = *reinterpret_cast<uint8_t*>(eventGroup + 2);
            uint8_t nConds = *reinterpret_cast<uint8_t*>(eventGroup + 3);
            int totalEntries = static_cast<int>(nConds) + static_cast<int>(nEvents);
            for (int i = 0; i < totalEntries; i++) {
                // Is it really uint8_t or uint16_t??????????
                auto paramCount = *reinterpret_cast<uint8_t*>(cond + 0xC);
                if (paramCount != 0) {
                    int32_t objectID = *reinterpret_cast<int32_t*>(cond + 2);
                    uint8_t* cond2 = cond + ((-1 < objectID) ? 0x0E : 0x10);
                    for (int p = 0; p < static_cast<int>(paramCount); p++) {
                        // Type should be 0xD
                        if (*reinterpret_cast<uint16_t*>(cond2 + 2) == 0xD) {
                            int intervalValue = *reinterpret_cast<int*>(cond2 + 4);
                            int timerValue = *reinterpret_cast<int*>(cond2 + 8);
                            data.push_back(IntPair(intervalValue, timerValue));
                            // of::debug("saved {} / {}", timerValue, intervalValue);
                        }
                        // Add it's size (first value of the struct)
                        cond2 += *reinterpret_cast<uint16_t*>(cond2);
                    }
                }
                // Add it's size (first value of the struct)
                cond += *reinterpret_cast<uint16_t*>(cond);
            }
        }
        // Substract it's size (first value of the struct)
        eventGroup -= static_cast<size_t>(*reinterpret_cast<short*>(eventGroup));
    }
    of::debug("timers fix size: {}", data.size());
    return {};
}

of::expected<void, std::string> timer_fix::load(std::vector<IntPair> data) {
    auto& cfg = conf::get();
    if (!cfg.allow_timers_fix) {
        if (!data.empty())
            of::warn("Not fixing timers");
        return {};
    }
    if (data.empty())
        return {};
    auto it = data.begin();
    void* gStats = plug::get().get_prop(plug::PtrProp::PStats);
    // of::debug("gStats before: {}", gStats);
    size_t eventGroup = *reinterpret_cast<size_t*>(reinterpret_cast<size_t>(gStats) + 0x80);
    if (eventGroup == 0) {
        of::error("eventGroup is nullptr WTF");
        return of::unexpected<std::string>("Failed to load timers - eventGroup was nullptr");
    }
    while (*reinterpret_cast<short*>(eventGroup) != 0) {
        uint8_t* cond = reinterpret_cast<uint8_t*>(eventGroup) + cfg.tm_fix_event_entry_offset;
        int type = *reinterpret_cast<int*>(eventGroup + cfg.tm_fix_event_entry_type_offset);
        if (type != -0xa0001 && type != -0x90001) {
            uint8_t nEvents = *reinterpret_cast<uint8_t*>(eventGroup + 2);
            uint8_t nConds = *reinterpret_cast<uint8_t*>(eventGroup + 3);
            int totalEntries = static_cast<int>(nConds) + static_cast<int>(nEvents);
            for (int i = 0; i < totalEntries; i++) {
                auto paramCount = *reinterpret_cast<uint8_t*>(cond + 0xC);
                if (paramCount != 0) {
                    int32_t objectID = *reinterpret_cast<int32_t*>(cond + 2);
                    uint8_t* cond2 = cond + ((-1 < objectID) ? 0x0E : 0x10);
                    for (int p = 0; p < static_cast<int>(paramCount); p++) {
                        // Type should be 0xD
                        if (*reinterpret_cast<uint16_t*>(cond2 + 2) == 0xD) {
                            int intervalValue = *reinterpret_cast<int*>(cond2 + 4);
                            int* timerValue = reinterpret_cast<int*>(cond2 + 8);
                            if (it == data.end())
                                return of::unexpected<std::string>(
                                    "WTF not enough data to fix timers");
                            if (intervalValue != it->first)
                                return of::unexpected<std::string>("Fixing timers gone wrong");
                            *timerValue = it->second;
                            it++;
                        }
                        // Add it's size (first value of the struct)
                        cond2 += *reinterpret_cast<uint16_t*>(cond2);
                    }
                }
                // Add it's size (first value of the struct)
                cond += *reinterpret_cast<uint16_t*>(cond);
            }
        }
        // Substract it's size (first value of the struct)
        eventGroup -= static_cast<size_t>(*reinterpret_cast<short*>(eventGroup));
    }
    ASS(it == data.end());
    return {};
}
