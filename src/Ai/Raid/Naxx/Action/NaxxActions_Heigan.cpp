#include "NaxxActions.h"

#include "Playerbots.h"

bool HeiganPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    if (helper.IsFastDance())
    {
        // Everyone dances: stand in the upcoming safe section, near its edge toward the
        // following one. The schedule is reconstructed from the teleport timestamp.
        auto [x, y] = helper.DancePosition();
        if (bot->GetExactDist2d(x, y) <= 1.5f)
            return false;

        return MoveTo(NAXX_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT, true);
    }

    // Slow phase: ranged and healers camp Heigan's (currently vacant) platform - no
    // eruption tiles up there and it keeps them out of Spell Disruption range.
    if (botAI->IsRanged(bot))
    {
        if (bot->GetExactDist2d(helper.platformPos.first, helper.platformPos.second) <= 4.0f)
            return false;

        return MoveTo(NAXX_MAP_ID, helper.platformPos.first, helper.platformPos.second, helper.platformZ,
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    // Melee hug the boss's rear instead of riding the edge of his large combat reach,
    // so the player steering him keeps them inside safe ground (fight-specific: other
    // encounters keep the default reach behaviour).
    Unit* boss = helper.GetBoss();
    if (boss && bot->GetVictim() == boss)
    {
        float combined = bot->GetCombatReach() + boss->GetCombatReach();
        float hug = std::max(2.0f, combined - 2.5f);
        if (bot->GetExactDist(boss) > hug + 1.0f)
        {
            float angle = boss->GetOrientation() + M_PI;
            float x = boss->GetPositionX() + cos(angle) * hug * 0.7f;
            float y = boss->GetPositionY() + sin(angle) * hug * 0.7f;
            return MoveTo(NAXX_MAP_ID, x, y, boss->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT, true);
        }
    }
    return false;
}
