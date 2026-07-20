#include "EoEMultipliers.h"

#include "ChooseTargetActions.h"
#include "EoEActions.h"
#include "EoETriggers.h"
#include "FollowActions.h"
#include "MovementActions.h"

float MalygosMultiplier::GetValue(Action* action)
{
    Unit* boss = MalygosTrigger::FindMalygos(bot);

    uint8 phase = MalygosTrigger::getPhase(bot, boss);
    if (phase == 0) { return 1.0f; }

    if (phase == 1)
    {
        if (dynamic_cast<FollowAction*>(action))
        {
            return 0.0f;
        }

        if (botAI->IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
        {
            return 0.0f;
        }

        if (botAI->IsRangedDps(bot) && dynamic_cast<DropTargetAction*>(action))
        {
            return 0.0f;
        }

        if (!botAI->IsTank(bot) && dynamic_cast<TankAssistAction*>(action))
        {
            return 0.0f;
        }

        // While a tank has aggro, only the anchor drag may move him. His own
        // reach-melee fires at the same boundary that starts the boss chasing,
        // so left enabled the two deadlock at the reach edge and the boss never
        // gets dragged to the center
        if (botAI->IsTank(bot) && boss->GetVictim() == bot &&
            dynamic_cast<MovementAction*>(action) && !dynamic_cast<MalygosPositionAction*>(action) &&
            !dynamic_cast<AttackAction*>(action))
        {
            return 0.0f;
        }

        // Everyone else holds their assigned ring: generic range-juggling cannot
        // cope with the 20yd combat reach (hunters endlessly pace around their
        // dead zone). Targeting, the grip action and movement toward a chained
        // spark stay free
        if (!botAI->IsTank(bot) && dynamic_cast<MovementAction*>(action) &&
            !dynamic_cast<MalygosPositionAction*>(action) &&
            !dynamic_cast<MalygosGripSparkAction*>(action) &&
            !dynamic_cast<AttackAction*>(action))
        {
            Unit* currentTarget = AI_VALUE(Unit*, "current target");
            if (!currentTarget || currentTarget->GetEntry() != NPC_POWER_SPARK)
            {
                return 0.0f;
            }
        }
    }
    else if (phase == 2)
    {
        Unit* vehicleBase = bot->GetVehicleBase();
        if (vehicleBase && vehicleBase->GetEntry() == NPC_HOVER_DISK)
        {
            // Only the disc-driving position action may move. AttackAction derives
            // from MovementAction but merely sets targets, so it stays usable
            if (dynamic_cast<MovementAction*>(action) && !dynamic_cast<MalygosPositionAction*>(action) &&
                !dynamic_cast<AttackAction*>(action))
            {
                return 0.0f;
            }

            if (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
                dynamic_cast<DropTargetAction*>(action))
            {
                return 0.0f;
            }

            return 1.0f;
        }

        if (dynamic_cast<FollowAction*>(action))
        {
            return 0.0f;
        }

        if (botAI->IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
        {
            return 0.0f;
        }

        if (dynamic_cast<FleeAction*>(action))
        {
            return 0.0f;
        }

        if (dynamic_cast<TankAssistAction*>(action))
        {
            Unit* target = action->GetTarget();
            if (target && target->GetEntry() == NPC_SCION_OF_ETERNITY)
            return 0.0f;
        }

        // AttackAction subclasses only set targets (no movement), so they are exempt
        // from the movement lock - without this, target selection dies with it
        if (dynamic_cast<MovementAction*>(action) && !dynamic_cast<MalygosPositionAction*>(action) &&
            !dynamic_cast<AttackAction*>(action))
        {
            // Free combat movement, but only from inside a bubble: melee brawling
            // ground adds and ranged with something inside their cast envelope.
            // Collecting tanks roam anywhere; nobody roams while the deep breath
            // is coming
            bool surgeImminent = MalygosTrigger::IsSurgeOfPowerImminent(boss);
            bool meleeBrawling = botAI->IsMelee(bot) && botAI->IsDps(bot) && !surgeImminent &&
                MalygosTrigger::IsNearBubble(bot, MALYGOS_BRAWL_TETHER) &&
                MalygosTrigger::FindNearestAlive(botAI, bot, NPC_NEXUS_LORD);
            bool collectingLords = botAI->IsTank(bot) && !surgeImminent &&
                MalygosTrigger::FindLooseNexusLord(botAI, bot);
            bool rangedInRange = botAI->IsRangedDps(bot) && !surgeImminent &&
                MalygosTrigger::IsNearBubble(bot, MALYGOS_BUBBLE_SLACK) &&
                (MalygosTrigger::FindNearestAlive(botAI, bot, NPC_SCION_OF_ETERNITY, MALYGOS_RANGED_RANGE) ||
                 MalygosTrigger::FindNearestAlive(botAI, bot, NPC_NEXUS_LORD, MALYGOS_RANGED_RANGE));
            if (!meleeBrawling && !collectingLords && !rangedInRange)
            {
                return 0.0f;
            }
        }
    }
    else if (phase == 3)
    {
        // Suppresses FollowAction as well as some attack-based movements
        if (dynamic_cast<MovementAction*>(action) && !dynamic_cast<EoEFlyDrakeAction*>(action))
        {
            return 0.0f;
        }
    }

    return 1.0f;
}
