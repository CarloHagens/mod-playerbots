#ifndef PLAYERBOTS_NAXXBOSSHELPER_H
#define PLAYERBOTS_NAXXBOSSHELPER_H

#include <string>
#include <unordered_map>

#include "AiObject.h"
#include "AiObjectContext.h"
#include "EventMap.h"
#include "Log.h"
#include "NamedObjectContext.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "Timer.h"
#include "NaxxSpellIds.h"

const uint32 NAXX_MAP_ID = 533;

template <class BossAiType>
class GenericBossHelper : public AiObject
{
public:
    GenericBossHelper(PlayerbotAI* botAI, std::string name) : AiObject(botAI), _name(name) {}
    virtual bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            _unit = nullptr;

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            _unit = nullptr;

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", _name);
            if (!_unit)
                return false;

            _target = _unit->ToCreature();
            if (!_target)
                return false;

            _ai = dynamic_cast<BossAiType*>(_target->GetAI());
            if (!_ai)
                return false;

            _event_map = &_ai->events;
            if (!_event_map)
                return false;
        }
        if (!_event_map)
            return false;

        _timer = getMSTime();
        return true;
    }
    virtual void Reset()
    {
        _unit = nullptr;
        _target = nullptr;
        _ai = nullptr;
        _event_map = nullptr;
        _timer = 0;
    }

protected:
    std::string _name;
    Unit* _unit = nullptr;
    Creature* _target = nullptr;
    BossAiType* _ai = nullptr;
    EventMap* _event_map = nullptr;
    uint32 _timer = 0;
};

class KelthuzadBossHelper : public AiObject
{
public:
    KelthuzadBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    const std::pair<float, float> center = {3716.19f, -5106.58f};
    const std::pair<float, float> tank_pos = {3709.19f, -5104.86f};
    const std::pair<float, float> assist_tank_pos = {3746.05f, -5112.74f};
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "kel'thuzad");

        return _unit != nullptr;
    }
    bool IsPhaseOne() { return _unit && _unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }
    bool IsPhaseTwo() { return _unit && !_unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }
    Unit* GetAnyShadowFissure()
    {
        Unit* shadow_fissure = nullptr;
        GuidVector units = *context->GetValue<GuidVector>("nearest triggers");
        for (auto i = units.begin(); i != units.end(); i++)
        {
            Unit* unit = botAI->GetUnit(*i);
            if (!unit)
                continue;
            if (botAI->EqualLowercaseName(unit->GetName(), "shadow fissure"))
                shadow_fissure = unit;
        }
        return shadow_fissure;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class RazuviousBossHelper : public AiObject
{
public:
    RazuviousBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "instructor razuvious");

        return _unit != nullptr;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class SapphironBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos = {3512.07f, -5274.06f};
    const std::pair<float, float> center = {3517.31f, -5253.74f};
    const float GENERIC_HEIGHT = 137.29f;
    SapphironBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "sapphiron");
            if (!_unit)
                return false;
        }
        // Landing state must be shared: every trigger/action/multiplier owns its own helper
        // instance, and the ground action never runs during the air phase, so a per-instance
        // _was_flying flag can never observe the flying->ground transition.
        LandingState& state = _landingStates[_unit->GetGUID()];
        bool now_flying = _unit->IsFlying();
        if (!_unit->IsInCombat())
        {
            state = {};
        }
        else
        {
            if (!state.inCombat)
            {
                state.inCombat = true;
                // Sapphiron starts grounded; treat the pull as a landing so the raid
                // spreads on the opening ground phase too.
                state.lastLandMs = getMSTime();
            }
            if (state.wasFlying && !now_flying)
                state.lastLandMs = getMSTime();
        }
        state.wasFlying = now_flying;
        return true;
    }
    bool IsPhaseGround() { return _unit && !_unit->IsFlying(); }
    bool IsPhaseFlight() { return _unit && _unit->IsFlying(); }
    bool JustLanded()
    {
        if (!_unit)
            return false;

        auto it = _landingStates.find(_unit->GetGUID());
        if (it == _landingStates.end() || !it->second.lastLandMs)
            return false;

        return getMSTime() - it->second.lastLandMs <= POSITION_TIME_AFTER_LANDED;
    }
    bool WaitForExplosion()
    {
        if (!IsPhaseFlight())
            return false;

        Group* group = bot->GetGroup();
        if (!group)
            return false;

        uint32 iced = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member &&
                (NaxxSpellIds::HasAnyAura(member, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
                 botAI->HasAura("icebolt", member, false, false, -1, true)))
            {
                ++iced;
            }
        }

        // Only hide once the LAST ice bolt is out (2 bolts on 10-man, 3 on 25-man; iced
        // players keep the aura until landing). Converging on the first block just herds
        // the raid into the later bolts, and the frost missile only comes 1s after the
        // final bolt with the explosion 8.5s later — there is ample time to hide then.
        uint32 const totalBolts = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? 3 : 2;
        return iced >= totalBolts;
    }
    bool FindPosToAvoidChill(std::vector<float>& dest)
    {
        Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::Chill10, NaxxSpellIds::Chill25});
        if (!aura)
        {
            // Fallback to name for custom spell data.
            aura = botAI->GetAura("chill", bot);
        }
        if (!aura)
            return false;

        DynamicObject* dyn_obj = aura->GetDynobjOwner();
        if (!dyn_obj)
            return false;

        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        float angle = 0;
        uint32 index = botAI->GetGroupSlotIndex(bot);
        if (currentTarget)
        {
            if (botAI->IsRanged(bot))
            {
                if (bot->GetExactDist2d(currentTarget) <= 45.0f)
                    angle = bot->GetAngle(dyn_obj) - M_PI + (rand_norm() - 0.5) * M_PI / 2;
                else
                {
                    if (index % 2 == 0)
                        angle = bot->GetAngle(currentTarget) + M_PI / 2;
                    else
                        angle = bot->GetAngle(currentTarget) - M_PI / 2;
                }
            }
            else
            {
                if (index % 3 == 0)
                    angle = bot->GetAngle(currentTarget);
                else if (index % 3 == 1)
                    angle = bot->GetAngle(currentTarget) + M_PI / 2;
                else
                    angle = bot->GetAngle(currentTarget) - M_PI / 2;
            }
        }
        else
            angle = bot->GetAngle(dyn_obj) - M_PI + (rand_norm() - 0.5) * M_PI / 2;

        dest = {bot->GetPositionX() + cos(angle) * 5.0f, bot->GetPositionY() + sin(angle) * 5.0f, bot->GetPositionZ()};
        return true;
    }

