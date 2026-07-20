#include "EoETriggers.h"

#include "SharedDefines.h"

uint8 MalygosTrigger::getPhase(Player* bot, Unit* boss)
{
    uint8 phase = 0;
    Unit* vehicle = bot->GetVehicleBase();
    if (bot->GetMapId() != EOE_MAP_ID) { return phase; }

    if (vehicle && vehicle->GetEntry() == NPC_WYRMREST_SKYTALON)
    {
        phase = 3;
    }
    else if (boss && boss->HealthAbovePct(50))
    {
        phase = 1;
    }
    else if (boss)
    {
        phase = 2;
    }

    return phase;
}

Unit* MalygosTrigger::FindNearestAlive(PlayerbotAI* botAI, Player* bot, uint32 entry, float maxRange)
{
    Unit* nearest = nullptr;
    GuidVector targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
            continue;

        if (maxRange > 0.0f && bot->GetDistance(unit) > maxRange)
            continue;

        if (!nearest || bot->GetExactDist(unit) < bot->GetExactDist(nearest))
            nearest = unit;
    }

    return nearest;
}

Unit* MalygosTrigger::FindLooseNexusLord(PlayerbotAI* botAI, Player* bot)
{
    Unit* nearest = nullptr;
    GuidVector targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_NEXUS_LORD)
            continue;

        Unit* victim = unit->GetVictim();
        Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
        if (victimPlayer && PlayerbotAI::IsTank(victimPlayer))
            continue;

        if (!nearest || bot->GetExactDist(unit) < bot->GetExactDist(nearest))
            nearest = unit;
    }

    return nearest;
}

Unit* MalygosTrigger::FindMalygos(Player* bot)
{
    if (bot->GetMapId() != EOE_MAP_ID)
        return nullptr;

    return bot->FindNearestCreature(NPC_MALYGOS, 250.0f);
}

Unit* MalygosTrigger::FindLiveSpark(Player* bot)
{
    std::list<Creature*> sparks;
    bot->GetCreatureListWithEntryInGrid(sparks, NPC_POWER_SPARK, 200.0f);

    Creature* nearest = nullptr;
    for (Creature* spark : sparks)
    {
        // Spent sparks linger as unattackable husks anchoring their buff zone
        if (!spark->IsAlive() || spark->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            continue;

        if (!nearest || bot->GetExactDist(spark) < bot->GetExactDist(nearest))
            nearest = spark;
    }

    return nearest;
}

bool MalygosTrigger::HasSparkGripper(PlayerbotAI* botAI, Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Unit* boss = FindMalygos(bot);
    Unit* tanking = boss ? boss->GetVictim() : nullptr;

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member->IsAlive() && member->IsClass(CLASS_DEATH_KNIGHT) && member != tanking)
            return true;
    }

    return false;
}

Creature* MalygosTrigger::FindSparkBuffZone(Player* bot)
{
    std::list<Creature*> sparks;
    bot->GetCreatureListWithEntryInGrid(sparks, NPC_POWER_SPARK, 150.0f);

    Creature* nearest = nullptr;
    for (Creature* spark : sparks)
    {
        // Spent sparks are flagged unattackable and hold their buff zone for 60s
        if (!spark->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            continue;

        if (!nearest || bot->GetExactDist(spark) < bot->GetExactDist(nearest))
            nearest = spark;
    }

    return nearest;
}

Creature* MalygosTrigger::FindNearestBubble(Player* bot)
{
    std::list<Creature*> bubbles;
    bot->GetCreatureListWithEntryInGrid(bubbles, NPC_ARCANE_OVERLOAD, 150.0f);

    Creature* nearest = nullptr;
    for (Creature* bubble : bubbles)
    {
        if (!nearest || bot->GetExactDist2d(bubble) < bot->GetExactDist2d(nearest))
            nearest = bubble;
    }

    return nearest;
}

bool MalygosTrigger::IsNearBubble(Player* bot, float radius)
{
    std::list<Creature*> bubbles;
    bot->GetCreatureListWithEntryInGrid(bubbles, NPC_ARCANE_OVERLOAD, 150.0f);
    if (bubbles.empty())
        return true;

    for (Creature* bubble : bubbles)
    {
        if (bot->GetExactDist2d(bubble) < radius)
            return true;
    }

    return false;
}

bool MalygosTrigger::IsSurgeOfPowerImminent(Unit* boss)
{
    if (!boss)
        return false;

    if (boss->FindCurrentSpellBySpellId(SPELL_SURGE_OF_POWER))
        return true;

    // In phase 2 Malygos circles ~83yd out and only closes on the platform center
    // to channel Surge of Power; the inbound flight is the ~5s warning. Raw
    // distance is essential: GetDistance2d subtracts his 20yd combat reach and
    // reads the 83yd circle as inside the threshold
    return boss->GetExactDist2d(EOE_CENTER_POSITION.first, EOE_CENTER_POSITION.second) < 70.0f;
}

bool MalygosTrigger::IsActive()
{
    Unit* boss = FindMalygos(bot);
    return boss && boss->IsInCombat();
}

bool PowerSparkTrigger::IsActive()
{
    if (bot->getClass() != CLASS_DEATH_KNIGHT)
    {
        return false;
    }

    Unit* boss = MalygosTrigger::FindMalygos(bot);
    if (!boss || !boss->IsInCombat())
    {
        return false;
    }

    return bool(MalygosTrigger::FindLiveSpark(bot));
}
