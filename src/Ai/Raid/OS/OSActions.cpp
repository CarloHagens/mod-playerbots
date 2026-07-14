#include "OSActions.h"
#include "OSTriggers.h"

#include "Playerbots.h"

bool IsRightFlameTsunami(Unit* tsunami)
{
    // Waves spawn facing their travel direction (east ledge facing ~pi, west ledge facing ~0)
    // and keep that facing while crossing, so orientation identifies the wave direction.
    // The spawn coordinates can't be used for this: they changed with the core's
    // Sartharion rewrite (azerothcore/azerothcore-wotlk#24218) and may change again.
    float orientation = Position::NormalizeOrientation(tsunami->GetOrientation());
    return orientation > M_PI / 2 && orientation < 3 * M_PI / 2;
}

bool IsFlameTsunamiIncoming(Unit* tsunami, Player* bot)
{
    return IsRightFlameTsunami(tsunami) ? bot->GetPositionX() < tsunami->GetPositionX()
                                        : bot->GetPositionX() > tsunami->GetPositionX();
}

// A 1.5x threat lead over every other group member survives a few more heal ticks
static bool HasSecureThreat(Player* bot, Unit* add)
{
    ThreatManager& mgr = add->GetThreatMgr();
    float botThreat = mgr.GetThreat(bot);
    if (botThreat <= 0.0f)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot)
            continue;

        if (mgr.GetThreat(member) > botThreat / 1.5f)
            return false;
    }
    return true;
}