private:
    struct LandingState
    {
        bool inCombat = false;
        bool wasFlying = false;
        uint32 lastLandMs = 0;
    };

    void Reset()
    {
        _unit = nullptr;
    }

    const uint32 POSITION_TIME_AFTER_LANDED = 5000;
    Unit* _unit = nullptr;
    // Shared across all SapphironBossHelper instances (per boss guid) — see UpdateBossAI.
    inline static std::unordered_map<ObjectGuid, LandingState> _landingStates;
};

class GluthBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos25 = {3331.48f, -3109.06f};
    const std::pair<float, float> mainTankPos10 = {3278.29f, -3162.06f};
    const std::pair<float, float> beforeDecimatePos = {3267.34f, -3175.68f};
    const std::pair<float, float> leftSlowDownPos = {3290.68f, -3141.65f};
    const std::pair<float, float> rightSlowDownPos = {3300.78f, -3151.98f};
    const std::pair<float, float> rangedPos = {3301.45f, -3139.29f};
    const std::pair<float, float> healPos = {3303.09f, -3135.24f};

    const float decimatedZombiePct = 10.0f;
    GluthBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "gluth");
            if (!_unit)
                return false;
        }
        if (_unit->IsInCombat())
        {
            if (_combat_start_ms == 0)
                _combat_start_ms = getMSTime();
        }
        else
            _combat_start_ms = 0;

        return true;
    }
    bool BeforeDecimate()
    {
        if (!_unit || !_unit->HasUnitState(UNIT_STATE_CASTING))
            return false;

        Spell* spell = _unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = _unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

        if (!spell)
            return false;

        SpellInfo const* info = spell->GetSpellInfo();
        if (!info)
            return false;

        if (NaxxSpellIds::MatchesAnySpellId(
                info, {NaxxSpellIds::Decimate10, NaxxSpellIds::Decimate25, NaxxSpellIds::Decimate25Alt}))
            return true;

        // Fallback to name for custom spell data.
        return info->SpellName[LOCALE_enUS] && botAI->EqualLowercaseName(info->SpellName[LOCALE_enUS], "decimate");
    }
    bool JustStartCombat() const { return _combat_start_ms != 0 && getMSTime() - _combat_start_ms < 10000; }
    bool IsZombieChow(Unit* unit) const { return unit && botAI->EqualLowercaseName(unit->GetName(), "zombie chow"); }

