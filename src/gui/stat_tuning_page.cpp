#include "stat_tuning_page.h"

#include <cstdio>

#include "framework.h"
#include "widgets.h"
#include "../core/settings.h"
#include "../core/state.h"
#include "../game/player.h"

namespace cdmm::gui
{
    namespace
    {
        bool RenderStat(const char* name, State::StatTune& t, char (&labels)[3][48])
        {
            snprintf(labels[0], sizeof(labels[0]), "%s Floor %%", name);
            snprintf(labels[1], sizeof(labels[1]), "%s Regen %%/sec", name);
            snprintf(labels[2], sizeof(labels[2]), "  Regen starts below %%##%s", name);

            bool changed = false;
            changed |= ui::ToggleInt(labels[0], &t.floorOn, &t.floorPct, 1, 100, 5, 30,
                                     "Never drops below this % of the bar.");
            changed |= ui::ToggleInt(labels[1], &t.regenOn, &t.regenPct, 1, 100, 1, 10,
                                     "Refills this % of the bar every second, from the start point up to full.");
            if (t.regenOn)
                changed |= ui::IntOption(labels[2], &t.regenStartPct, 1, 100, 5, 30,
                                         "Regen kicks in when the bar drops to this %, runs until full, then waits. 100 = always.");
            return changed;
        }
    }

    void RenderStatTuning()
    {
        State& st = State::Get();
        static char labels[State::kStatTuneCount][3][48];
        static const char* const kNames[State::kStatTuneCount] = { "Health", "Stamina", "Spirit" };

        ui::Begin();
        if (!game::Player::Ready())
            ui::Option("Load into the world first", "These only take effect once your character is loaded.");

        bool changed = ui::ToggleInt("Can't Die", &st.cannotDie, &st.cannotDiePct, 1, 10, 1, 1,
                                     "You still take damage, but health stops at this % instead of 0.");
        changed |= ui::ToggleInt("Health Max %", &st.hpMaxOn, &st.hpMaxPct, 100, 1000, 25, 200,
                                 "Health bar size as % of normal (200 = double). Off restores it. Experimental.");
        for (int i = 0; i < State::kStatTuneCount; ++i)
            changed |= RenderStat(kNames[i], st.statTune[i], labels[i]);

        if (changed)
            Settings::Save();
        ui::End();
    }
}
