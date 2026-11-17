#include "storage_slots.h"

#include <cstdio>
#include <cstring>
#include <iterator>

namespace trinity::game::StorageSlots
{
    int Step()
    {
        const int i = State::Get().invStepIdx;
        return (i >= 0 && i < static_cast<int>(std::size(kSteps))) ? kSteps[i] : 10;
    }

    State::StorageSlots* Find(const char* key)
    {
        if (!key || !*key) return nullptr;
        State& st = State::Get();
        for (int i = 0; i < st.invStorageSlotCount; ++i)
            if (!strcmp(st.invStorageSlots[i].key, key)) return &st.invStorageSlots[i];
        return nullptr;
    }

    State::StorageSlots* FindOrAdd(const char* key, int initialValue)
    {
        if (State::StorageSlots* e = Find(key)) return e;
        State& st = State::Get();
        if (!key || !*key || st.invStorageSlotCount >= State::kMaxStorageSlots) return nullptr;
        State::StorageSlots& e = st.invStorageSlots[st.invStorageSlotCount++];
        snprintf(e.key, sizeof(e.key), "%s", key);
        e.on = false;
        e.value = initialValue;
        return &e;
    }

    int TargetFor(const char* key)
    {
        if (const State::StorageSlots* e = Find(key); e && e->on) return e->value;
        const State& st = State::Get();
        return st.invSlotSize ? st.invSlotSizeVal : 0;
    }
}
