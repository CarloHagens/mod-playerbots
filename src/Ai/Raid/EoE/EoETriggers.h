#ifndef PLAYERBOTS_EOETRIGGERS_H
#define PLAYERBOTS_EOETRIGGERS_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

enum EyeOfEternityIDs
{
    NPC_MALYGOS                         = 28859,
    NPC_POWER_SPARK                     = 30084,
    NPC_NEXUS_LORD                      = 30245,
    NPC_SCION_OF_ETERNITY               = 30249,
    NPC_HOVER_DISK                      = 30248,
    NPC_ARCANE_OVERLOAD                 = 30282,
    NPC_WYRMREST_SKYTALON               = 30161,

    // Ground zone (+50% damage done) left where a spark is killed
    SPELL_POWER_SPARK_GROUND_BUFF       = 55852,

    // Phase 2 deep breath, channeled at a stalk in the platform center
    SPELL_SURGE_OF_POWER                = 56505,

    // Drake abilities (phase 3)
    SPELL_FLAME_SPIKE                   = 56091,
    SPELL_ENGULF_IN_FLAMES              = 56092,
    SPELL_REVIVIFY                      = 57090,
    SPELL_LIFE_BURST                    = 57143,
};

const uint32 EOE_MAP_ID = 616;
const float EOE_PLATFORM_Z = 267.24f;
const std::pair<float, float> EOE_CENTER_POSITION = {754.4f, 1301.3f};

class MalygosTrigger : public Trigger
{
public:
    MalygosTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos") {}
    bool IsActive() override;
    uint8 static getPhase(Player* bot, Unit* boss);
    static Unit* FindNearestAlive(PlayerbotAI* botAI, Player* bot, uint32 entry, float maxRange = 0.0f);
    // Grid-based lookups: sight-limited values (~60yd) lose Malygos on his 83yd
    // phase 2 circle and miss sparks spawning 95yd out
    static Unit* FindMalygos(Player* bot);
    static Unit* FindLiveSpark(Player* bot);
    static Unit* FindLooseNexusLord(PlayerbotAI* botAI, Player* bot);
    static bool IsSurgeOfPowerImminent(Unit* boss);
    // True while the bot stands within radius of a live bubble - or when no
    // bubbles are up at all (nothing to hide in yet)
    static bool IsNearBubble(Player* bot, float radius);
    static Creature* FindNearestBubble(Player* bot);
    // A living DK not currently tanking the boss, free to grip sparks to the stack
    static bool HasSparkGripper(PlayerbotAI* botAI, Player* bot);
    // A spent spark (unattackable, grounded) anchoring its 60s damage buff zone
    static Creature* FindSparkBuffZone(Player* bot);
};

class PowerSparkTrigger : public Trigger
{
public:
    PowerSparkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "power spark") {}
    bool IsActive() override;
};

#endif