private:
    void Reset()
    {
        _unit = nullptr;
        _combat_start_ms = 0;
    }

    Unit* _unit = nullptr;
    uint32 _combat_start_ms = 0;
};

class LoathebBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos = {2877.57f, -3967.00f};
    const std::pair<float, float> rangePos = {2896.96f, -3980.61f};
    LoathebBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "loatheb");

        return _unit != nullptr;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class HeiganBossHelper : public AiObject
{
public:
    HeiganBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    // Heigan's own platform (safe camp for ranged in the slow phase; he teleports onto it
    // for the fast dance and fills it with Plague Cloud).
    const std::pair<float, float> platformPos = {2794.26f, -3706.67f};
    const float platformZ = 276.54f;
    // Mid-slice floor waypoints for the core's eruption sections 0..3. Derived from the
    // original dance waypoints, classified against GetEruptionSection geometry
    // (instance_naxxramas.cpp: HeiganPos {2796,-3707} + slopes).
    const std::pair<float, float> sectionPos[4] = {
        {2755.99f, -3703.96f},  // section 0
        {2762.30f, -3684.59f},  // section 1
        {2775.49f, -3674.43f},  // section 2
        {2794.88f, -3668.12f},  // section 3
    };

    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "heigan the unclean");
            if (!_unit)
                return false;
        }
        // Dance state is shared across all helper instances (see SapphironBossHelper).
        DanceState& state = _danceStates[_unit->GetGUID()];
        if (!_unit->IsInCombat())
        {
            state = {};
        }
        else
        {
            bool onPlatform = _unit->IsWithinDist2d(platformPos.first, platformPos.second, 10.0f);
            // Heigan idles ON his platform before the pull, so being up there is not enough:
            // only a ground->platform transition observed during combat is a dance teleport.
            if (!onPlatform)
                state.seenOffPlatform = true;

            if (onPlatform && !state.wasOnPlatform && state.seenOffPlatform)
                state.fastStartMs = getMSTime();

            state.wasOnPlatform = onPlatform;
        }
        return true;
    }
    // Timing from boss_heigan.cpp StartFightPhase: the fast dance teleports at t=0, first
    // eruption at 7s, repeating every 4s, phase lasts 45s. Deterministic, no RNG.
    bool IsFastDance()
    {
        if (!_unit)
            return false;

        auto it = _danceStates.find(_unit->GetGUID());
        if (it == _danceStates.end() || !it->second.fastStartMs)
            return false;

        return getMSTime() - it->second.fastStartMs <= FAST_DANCE_DURATION;
    }
    Unit* GetBoss() { return _unit; }
    // Safe sections ping-pong 3,2,1,0,1,2,... (each phase resets to 3 and moves down first).
    std::pair<float, float> DancePosition()
    {
        uint32 elapsed = getMSTime() - _danceStates[_unit->GetGUID()].fastStartMs;
        // Index of the NEXT eruption: that is the section to stand in right now.
        uint32 k = elapsed < FAST_FIRST_ERUPTION ? 0 : (elapsed - FAST_FIRST_ERUPTION) / FAST_ERUPTION_INTERVAL + 1;
        uint8 cur = PingPongSection(k);
        uint8 next = PingPongSection(k + 1);
        // Lean toward the following section to shorten the next walk, but verify the
        // candidate still classifies inside the safe wedge (the boundaries are angular,
        // and boundary tiles splash across) — fall back toward mid-section if not.
        for (float bias : {0.20f, 0.10f, 0.0f})
        {
            float x = sectionPos[cur].first + (sectionPos[next].first - sectionPos[cur].first) * bias;
            float y = sectionPos[cur].second + (sectionPos[next].second - sectionPos[cur].second) * bias;
            if (SectionOf(x, y) == cur)
                return {x, y};
        }
        return sectionPos[cur];
    }

