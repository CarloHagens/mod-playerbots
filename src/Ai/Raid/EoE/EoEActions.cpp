#include "Playerbots.h"
#include "EoEActions.h"
#include "EoETriggers.h"
#include "ObjectDefines.h"
#include "Vehicle.h"

bool MalygosPositionAction::Execute(Event /*event*/)
{
    Unit* boss = MalygosTrigger::FindMalygos(bot);
    if (!boss) { return false; }

    uint8 phase = MalygosTrigger::getPhase(bot, boss);

    float distance = 5.0f;

    if (phase == 1)
    {
        // No positioning while he is still airborne (intro flight, vortex):
        // flanking a flying boss projects points over the platform edge and the
        // raid walks off chasing them
        if (boss->GetPositionZ() > EOE_PLATFORM_Z + 5.0f)
        {
            return false;
        }

        // Position tanks. Role strategy, not IsMainTank: with a player tanking,
        // group slot order can crown someone else "main tank" and a bot tank
        // misclassified as raid would drag the boss into the stack
        if (botAI->IsTank(bot))
        {
            // Anchor only once Malygos is chasing us - he follows the walk there.
            // Before that, normal combat movement closes on him to build threat;
            // anchoring early just paces between the boss and the empty spot
            if (boss->GetVictim() != bot)
            {
                return false;
            }

            if (bot->GetDistance2d(MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second) > distance)
            {
                return MoveTo(EOE_MAP_ID, MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second, bot->GetPositionZ(),
                    false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            return false;
        }
        // Dps park inside a fresh spark buff zone, but only between sparks - a
        // live spark means everyone belongs on the intercept flank, DK first -
        // and only if the boss can be attacked from the zone AND it sits on his
        // flank, never anywhere his breath can point. Raw distances:
        // reach-inclusive ones are uselessly loose against a 20yd combat reach.
        // Hunters skip it (a usable zone is always inside their dead zone)
        if (botAI->IsDps(bot) && !bot->IsClass(CLASS_HUNTER) &&
            !MalygosTrigger::FindLiveSpark(bot))
        {
            if (Creature* zone = MalygosTrigger::FindSparkBuffZone(bot))
            {
                float zoneDist = zone->GetExactDist2d(boss);
                bool onFlank = !boss->HasInArc(2 * M_PI / 3, zone);
                bool canUseZone = onFlank && zoneDist < (botAI->IsMelee(bot) ? 22.0f : 40.0f);
                if (canUseZone)
                {
                    if (!bot->HasAura(SPELL_POWER_SPARK_GROUND_BUFF))
                    {
                        return MoveTo(EOE_MAP_ID, zone->GetPositionX(), zone->GetPositionY(), zone->GetPositionZ(),
                            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                    }

                    return false;
                }
            }
        }

        // The whole raid stacks on the boss's flank matching the inbound spark
        // (hunters on the same line, deeper, past their dead zone), holding that
        // side until the next spark dictates otherwise. His 20yd combat reach
        // means melee still connect from the stack. Flanks sit perpendicular to
        // his live facing (he faces his victim), so wherever and however he is
        // parked, the stack is never in the breath; the spark then flies straight
        // through it, is chained and killed well outside the absorb radius, and
        // drops its buff zone on the stack
        float orientation = boss->GetOrientation();
        float delta = orientation - _flankAxis;
        while (delta > M_PI)
            delta -= 2 * M_PI;
        while (delta < -M_PI)
            delta += 2 * M_PI;
        if (_flankAxis < -9.0f || std::fabs(delta) > 0.5f)
        {
            _flankAxis = orientation;
        }

        float perpX = std::cos(_flankAxis + M_PI_2);
        float perpY = std::sin(_flankAxis + M_PI_2);

        if (Unit* spark = MalygosTrigger::FindLiveSpark(bot))
        {
            float dot = (spark->GetPositionX() - boss->GetPositionX()) * perpX +
                (spark->GetPositionY() - boss->GetPositionY()) * perpY;
            _flankSign = dot >= 0.0f ? 1.0f : -1.0f;
        }

        float offset = bot->IsClass(CLASS_HUNTER) ? MALYGOS_HUNTER_OFFSET : MALYGOS_STACK_OFFSET;
        float stackX = boss->GetPositionX() + perpX * offset * _flankSign;
        float stackY = boss->GetPositionY() + perpY * offset * _flankSign;

        // Keep the stack on the platform even when he is parked near the rim
        float centerDX = stackX - EOE_CENTER_POSITION.first;
        float centerDY = stackY - EOE_CENTER_POSITION.second;
        float centerDist = std::sqrt(centerDX * centerDX + centerDY * centerDY);
        if (centerDist > 32.0f)
        {
            stackX = EOE_CENTER_POSITION.first + centerDX / centerDist * 32.0f;
            stackY = EOE_CENTER_POSITION.second + centerDY / centerDist * 32.0f;
        }

        if (bot->GetDistance2d(stackX, stackY) > 5.0f)
        {
            return MoveTo(EOE_MAP_ID, stackX, stackY, bot->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }
    else if (phase == 2)
    {
        Unit* vehicleBase = bot->GetVehicleBase();
        if (vehicleBase && vehicleBase->GetEntry() == NPC_HOVER_DISK)
        {
            return FlyDiscToScion(vehicleBase);
        }

        // Deep breath incinerates everything outside a bubble - everyone piles
        // into the freshest one as soon as the boss heads for the center
        if (MalygosTrigger::IsSurgeOfPowerImminent(boss))
        {
            return MoveToBubble(true);
        }

        if (botAI->IsMelee(bot) && botAI->IsDps(bot))
        {
            // Every dead rider frees his disc immediately - first melee to claim
            // it flies off after the Scions; the rest keep brawling lords. Waiting
            // for the last lord is too late: ranged usually finish the Scions first
            if (MalygosTrigger::FindNearestAlive(botAI, bot, NPC_SCION_OF_ETERNITY))
            {
                if (Creature* disc = FindFreeDisc())
                {
                    if (bot->GetExactDist(disc) > INTERACTION_DISTANCE)
                    {
                        return MoveTo(EOE_MAP_ID, disc->GetPositionX(), disc->GetPositionY(), disc->GetPositionZ(),
                            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                    }

                    disc->HandleSpellClick(bot);
                    if (!bot->IsOnVehicle(disc))
                    {
                        // Spellclick can fail quietly (e.g. combat state); board directly
                        bot->EnterVehicle(disc);
                    }

                    return bot->IsOnVehicle(disc);
                }
            }

            // Ground adds, brawled from inside the bubble (the tank drags them
            // in); if positional movement strayed out, step back into the
            // NEAREST cover rather than trekking to the newest across the room
            if (MalygosTrigger::FindNearestAlive(botAI, bot, NPC_NEXUS_LORD))
            {
                if (MalygosTrigger::IsNearBubble(bot, MALYGOS_BRAWL_TETHER))
                {
                    return false;
                }

                if (Creature* bubble = MalygosTrigger::FindNearestBubble(bot))
                {
                    return MoveTo(EOE_MAP_ID, bubble->GetPositionX(), bubble->GetPositionY(), bubble->GetPositionZ(),
                        false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }

                return false;
            }
        }

        // Tanks roam free until every lord is beating on a tank, then bunker up
        if (botAI->IsTank(bot) && MalygosTrigger::FindLooseNexusLord(botAI, bot))
        {
            return false;
        }

        // Healers, ranged, disc-less melee and settled tanks camp the bubble
        return MoveToBubble();
    }

    return false;
}

bool MalygosPositionAction::MoveToBubble(bool preferNewest)
{
    std::list<Creature*> bubbles;
    bot->GetCreatureListWithEntryInGrid(bubbles, NPC_ARCANE_OVERLOAD, 150.0f);
    if (bubbles.empty())
    {
        return false;
    }

    time_t now = time(nullptr);

    // Track bubble ages ourselves (despawn timers are not readable): they live
    // 45s, so ride the current one until its final stretch instead of hopping
    // to every fresh spawn
    Creature* newest = nullptr;
    Creature* standingIn = nullptr;
    for (Creature* bubble : bubbles)
    {
        if (_bubbleFirstSeen.find(bubble->GetGUID()) == _bubbleFirstSeen.end())
        {
            _bubbleFirstSeen[bubble->GetGUID()] = now;
        }

        if (!newest || _bubbleFirstSeen[bubble->GetGUID()] > _bubbleFirstSeen[newest->GetGUID()])
        {
            newest = bubble;
        }

        // Generous radius: bots may drift around inside the bubble for positional
        // combat movement without being yanked back to its exact center
        if (bot->GetExactDist2d(bubble) < MALYGOS_BUBBLE_SLACK)
        {
            standingIn = bubble;
        }
    }

    // Forget bubbles that have despawned
    for (auto itr = _bubbleFirstSeen.begin(); itr != _bubbleFirstSeen.end();)
    {
        bool alive = false;
        for (Creature* bubble : bubbles)
        {
            if (bubble->GetGUID() == itr->first)
            {
                alive = true;
                break;
            }
        }

        itr = alive ? std::next(itr) : _bubbleFirstSeen.erase(itr);
    }

    // Outside a surge, the current bubble stays fine until it nears despawn
    Creature* dest = newest;
    if (standingIn && !preferNewest && now - _bubbleFirstSeen[standingIn->GetGUID()] < 35)
    {
        dest = standingIn;
    }

    if (!botAI->IsTank(bot))
    {
        if (standingIn == dest)
        {
            return false;
        }

        return MoveTo(EOE_MAP_ID, dest->GetPositionX(), dest->GetPositionY(), dest->GetPositionZ(),
            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    // Tanks park at the bubble's far edge (relative to the platform center) so
    // the lords glued to them end up standing in the bubble's middle
    float dirX = dest->GetPositionX() - EOE_CENTER_POSITION.first;
    float dirY = dest->GetPositionY() - EOE_CENTER_POSITION.second;
    float len = std::sqrt(dirX * dirX + dirY * dirY);
    if (len < 1.0f)
    {
        dirX = 1.0f;
        dirY = 0.0f;
        len = 1.0f;
    }

    float edgeX = dest->GetPositionX() + dirX / len * MALYGOS_TANK_EDGE_OFFSET;
    float edgeY = dest->GetPositionY() + dirY / len * MALYGOS_TANK_EDGE_OFFSET;
    if (bot->GetDistance2d(edgeX, edgeY) < 2.5f)
    {
        return false;
    }

    return MoveTo(EOE_MAP_ID, edgeX, edgeY, dest->GetPositionZ(),
        false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

Creature* MalygosPositionAction::FindFreeDisc()
{
    std::list<Creature*> discs;
    bot->GetCreatureListWithEntryInGrid(discs, NPC_HOVER_DISK, 150.0f);

    Creature* nearest = nullptr;
    for (Creature* disc : discs)
    {
        if (!disc->IsAlive())
            continue;

        Vehicle* kit = disc->GetVehicleKit();
        if (!kit)
            continue;

        // A dead rider can linger in the seat; only a living passenger blocks us
        Unit* passenger = kit->GetPassenger(0);
        if (passenger && passenger->IsAlive())
            continue;

        // Discs land when their rider dies; ignore ones still descending far overhead
        if (disc->GetPositionZ() > bot->GetPositionZ() + 10.0f)
            continue;

        if (!nearest || bot->GetExactDist(disc) < bot->GetExactDist(nearest))
            nearest = disc;
    }

    return nearest;
}

bool MalygosPositionAction::FlyDiscToScion(Unit* vehicleBase)
{
    MotionMaster* mm = vehicleBase->GetMotionMaster();
    Unit* scion = MalygosTrigger::FindNearestAlive(botAI, bot, NPC_SCION_OF_ETERNITY);
    if (!scion)
    {
        // Adds are done: get back down before the platform shatters
        if (vehicleBase->GetPositionZ() > EOE_PLATFORM_Z + 3.0f)
        {
            mm->MovePoint(0, vehicleBase->GetPositionX(), vehicleBase->GetPositionY(), EOE_PLATFORM_Z);
            vehicleBase->SendMovementFlagUpdate();
            return true;
        }

        bot->ExitVehicle();
        return true;
    }

    if (vehicleBase->GetExactDist(scion) > 4.0f)
    {
        mm->MovePoint(0, scion->GetPositionX(), scion->GetPositionY(), scion->GetPositionZ());
        vehicleBase->SendMovementFlagUpdate();
        return true;
    }

    vehicleBase->SetFacingToObject(scion);
    mm->MoveIdle();
    vehicleBase->SendMovementFlagUpdate();
    return false;
}

bool MalygosTargetAction::Execute(Event /*event*/)
{
    Unit* boss = MalygosTrigger::FindMalygos(bot);
    if (!boss) { return false; }

    uint8 phase = MalygosTrigger::getPhase(bot, boss);

    if (phase == 1)
    {
        if (botAI->IsHeal(bot)) { return false; }

        // Default to the boss. Sparks are killed once the DK has them chained, so
        // the buff zone drops where the raid stands - nobody snipes them in transit.
        // Ranged free-fire only when no gripper is alive
        Unit* newTarget = boss;
        if (!botAI->IsTank(bot))
        {
            if (Unit* spark = MalygosTrigger::FindLiveSpark(bot))
            {
                bool captured = botAI->HasAura("chains of ice", spark);
                bool attackSpark;
                if (botAI->IsMelee(bot))
                {
                    attackSpark = captured;
                }
                else
                {
                    attackSpark = (captured || !MalygosTrigger::HasSparkGripper(botAI, bot)) &&
                        bot->GetDistance(spark) <= MALYGOS_RANGED_RANGE;
                }

                if (attackSpark)
                {
                    newTarget = spark;
                }
            }
        }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
        {
            return Attack(newTarget);
        }
    }
    else if (phase == 2)
    {
        if (botAI->IsHeal(bot)) { return false; }

        Unit* vehicleBase = bot->GetVehicleBase();
        bool onDisc = vehicleBase && vehicleBase->GetEntry() == NPC_HOVER_DISK;
        bool rangedFromBubble = !onDisc && botAI->IsRangedDps(bot);

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        // Keep a still-valid target to avoid thrashing between adds
        if (currentTarget && currentTarget->IsAlive())
        {
            if (onDisc && currentTarget->GetEntry() == NPC_SCION_OF_ETERNITY)
                return false;

            if (rangedFromBubble && bot->GetDistance(currentTarget) <= MALYGOS_RANGED_RANGE &&
                (currentTarget->GetEntry() == NPC_SCION_OF_ETERNITY ||
                 (currentTarget->GetEntry() == NPC_NEXUS_LORD &&
                  !MalygosTrigger::FindNearestAlive(botAI, bot, NPC_SCION_OF_ETERNITY))))
                return false;

            if (!onDisc && !rangedFromBubble && currentTarget->GetEntry() == NPC_NEXUS_LORD)
            {
                if (!botAI->IsTank(bot))
                    return false;

                // A collecting tank stays on a lord it is still pulling; once it
                // sticks, move on to the next loose one
                Unit* victim = currentTarget->GetVictim();
                Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
                bool stuckOnTank = victimPlayer && PlayerbotAI::IsTank(victimPlayer);
                if (!stuckOnTank || !MalygosTrigger::FindLooseNexusLord(botAI, bot))
                    return false;
            }
        }

        Unit* newTarget = nullptr;
        if (onDisc)
        {
            newTarget = MalygosTrigger::FindNearestAlive(botAI, bot, NPC_SCION_OF_ETERNITY);
        }
        else if (rangedFromBubble)
        {
            // Fliers only, and only when they orbit into range; ground adds are just a
            // fallback once every Scion is dead. Otherwise hold fire in the bubble.
            newTarget = MalygosTrigger::FindNearestAlive(botAI, bot, NPC_SCION_OF_ETERNITY, MALYGOS_RANGED_RANGE);
            if (!newTarget && !MalygosTrigger::FindNearestAlive(botAI, bot, NPC_SCION_OF_ETERNITY))
                newTarget = MalygosTrigger::FindNearestAlive(botAI, bot, NPC_NEXUS_LORD, MALYGOS_RANGED_RANGE);
        }
        else
        {
            // Tanks grab lords not yet glued to a tank first; melee just hit the nearest
            if (botAI->IsTank(bot))
            {
                newTarget = MalygosTrigger::FindLooseNexusLord(botAI, bot);
            }

            if (!newTarget)
            {
                newTarget = MalygosTrigger::FindNearestAlive(botAI, bot, NPC_NEXUS_LORD);
            }
        }

        // Transition windows: no adds up yet, but the boss is still attackable
        // while he lands and lifts off - keep hitting him instead of idling
        if (!newTarget && !boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) &&
            (!rangedFromBubble || bot->GetDistance(boss) <= MALYGOS_RANGED_RANGE))
        {
            newTarget = boss;
        }

        if (newTarget)
        {
            return Attack(newTarget);
        }

        // Nothing in reach: stop attacking instead of trailing after the fliers
        if (currentTarget)
        {
            context->GetValue<Unit*>("current target")->Set(nullptr);
            bot->SetTarget(ObjectGuid::Empty);
            bot->SetSelection(ObjectGuid());
            bot->AttackStop();
        }

        return false;
    }

    return false;
}

bool MalygosGripSparkAction::isUseful()
{
    if (!bot->IsClass(CLASS_DEATH_KNIGHT))
        return false;

    // Anyone currently tanking Malygos is excused from spark duty
    Unit* boss = MalygosTrigger::FindMalygos(bot);
    return !(boss && boss->GetVictim() == bot);
}

bool MalygosGripSparkAction::Execute(Event /*event*/)
{
    Unit* spark = MalygosTrigger::FindLiveSpark(bot);
    if (!spark)
        return false;

    // The raid stacks on the spark's inbound side, so it flies straight through
    // us. Chains has no cooldown - snare on sight, so a grip on cooldown can
    // never let it slip through
    if (!botAI->HasAura("chains of ice", spark) && botAI->CanCastSpell("chains of ice", spark))
    {
        if (botAI->CastSpell("chains of ice", spark))
        {
            return true;
        }
    }

    // Pull it into the stack so it dies (and drops its buff) on the raid. The
    // CanCast guard matters: an uncastable grip must fail through to positioning
    // or this action starves it and the DK freezes in place
    if (bot->GetExactDist(spark) > MALYGOS_SPARK_MELEE_RANGE && botAI->CanCastSpell("death grip", spark))
    {
        return botAI->CastSpell("death grip", spark);
    }

    return false;
}

bool EoEFlyDrakeAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}
bool EoEFlyDrakeAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();
    if (!master) { return false; }
    Unit* masterVehicle = master->GetVehicleBase();
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || !masterVehicle) { return false; }

    MotionMaster* mm = vehicleBase->GetMotionMaster();
    if (vehicleBase->GetExactDist(masterVehicle) > 5.0f)
    {
        uint8 numPlayers;
        bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? numPlayers = 25 : numPlayers = 10;
        // 3/4 of a circle, with frontal cone 90 deg unobstructed
        float angle = botAI->GetGroupSlotIndex(bot) * (2*M_PI - M_PI_2)/numPlayers + M_PI_2;
        vehicleBase->SetCanFly(true);
        mm->MoveFollow(masterVehicle, 3.0f, angle);
        vehicleBase->SendMovementFlagUpdate();
        return true;
    }
    return false;
}

bool EoEDrakeAttackAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool EoEDrakeAttackAction::Execute(Event /*event*/)
{
    vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
    {
        return false;
    }

    Unit* boss = MalygosTrigger::FindMalygos(bot);
    if (!boss)
    {
        return false;
    }

    // Phase 3 wants a fixed healing corps (surge damage is simply healed
    // through): every real healer, topped up to the quota with a stable
    // guid-ordered pick of bot dps when the group runs short of healers
    bool healerDrake = botAI->IsHeal(bot);
    if (!healerDrake)
    {
        uint32 desiredHealers;
        bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? desiredHealers = 6 : desiredHealers = 2;

        Group* group = bot->GetGroup();
        if (group)
        {
            uint32 numHealers = 0;
            std::vector<ObjectGuid> candidates;
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || !member->IsAlive())
                    continue;

                if (PlayerbotAI::IsHeal(member))
                    ++numHealers;
                else if (GET_PLAYERBOT_AI(member))
                    candidates.push_back(member->GetGUID());
            }

            if (numHealers < desiredHealers)
            {
                std::sort(candidates.begin(), candidates.end());
                uint32 needed = desiredHealers - numHealers;
                for (uint32 i = 0; i < needed && i < candidates.size(); ++i)
                {
                    if (candidates[i] == bot->GetGUID())
                    {
                        healerDrake = true;
                        break;
                    }
                }
            }
        }
    }

    if (healerDrake)
    {
        return DrakeHealAction();
    }

    return DrakeDpsAction(boss);
}

bool EoEDrakeAttackAction::CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown)
{
    if (botAI->CanCastVehicleSpell(spellId, target))
        if (botAI->CastVehicleSpell(spellId, target))
        {
            vehicleBase->AddSpellCooldown(spellId, 0, cooldown);
            return true;
        }
    return false;
}

bool EoEDrakeAttackAction::DrakeDpsAction(Unit* target)
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase) { return false; }

    // 1-1-2: two builders, then the Engulf finisher
    uint8 comboPoints = vehicleBase->GetComboPoints(target);
    if (comboPoints >= 2)
    {
        return CastDrakeSpellAction(target, SPELL_ENGULF_IN_FLAMES, 0);
    }

    return CastDrakeSpellAction(target, SPELL_FLAME_SPIKE, 0);
}

bool EoEDrakeAttackAction::DrakeHealAction()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
    {
        return false;
    }

    uint8 comboPoints = vehicleBase->GetComboPoints(vehicleBase);
    if (comboPoints >= 5)
    {
        return CastDrakeSpellAction(vehicleBase, SPELL_LIFE_BURST, 0);
    }
    // Forced cast: CanCastVehicleSpell() returns SPELL_FAILED_BAD_TARGETS when
    // targeting drakes, but the cast itself succeeds
    return botAI->CastVehicleSpell(SPELL_REVIVIFY, vehicleBase);
}
