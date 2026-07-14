#include "OSMultipliers.h"

#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "OSActions.h"
#include "OSTriggers.h"
#include "ReachTargetActions.h"
#include "ScriptedCreature.h"
#include "WarriorActions.h"

float SartharionMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return 1.0f; }

    Unit* target = action->GetTarget();

    if (botAI->IsMainTank(bot) && dynamic_cast<TankFaceAction*>(action))
    {
        // return 0.0f;
    }

    if (botAI->IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
    {
        return 0.0f;
    }

    if (botAI->IsMainTank(bot) && target && target != boss &&
        (dynamic_cast<TankAssistAction*>(action) || dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }

    if (botAI->IsAssistTank(bot) && target && target == boss &&
        (dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }

    // While a flame tsunami is inbound, don't let generic movement drag bots out of the safe
    // lanes, and don't let the anchor actions pull a sidestepping tank back into the wave
    if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
        dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<RearFlankAction*>(action) ||
        dynamic_cast<SartharionRangedPositionAction*>(action) || dynamic_cast<SartharionTankPositionAction*>(action) ||
        dynamic_cast<SartharionMeleePositionAction*>(action))
    {
        GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
        for (auto& npc : npcs)
        {
            Unit* unit = botAI->GetUnit(npc);
            if (unit && unit->GetEntry() == NPC_FLAME_TSUNAMI && IsFlameTsunamiIncoming(unit, bot))
            {
                return 0.0f;
            }
        }
    }
    return 1.0f;
}