private:
    static uint8 PingPongSection(uint32 k)
    {
        static uint8 const seq[6] = {3, 2, 1, 0, 1, 2};
        return seq[k % 6];
    }

    // Mirror of GetEruptionSection in instance_naxxramas.cpp (HeiganPos {2796,-3707}).
    static uint8 SectionOf(float x, float y)
    {
        constexpr float HX = 2796.0f;
        constexpr float HY = -3707.0f;
        float dy = y - HY;
        if (dy < 1.0f)
            return 0;

        float dx = x - HX;
        if (dx > -1.0f)
            return 3;

        constexpr float slopes[3] = {(-3685.0f - HY) / (2724.0f - HX), (-3647.0f - HY) / (2749.0f - HX),
                                     (-3637.0f - HY) / (2771.0f - HX)};
        float slope = dy / dx;
        for (uint8 i = 0; i < 3; ++i)
        {
            if (slope > slopes[i])
                return i;
        }
        return 3;
    }

    struct DanceState
    {
        bool wasOnPlatform = false;
        bool seenOffPlatform = false;
        uint32 fastStartMs = 0;
    };

    void Reset() { _unit = nullptr; }

    static constexpr uint32 FAST_FIRST_ERUPTION = 7000;
    static constexpr uint32 FAST_ERUPTION_INTERVAL = 4000;
    static constexpr uint32 FAST_DANCE_DURATION = 45000;

    Unit* _unit = nullptr;
    inline static std::unordered_map<ObjectGuid, DanceState> _danceStates;
};

class GothikBossHelper : public AiObject
{
public:
    GothikBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "gothik the harvester");

        return _unit != nullptr;
    }
    // Balcony phase: rooted up top (UNIT_FLAG_DISABLE_MOVE is set on engage and removed when
    // he descends, see boss_gothik.cpp) but NOT immune to players — without special handling
    // bots tunnel the boss instead of killing the waves.
    bool IsBalconyPhase() { return _unit && _unit->HasUnitFlag(UNIT_FLAG_DISABLE_MOVE); }
    Unit* GetBoss() { return _unit; }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class FourHorsemenBossHelper : public AiObject
{
public:
    const float posZ = 241.27f;
    const std::pair<float, float> attractPos[2] = {{2502.03f, -2910.90f},
                                                   {2484.61f, -2947.07f}};  // left (sir zeliek), right (lady blaumeux)
    FourHorsemenBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        else if (_combat_start_ms == 0)
            _combat_start_ms = getMSTime();

        if (_sir && (!_sir->IsInWorld() || !_sir->IsAlive()))
            Reset();

        if (!_sir)
        {
            _sir = AI_VALUE2(Unit*, "find target", "sir zeliek");
            if (!_sir)
                return false;
        }
        _lady = AI_VALUE2(Unit*, "find target", "lady blaumeux");
        return true;
    }
    void Reset()
    {
        _sir = nullptr;
        _lady = nullptr;
        _combat_start_ms = 0;
        posToGo = 0;
    }
    bool IsAttracter(Player* bot)
    {
        Difficulty diff = bot->GetRaidDifficulty();
        if (diff == RAID_DIFFICULTY_25MAN_NORMAL)
        {
            return botAI->IsAssistRangedDpsOfIndex(bot, 0) || botAI->IsAssistHealOfIndex(bot, 0) ||
                   botAI->IsAssistHealOfIndex(bot, 1) || botAI->IsAssistHealOfIndex(bot, 2);
        }
        return botAI->IsAssistRangedDpsOfIndex(bot, 0) || botAI->IsAssistHealOfIndex(bot, 0);
    }
    void CalculatePosToGo(Player* bot)
    {
        bool raid25 = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL;
        Unit* lady = _lady;
        if (!lady)
            posToGo = 0;
        else
        {
            uint32 elapsed_ms = _combat_start_ms ? getMSTime() - _combat_start_ms : 0;
            // Interval: 24s - 15s - 15s - ...
            posToGo = !(elapsed_ms <= 9000 || ((elapsed_ms - 9000) / 67500) % 2 == 0);
            if (botAI->IsAssistRangedDpsOfIndex(bot, 0) || (raid25 && botAI->IsAssistHealOfIndex(bot, 1)))
                posToGo = 1 - posToGo;
        }
    }
    std::pair<float, float> CurrentAttractPos()
    {
        bool raid25 = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL;
        float posX = attractPos[posToGo].first, posY = attractPos[posToGo].second;
        if (posToGo == 1)
        {
            float offset_x = 0.0f;
            float offset_y = 0.0f;
            float bias = 4.5f;
            if (raid25)
            {
                offset_x = -bias;
                offset_y = bias;
            }
            posX += offset_x;
            posY += offset_y;
        }
        return AvoidVoidZones(posX, posY);
    }
    Unit* CurrentAttackTarget()
    {
        if (posToGo == 0)
            return _sir;

        return _lady;
    }
    // Blaumeux drops Void Zones on her target — the attractor parked on the fixed corner
    // coordinate. Generic avoid-aoe gets overridden by the scripted corner move every tick,
    // so the corner itself must shift off any zone.
    std::pair<float, float> AvoidVoidZones(float x, float y)
    {
        constexpr float SAFE_DISTANCE = 6.0f;
        std::vector<Unit*> zones;
        GuidVector triggers = *context->GetValue<GuidVector>("nearest triggers");
        for (ObjectGuid const guid : triggers)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->IsAlive() && botAI->EqualLowercaseName(unit->GetName(), "void zone"))
                zones.push_back(unit);
        }
        if (zones.empty())
            return {x, y};

        auto isSafe = [&zones](float px, float py)
        {
            for (Unit* zone : zones)
            {
                if (zone->GetDistance2d(px, py) < SAFE_DISTANCE)
                    return false;
            }
            return true;
        };
        if (isSafe(x, y))
            return {x, y};

        for (uint32 i = 0; i < 12; ++i)
        {
            float angle = 2 * M_PI * i / 12;
            float candX = x + cos(angle) * (SAFE_DISTANCE + 2.0f);
            float candY = y + sin(angle) * (SAFE_DISTANCE + 2.0f);
            if (isSafe(candX, candY))
                return {candX, candY};
        }
        return {x, y};
    }

