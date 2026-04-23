#include "timer_fix.hpp"
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include <spdlog/spdlog.h>

struct EventGroup {
    short length;
    unsigned char eventCount;
    unsigned char condCount;
    unsigned short flags;
    short groupTimerBase;
    short field5_0x8;
    short field6_0xa;
    unsigned char field7_0xc;
    unsigned char field8_0xd;
    short condStart;
    int type;
    unsigned char field11_0x14;
    unsigned char field12_0x15;
    unsigned char field13_0x16;
    unsigned char field14_0x17;
    unsigned char field15_0x18;
    unsigned char field16_0x19;
    unsigned char field17_0x1a;
    unsigned char field18_0x1b;
    unsigned char field19_0x1c;
    unsigned char field20_0x1d;
    unsigned char field21_0x1e;
    unsigned char field22_0x1f;
    unsigned char field23_0x20;
    unsigned char field24_0x21;
    unsigned short groupFlags;
};

struct ConditionHeader {
    short size;
    short field1_0x2;
    short condID;
    short field3_0x6;
    short field4_0x8;
    unsigned char field5_0xa;
    unsigned char unk1;
    unsigned char unk2;
    unsigned char unk3;
    unsigned char unk4;
    unsigned char unk5;
    unsigned char parameterData;
    unsigned char unk6;
    short conditionType;
    int interval;
    int currentTimer;
};

ost::expected<void, std::string> timer_fix::save(std::vector<int>& data) {
    auto& cfg = conf::get();
    EventGroup* eventPtr =
        *reinterpret_cast<EventGroup**>(reinterpret_cast<size_t>(cfg.gStats) + 0x80);
    while (eventPtr->length != 0) {
        ConditionHeader* cond = (ConditionHeader*)&eventPtr->condStart;
        for (int i = (int)eventPtr->eventCount; i != 0; i--) {
            if (cond->condID == -4) {
                data.push_back(cond->interval);
                data.push_back(cond->currentTimer);
            }
            cond = (ConditionHeader*)((size_t)cond + (size_t)cond->size);
        }
        eventPtr = (EventGroup*)((size_t)eventPtr - (size_t)eventPtr->length);
    }
    spdlog::debug("timers fix size: {}", data.size());
    return {};
}

ost::expected<void, std::string> timer_fix::load(std::vector<int> data) {
    auto& cfg = conf::get();
    ASS(data.size() % 2 == 0);
    EventGroup* eventPtr =
        *reinterpret_cast<EventGroup**>(reinterpret_cast<size_t>(cfg.gStats) + 0x80);
    auto it = data.begin();
    while (eventPtr->length != 0) {
        ConditionHeader* cond = (ConditionHeader*)&eventPtr->condStart;
        for (int i = (int)eventPtr->eventCount; i != 0; i--) {
            if (cond->condID == -4) {
                ASS(it != data.end());
                int interval = *(it++);
                int timer = *(it++);
                ENSURE(interval == cond->interval);
                cond->currentTimer = timer;
                // TODO: maybe return error actually when failed?
            }
            cond = (ConditionHeader*)((size_t)cond + (size_t)cond->size);
        }
        eventPtr = (EventGroup*)((size_t)eventPtr - (size_t)eventPtr->length);
    }
    return {};
}