bool SartharionTankPositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return false; }

    Unit* shadron = nullptr;
    Unit* tenebron = nullptr;
    Unit* vesperon = nullptr;

    // Detect incoming drakes before they are on aggro table
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit) { continue; }

        switch (unit->GetEntry())
        {
            case NPC_SHADRON:
                shadron = unit;
                continue;
            case NPC_TENEBRON:
                tenebron = unit;
                continue;
            case NPC_VESPERON:
                vesperon = unit;
                continue;
            default:
                continue;
        }
    }

    Position currentPos = bot->GetPosition();
    // Adjustable, this is the acceptable distance to stack point that will be accepted as "safe"
    float looseDistance = 12.0f;

    if (botAI->IsMainTank(bot))
    {
        if (bot->GetExactDist2d(SARTHARION_MAINTANK_POSITION.first, SARTHARION_MAINTANK_POSITION.second) > looseDistance)
        {
            return MoveTo(OS_MAP_ID, SARTHARION_MAINTANK_POSITION.first, SARTHARION_MAINTANK_POSITION.second, currentPos.GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    // Offtank grab drakes
    else
    {
        float triggerDistance = 100.0f;
        // Prioritise threat before positioning
        if (tenebron && bot->GetExactDist2d(tenebron) < triggerDistance &&
            tenebron->GetTarget() != bot->GetGUID() && AI_VALUE(Unit*, "current target") != tenebron)
        {
            return Attack(tenebron);
        }
        if (shadron && bot->GetExactDist2d(shadron) < triggerDistance &&
            shadron->GetTarget() != bot->GetGUID() && AI_VALUE(Unit*, "current target") != shadron)
        {
            return Attack(shadron);
        }
        if (vesperon && bot->GetExactDist2d(vesperon) < triggerDistance &&
            vesperon->GetTarget() != bot->GetGUID() && AI_VALUE(Unit*, "current target") != vesperon)
        {
            return Attack(vesperon);
        }

        // Pick up hatched whelps and lava blazes that are loose on the raid
        bool tankingAdd = false;
        GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
        for (auto& npc : npcs)
        {
            Unit* unit = botAI->GetUnit(npc);
            if (!unit || (unit->GetEntry() != NPC_TWILIGHT_WHELP && unit->GetEntry() != NPC_LAVA_BLAZE))
                continue;

            Unit* victim = unit->GetVictim();
            if (victim == bot)
            {
                tankingAdd = true;
                continue;
            }
            if (victim && victim->IsPlayer() && !botAI->IsTank(victim->ToPlayer()) &&
                AI_VALUE(Unit*, "current target") != unit)
            {
                return Attack(unit);
            }
        }

        // Stay on a freshly grabbed add until threat is secured: a single hit's lead
        // evaporates with the next heal tick and the add just peels back to the healer
        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        if (currentTarget && currentTarget->IsAlive() &&
            (currentTarget->GetEntry() == NPC_TWILIGHT_WHELP || currentTarget->GetEntry() == NPC_LAVA_BLAZE) &&
            !HasSecureThreat(bot, currentTarget))
        {
            return false;
        }

        bool drakeInCombat = (tenebron && bot->GetExactDist2d(tenebron) < triggerDistance) ||
                                (shadron && bot->GetExactDist2d(shadron) < triggerDistance) ||
                                (vesperon && bot->GetExactDist2d(vesperon) < triggerDistance);
        // Anchor while tanking anything, so grabbed adds get dragged back to the offtank
        // spot instead of being tanked wherever they were caught (boss frontal/tail arcs)
        if ((drakeInCombat || tankingAdd) &&
            bot->GetExactDist2d(SARTHARION_OFFTANK_POSITION.first, SARTHARION_OFFTANK_POSITION.second) > looseDistance)
        {
            return MoveTo(OS_MAP_ID, SARTHARION_OFFTANK_POSITION.first, SARTHARION_OFFTANK_POSITION.second, currentPos.GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}

bool AvoidTwilightFissureAction::Execute(Event /*event*/)
{
    const float radius = 5.0f;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == NPC_TWILIGHT_FISSURE)
        {
            float currentDistance = bot->GetDistance2d(unit);
            if (currentDistance < radius)
                return MoveAway(unit, radius - currentDistance);
        }
    }
    return false;
}

bool AvoidFlameTsunamiAction::Execute(Event /*event*/)
{
    // Adjustable, this is the acceptable distance to stack point that will be accepted as "safe"
    float looseDistance = 4.0f;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == NPC_FLAME_TSUNAMI)
        {
            Position currentPos = bot->GetPosition();

            if (!IsFlameTsunamiIncoming(unit, bot))
                return false;

            if (botAI->IsTank(bot))
            {
                // Adds in tow trail at melee range behind the tank, so a barely-dodged wall
                // still clips them - and walls buff lava blazes. Dodge deeper when tanking adds.
                bool tankingAdds = false;
                for (auto& addGuid : npcs)
                {
                    Unit* add = botAI->GetUnit(addGuid);
                    if (add && add->IsAlive() && add->GetVictim() == bot &&
                        (add->GetEntry() == NPC_TWILIGHT_WHELP || add->GetEntry() == NPC_LAVA_BLAZE))
                    {
                        tankingAdds = true;
                        break;
                    }
                }

                float y = currentPos.GetPositionY();
                if (IsRightFlameTsunami(unit))
                {
                    // The tank anchors already sit inside the right wave gap (Y ~522-542);
                    // only a tank caught away from it (or with adds near the gap edge) moves
                    bool needsMove = tankingAdds ? (y < 527.0f || y > 537.0f) : (y < 523.0f || y > 541.0f);
                    if (needsMove)
                        return MoveTo(OS_MAP_ID, currentPos.GetPositionX(),
                            tankingAdds ? 532.0f : TSUNAMI_RIGHT_SAFE_ALL, currentPos.GetPositionZ(),
                            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }
                else
                {
                    // The main tank always sidesteps north, pivoting the boss away from the
                    // raid; other tanks (anchored in the south left-wave gap) take the nearest
                    // lane, which from their anchor means standing still
                    float safeY;
                    if (botAI->IsMainTank(bot))
                        safeY = TSUNAMI_LEFT_SAFE_TANK;
                    else
                        safeY = fabsf(y - TSUNAMI_LEFT_SAFE_RANGED) < fabsf(y - TSUNAMI_LEFT_SAFE_TANK)
                                    ? TSUNAMI_LEFT_SAFE_RANGED : TSUNAMI_LEFT_SAFE_TANK;
                    // The north lane is entered from its south edge; push past it so trailing
                    // adds clear the wall (the south lane has yards of natural margin)
                    if (tankingAdds && safeY == TSUNAMI_LEFT_SAFE_TANK && y < safeY)
                        safeY += 5.0f;
                    if (bot->GetExactDist2d(currentPos.GetPositionX(), safeY) > looseDistance)
                        return MoveTo(OS_MAP_ID, currentPos.GetPositionX(), safeY, currentPos.GetPositionZ(),
                            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }
                return false;
            }

            if (IsRightFlameTsunami(unit))     // RIGHT WAVE (travelling east to west)
            {
                if (bot->GetExactDist2d(currentPos.GetPositionX(), TSUNAMI_RIGHT_SAFE_ALL) > looseDistance)
                    return MoveTo(OS_MAP_ID, currentPos.GetPositionX(), TSUNAMI_RIGHT_SAFE_ALL, currentPos.GetPositionZ(),
                        false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            else    // LEFT WAVE (travelling west to east)
            {
                if (botAI->IsMelee(bot))
                {
                    if (bot->GetExactDist2d(currentPos.GetPositionX(), TSUNAMI_LEFT_SAFE_MELEE) > looseDistance)
                        return MoveTo(OS_MAP_ID, currentPos.GetPositionX(), TSUNAMI_LEFT_SAFE_MELEE, currentPos.GetPositionZ(),
                            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }
                else    // Ranged/healers
                {
                    if (bot->GetExactDist2d(currentPos.GetPositionX(), TSUNAMI_LEFT_SAFE_RANGED) > looseDistance)
                        return MoveTo(OS_MAP_ID, currentPos.GetPositionX(), TSUNAMI_LEFT_SAFE_RANGED, currentPos.GetPositionZ(),
                            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }
            }
        }
    }
    return false;
}

bool SartharionRangedPositionAction::Execute(Event /*event*/)
{
    // Adjustable, this is the acceptable distance to stack point that will be accepted as "safe"
    float looseDistance = 4.0f;

    // The anchor sits in a left wave gap, which keeps left wave dodges near-zero for ranged
    // and makes every right wave dodge the same short, predictable move
    if (bot->GetExactDist2d(SARTHARION_RANGED_POSITION.first, SARTHARION_RANGED_POSITION.second) > looseDistance)
    {
        return MoveTo(OS_MAP_ID, SARTHARION_RANGED_POSITION.first, SARTHARION_RANGED_POSITION.second, bot->GetPositionZ(),
            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

bool SartharionMeleePositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return false; }

    // True side flank from the boss's CURRENT facing: his body parks at an angle from
    // the pull approach and only slowly settles, so a fixed offset drifts into the
    // frontal or tail arcs. The perpendicular tracks it exactly; take the south side,
    // toward the dodge cluster and the offtank's adds for quick target switches.
    float flank = boss->GetOrientation() - static_cast<float>(M_PI) / 2.0f;
    float targetX = boss->GetPositionX() + 11.0f * std::cos(flank);
    float targetY = boss->GetPositionY() + 11.0f * std::sin(flank);
    if (targetY > boss->GetPositionY())
    {
        targetX = boss->GetPositionX() - 11.0f * std::cos(flank);
        targetY = boss->GetPositionY() - 11.0f * std::sin(flank);
    }

    if (bot->GetExactDist2d(targetX, targetY) > 5.0f)
    {
        return MoveTo(OS_MAP_ID, targetX, targetY, bot->GetPositionZ(),
            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

bool SartharionAttackPriorityAction::Execute(Event /*event*/)
{
    Unit* sartharion = AI_VALUE2(Unit*, "find target", "sartharion");
    Unit* shadron = AI_VALUE2(Unit*, "find target", "shadron");
    Unit* tenebron = AI_VALUE2(Unit*, "find target", "tenebron");
    Unit* vesperon = AI_VALUE2(Unit*, "find target", "vesperon");
    Unit* acolyte = AI_VALUE2(Unit*, "find target", "acolyte of shadron");
    Unit* whelp = AI_VALUE2(Unit*, "find target", "twilight whelp");
    Unit* blaze = AI_VALUE2(Unit*, "find target", "lava blaze");

    Unit* target = nullptr;

    if (acolyte)
        target = acolyte;
    else if (whelp)
        target = whelp;
    else if (vesperon)
        target = vesperon;
    else if (tenebron)
        target = tenebron;
    else if (shadron)
        target = shadron;
    else if (blaze)
        target = blaze;
    else if (sartharion)
        target = sartharion;

    if (target && AI_VALUE(Unit*, "current target") != target)
        return Attack(target);

    return false;
}

bool EnterTwilightPortalAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss || !boss->HasAura(SPELL_GIFT_OF_TWILIGHT_FIRE)) { return false; }

    GameObject* portal = bot->FindNearestGameObject(GO_TWILIGHT_PORTAL, 100.0f);
    if (!portal) { return false; }

    if (!portal->IsAtInteractDistance(bot))
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f));

    // Go through portal
    WorldPacket data1(CMSG_GAMEOBJ_USE);
    data1 << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data1);

    return true;
}

bool ExitTwilightPortalAction::Execute(Event /*event*/)
{
    GameObject* portal = bot->FindNearestGameObject(GO_NORMAL_PORTAL, 100.0f);
    if (!portal)
        return false;

    if (!portal->IsAtInteractDistance(bot))
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f));

    // Go through portal
    WorldPacket data1(CMSG_GAMEOBJ_USE);
    data1 << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data1);

    return true;
}