protected:
    Unit* _sir = nullptr;
    Unit* _lady = nullptr;
    uint32 _combat_start_ms = 0;
    int posToGo = 0;
};
class ThaddiusBossHelper : public AiObject
{
public:
    const std::pair<float, float> tankPosFeugen = {3522.94f, -3002.60f};
    const std::pair<float, float> tankPosStalagg = {3436.14f, -2919.98f};
    const std::pair<float, float> rangedPosFeugen = {3500.45f, -2997.92f};
    const std::pair<float, float> rangedPosStalagg = {3441.01f, -2942.04f};
    const float tankPosZ = 312.61f;
    ThaddiusBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "thaddius");
            if (!_unit)
                return false;
        }
        feugen = AI_VALUE2(Unit*, "find target", "feugen");
        stalagg = AI_VALUE2(Unit*, "find target", "stalagg");
        return true;
    }
    bool IsPhasePet() { return (feugen && feugen->IsAlive()) || (stalagg && stalagg->IsAlive()); }
    bool IsPhaseTransition()
    {
        if (IsPhasePet())
            return false;

        return _unit && _unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }
    bool IsPhaseThaddius() { return !IsPhasePet() && !IsPhaseTransition(); }
    Unit* GetNearestPet()
    {
        Unit* unit = nullptr;
        if (feugen && feugen->IsAlive())
            unit = feugen;

        if (stalagg && stalagg->IsAlive() &&
            (!feugen || !feugen->IsAlive() || bot->GetDistance(stalagg) < bot->GetDistance(feugen)))
            unit = stalagg;

        return unit;
    }
    std::pair<float, float> PetPhaseGetPosForTank()
    {
        if (GetNearestPet() == feugen)
            return tankPosFeugen;

        return tankPosStalagg;
    }
    std::pair<float, float> PetPhaseGetPosForRanged()
    {
        if (GetNearestPet() == feugen)
            return rangedPosFeugen;

        return rangedPosStalagg;
    }

protected:
    void Reset()
    {
        _unit = nullptr;
        feugen = nullptr;
        stalagg = nullptr;
    }

    Unit* _unit = nullptr;
    Unit* feugen = nullptr;
    Unit* stalagg = nullptr;
};

#endif
