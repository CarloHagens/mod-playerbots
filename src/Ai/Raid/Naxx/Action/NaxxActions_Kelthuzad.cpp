#include "NaxxActions.h"

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

bool KelthuzadChooseTargetAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    helper.UpdatePetSafety();

    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    Unit* target = nullptr;
    Unit *target_soldier = nullptr, *target_weaver = nullptr, *target_abomination = nullptr, *target_kelthuzad = nullptr,
         *target_guardian = nullptr;
    for (auto i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit)
            continue;

        if (botAI->EqualLowercaseName(unit->GetName(), "guardian of icecrown"))
        {
            if (!target_guardian)
                target_guardian = unit;
            else if (unit->GetVictim() && target_guardian->GetVictim() && unit->GetVictim()->ToPlayer() &&
                     target_guardian->GetVictim()->ToPlayer() && !botAI->IsAssistTank(unit->GetVictim()->ToPlayer()) &&
                     botAI->IsAssistTank(target_guardian->GetVictim()->ToPlayer()))
            {
                target_guardian = unit;
            }
            else if (unit->GetVictim() && target_guardian->GetVictim() && unit->GetVictim()->ToPlayer() &&
                     target_guardian->GetVictim()->ToPlayer() && !botAI->IsAssistTank(unit->GetVictim()->ToPlayer()) &&
                     !botAI->IsAssistTank(target_guardian->GetVictim()->ToPlayer()) &&
                     target_guardian->GetDistance2d(helper.center.first, helper.center.second) >
                         bot->GetDistance2d(unit))
            {
                target_guardian = unit;
            }
        }

        if (unit->GetDistance2d(helper.center.first, helper.center.second) > 30.0f)
            continue;

        if (bot->GetDistance2d(unit) > sPlayerbotAIConfig.spellDistance)
            continue;

        if (botAI->EqualLowercaseName(unit->GetName(), "unstoppable abomination"))
        {
            if (target_abomination == nullptr ||
                target_abomination->GetDistance2d(helper.center.first, helper.center.second) >
                    unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_abomination = unit;
            }
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "soldier of the frozen wastes"))
        {
            if (target_soldier == nullptr ||
                target_soldier->GetDistance2d(helper.center.first, helper.center.second) >
                    unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_soldier = unit;
            }
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "soul weaver"))
        {
            if (target_weaver == nullptr || target_weaver->GetDistance2d(helper.center.first, helper.center.second) >
                                                unit->GetDistance2d(helper.center.first, helper.center.second))
                target_weaver = unit;
        }

        if (botAI->EqualLowercaseName(unit->GetName(), "kel'thuzad"))
            target_kelthuzad = unit;
    }
    std::vector<Unit*> targets;
    if (botAI->IsRanged(bot))
    {
        if (botAI->GetRangedDpsIndex(bot) <= 1)
            targets = {target_soldier, target_weaver, target_abomination, target_kelthuzad};
        else
            targets = {target_weaver, target_soldier, target_abomination, target_kelthuzad};
    }
    else if (botAI->IsAssistTank(bot))
        targets = {target_abomination, target_guardian, target_kelthuzad};
    else
        targets = {target_abomination, target_kelthuzad};

    for (Unit* t : targets)
    {
        if (t)
        {
            target = t;
            break;
        }
    }
    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;

    if (target_kelthuzad && target == target_kelthuzad)
        return Attack(target, true);

    return Attack(target, false);
}

bool KelthuzadCcCharmedPlayerAction::IsCrowdControlled(Unit* target) const
{
    if (target->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING) ||
        target->IsPolymorphed())
    {
        return true;
    }
    constexpr uint64 ccMechanicMask =
        (1ull << MECHANIC_STUN) | (1ull << MECHANIC_FEAR) | (1ull << MECHANIC_DISORIENTED) |
        (1ull << MECHANIC_POLYMORPH) | (1ull << MECHANIC_SLEEP) | (1ull << MECHANIC_KNOCKOUT) |
        (1ull << MECHANIC_BANISH) | (1ull << MECHANIC_HORROR) | (1ull << MECHANIC_SHACKLE) |
        (1ull << MECHANIC_FREEZE) | (1ull << MECHANIC_SAPPED);
    return target->HasAuraWithMechanic(ccMechanicMask);
}

// A CC cast already on its way to the target counts as CC too, else every mage in the
// raid polymorphs the same charmed player on the same tick.
bool KelthuzadCcCharmedPlayerAction::IsCcIncoming(Player* target) const
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->IsCharmed())
            continue;

        Spell* spell = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (spell && spell->m_targets.GetUnitTargetGUID() == target->GetGUID())
            return true;
    }
    return false;
}

bool KelthuzadCcCharmedPlayerAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI() || !helper.IsPhaseTwo() || !bot->IsInCombat())
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Per-class candidates, best first. CanCastSpell filters unknown spells, cooldowns,
    // range and line of sight, so missing talents simply fall through. Psychic Scream is
    // the priests' only CC that works on players — untargeted PBAoE, handled below.
    static const std::unordered_map<uint8, std::vector<std::string>> ccSpells = {
        {CLASS_MAGE, {"polymorph"}},
        {CLASS_DRUID, {"cyclone", "bash"}},
        {CLASS_SHAMAN, {"hex"}},
        {CLASS_PALADIN, {"repentance", "hammer of justice"}},
        {CLASS_ROGUE, {"blind", "gouge"}},
        {CLASS_HUNTER, {"scatter shot", "wyvern sting"}},
        {CLASS_WARLOCK, {"fear"}},
        // Intimidating Shout: the shouted target cowers in place; the splash fear on
        // nearby enemies is safe here (Kel'Thuzad is fear-immune, guardians are tanked
        // well outside its 8yd radius).
        {CLASS_WARRIOR, {"concussion blow", "intimidating shout"}},
        // Chains of Ice is only a hard snare, not full CC, so it deliberately does not
        // register in IsCrowdControlled — a chained player still gets properly CC'd by
        // someone else.
        {CLASS_DEATH_KNIGHT, {"strangulate", "chains of ice"}},
        {CLASS_PRIEST, {"psychic scream"}},
    };
    auto it = ccSpells.find(bot->getClass());
    if (it == ccSpells.end())
        return false;

    bool melee = !botAI->IsRanged(bot);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->IsCharmed() ||
            member->GetMapId() != bot->GetMapId())
        {
            continue;
        }
        if (melee ? !bot->IsWithinMeleeRange(member) : bot->GetDistance(member) > sPlayerbotAIConfig.spellDistance)
            continue;

        if (IsCrowdControlled(member) || IsCcIncoming(member))
            continue;

        for (std::string const& spell : it->second)
        {
            // Psychic Scream is centered on the caster, not the target: it only counts
            // when the charmed player is standing inside its 8yd radius.
            if (spell == "psychic scream")
            {
                if (bot->GetDistance(member) <= 8.0f && botAI->CanCastSpell(spell, bot) &&
                    botAI->CastSpell(spell, bot))
                {
                    return true;
                }
                continue;
            }
            if (botAI->CanCastSpell(spell, member) && botAI->CastSpell(spell, member))
                return true;
        }
    }
    return false;
}

bool KelthuzadPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    if (helper.IsPhaseOne())
    {
        if (AI_VALUE(Unit*, "current target") == nullptr)
            return MoveInside(NAXX_MAP_ID, helper.center.first, helper.center.second, bot->GetPositionZ(), 3.0f,
                              MovementPriority::MOVEMENT_COMBAT);
    }
    else if (helper.IsPhaseTwo())
    {
        Unit* shadow_fissure = helper.GetAnyShadowFissure();
        if (!shadow_fissure || !bot->IsWithinDistInMap(shadow_fissure, 10.0f))
        {
            float distance, angle;
            if (botAI->IsMainTank(bot))
            {
                if (AI_VALUE2(bool, "has aggro", "current target"))
                    return MoveTo(NAXX_MAP_ID, helper.tank_pos.first, helper.tank_pos.second, bot->GetPositionZ(), false, false, false,
                                  false, MovementPriority::MOVEMENT_COMBAT);
                else
                    return false;
            }
            else if (botAI->IsRanged(bot))
            {
                uint32 index = botAI->GetRangedIndex(bot);
                if (index < 8)
                {
                    distance = 20.0f;
                    angle = index * M_PI / 4;
                }
                else
                {
                    distance = 32.0f;
                    angle = (index - 8) * M_PI / 4;
                }
                float dx, dy;
                dx = helper.center.first + cos(angle) * distance;
                dy = helper.center.second + sin(angle) * distance;
                return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            else
            {
                // Off-tanks: picking up the Guardians of Icecrown stays their job — keep
                // the scripted guardian camp whenever any guardian is up, and only join
                // the melee camps before the adds spawn.
                if (botAI->IsTank(bot))
                {
                    Unit* cur_tar = AI_VALUE(Unit*, "current target");
                    if (cur_tar && cur_tar->GetVictim() && cur_tar->GetVictim()->ToPlayer() &&
                        botAI->EqualLowercaseName(cur_tar->GetName(), "guardian of icecrown") &&
                        botAI->IsAssistTank(cur_tar->GetVictim()->ToPlayer()))
                    {
                        return MoveTo(NAXX_MAP_ID, helper.assist_tank_pos.first, helper.assist_tank_pos.second,
                                      bot->GetPositionZ(), false, false, false, false,
                                      MovementPriority::MOVEMENT_COMBAT);
                    }
                    Unit* guardian = AI_VALUE2(Unit*, "find target", "guardian of icecrown");
                    if (guardian && guardian->IsAlive())
                        return false;
                }
                // Melee split into two camps at max melee range, 120° to either side of
                // the main tank's direction. Kel'Thuzad's combat reach is 10yd, so the
                // camps sit ~11yd from his center and ~19yd from each other and from the
                // tank — outside Frost Blast's ~10yd chain radius, one camp per blast.
                Unit* boss = helper.GetBoss();
                if (!boss)
                    return false;

                float radius = std::max(bot->GetMeleeRange(boss) - 1.5f, 5.0f);
                float tank_angle = boss->GetAngle(helper.tank_pos.first, helper.tank_pos.second);
                float camp_angle =
                    tank_angle + (botAI->GetMeleeIndex(bot) % 2 == 0 ? 1.0f : -1.0f) * 2.0f * M_PI / 3;
                float dx = boss->GetPositionX() + cos(camp_angle) * radius;
                float dy = boss->GetPositionY() + sin(camp_angle) * radius;
                if (bot->GetDistance2d(dx, dy) < 2.0f)
                    return false;

                return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT);
            }
        }
        else
        {
            float dx, dy;
            float angle;
            if (!botAI->IsRanged(bot))
                angle = shadow_fissure->GetAngle(helper.center.first, helper.center.second);
            else
                angle = bot->GetAngle(shadow_fissure) + M_PI;

            dx = shadow_fissure->GetPositionX() + cos(angle) * 10.0f;
            dy = shadow_fissure->GetPositionY() + sin(angle) * 10.0f;
            return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}
