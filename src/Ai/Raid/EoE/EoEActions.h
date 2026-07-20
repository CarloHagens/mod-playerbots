#ifndef PLAYERBOTS_EOEACTIONS_H
#define PLAYERBOTS_EOEACTIONS_H

#include <map>

#include "AttackAction.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

// Tank anchor ~14yd west of the platform center: the boss body ends up near the
// center, so every spark approach is roughly equally long
const std::pair<float, float> MALYGOS_MAINTANK_POSITION = {754.4f, 1315.0f};
// Malygos's combat reach is 20yd, so melee range against him is ~22.8yd raw and
// hunter minimum range is ~27.8yd raw. The whole raid (minus hunters) stacks at
// 20yd off his flank: melee still connect, gripped sparks land 8yd clear of the
// raw 12yd absorb radius, and the dropped buff zone covers the stack. Hunters
// hold the same flank line deeper, past their dead zone
const float MALYGOS_STACK_OFFSET = 20.0f;
const float MALYGOS_HUNTER_OFFSET = 32.0f;
const float MALYGOS_RANGED_RANGE = 30.0f;
const float MALYGOS_SPARK_MELEE_RANGE = 10.0f;
const float MALYGOS_BUBBLE_SLACK = 6.0f;
const float MALYGOS_TANK_EDGE_OFFSET = 4.0f;
// Melee may brawl this far from a bubble before being walked back in
const float MALYGOS_BRAWL_TETHER = 15.0f;

class MalygosPositionAction : public MovementAction
{
public:
    MalygosPositionAction(PlayerbotAI* botAI, std::string const name = "malygos position") : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;

private:
    bool MoveToBubble(bool preferNewest = false);
    bool FlyDiscToScion(Unit* vehicleBase);
    Creature* FindFreeDisc();

    // Which flank of the boss (sign along the perpendicular of his facing) the
    // raid last stacked on for sparks
    float _flankSign = 1.0f;
    // Anchored flank axis with hysteresis: re-follows the boss's orientation only
    // on real turns (>~30 degrees), so combat facing wobble does not swing the
    // stack around. Sentinel forces initialization on first use
    float _flankAxis = -10.0f;
    // When each phase 2 bubble was first seen - their despawn timers are not
    // readable, so age is tracked to know when one is about to fade
    std::map<ObjectGuid, time_t> _bubbleFirstSeen;
};

class MalygosTargetAction : public AttackAction
{
public:
    MalygosTargetAction(PlayerbotAI* botAI, std::string const name = "malygos target") : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
};

class MalygosGripSparkAction : public MovementAction
{
public:
    MalygosGripSparkAction(PlayerbotAI* botAI, std::string const name = "malygos grip spark")
        : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

class EoEFlyDrakeAction : public MovementAction
{
public:
    EoEFlyDrakeAction(PlayerbotAI* ai) : MovementAction(ai, "eoe fly drake") {}

    bool Execute(Event event) override;
    bool isPossible() override;
};

class EoEDrakeAttackAction : public Action
{
public:
    EoEDrakeAttackAction(PlayerbotAI* botAI) : Action(botAI, "eoe drake attack") {}

    bool Execute(Event event) override;
    bool isPossible() override;

protected:
    Unit* vehicleBase = nullptr;
    bool CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown);
    bool DrakeDpsAction(Unit* target);
    bool DrakeHealAction();
};

#endif
