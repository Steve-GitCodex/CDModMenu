#include "storage_sizes_page.h"

#include <cstdio>
#include <iterator>

#include "framework.h"
#include "widgets.h"
#include "../core/settings.h"
#include "../core/state.h"
#include "../game/inventory.h"
#include "../game/storage_slots.h"

namespace trinity::gui
{
    namespace
    {
        constexpr int kFallbackSlots = 100;
        char g_desc[State::kMaxStorageSlots][128];
    }

    void RenderStorageSizes()
    {
        using game::Inventory;
        namespace Slots = game::StorageSlots;
        State& st = State::Get();
        ui::Begin();

        if (!Inventory::Ready())
        {
            ui::Option("Loading inventory...",
                       "Open your in-game inventory once and come back here.");
            ui::End();
            return;
        }
        Inventory::Refresh();

        bool changed = ui::Combo("Step", &st.invStepIdx, Slots::kStepNames,
                                 static_cast<int>(std::size(Slots::kStepNames)),
                                 "How much Left/Right changes a slot number. Hold the key for x10.");

        const int n = Inventory::StorageCount();
        for (int s = 0; s < n && s < State::kMaxStorageSlots; ++s)
        {
            int defSlots = 0, maxSlots = 0;
            const bool known = Inventory::StorageSlots(s, &defSlots, &maxSlots);
            const int normal = known && defSlots > 0 ? defSlots : kFallbackSlots;

            State::StorageSlots* e = Slots::FindOrAdd(Inventory::StorageKey(s), normal);
            if (!e) continue;

            if (known)
                snprintf(g_desc[s], sizeof(g_desc[s]), "Normal: %d slots (game max %d). On = use this number for this storage only.",
                         defSlots, maxSlots);
            else
                snprintf(g_desc[s], sizeof(g_desc[s]), "On = use this number for this storage only.");

            if (ui::ToggleInt(Inventory::StorageName(s), &e->on, &e->value, 1, 9999, Slots::Step(), normal, g_desc[s]))
                changed = true;
        }

        if (n == 0)
            ui::Option("No storages found", "Only storages that hold at least one item are listed.");

        if (changed)
            Settings::Save();
        ui::End();
    }
}
