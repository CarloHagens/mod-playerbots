#ifndef PLAYERBOTS_OSACTIONS_H
#define PLAYERBOTS_OSACTIONS_H

#include "MovementActions.h"
#include "AttackAction.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

// Left wave gaps are Y ~498-518 (south) and ~546-566 (north); the right wave gap is Y ~522-542.
// Everyone except the main tank lives on the south side, so left waves are dodged into the
// south gap and only the main tank sidesteps north (which pivots the boss away from the raid).
const float TSUNAMI_LEFT_SAFE_MELEE = 510.0f;
const float TSUNAMI_LEFT_SAFE_RANGED = 504.0f;
const float TSUNAMI_LEFT_SAFE_TANK = 552.0f;
const float TSUNAMI_RIGHT_SAFE_ALL = 529.0f;
// Tank anchors sit inside the right wave gap (Y ~522-542), hugging its north edge:
// right waves then require no movement at all, and left waves are a short sidestep
// north to TSUNAMI_LEFT_SAFE_MELEE, which turns the boss away from the ranged camp.
// The main tank holds the boss at the WEST edge: the only waves that can catch a tank
// travel west-to-east, so a missed sidestep knocks him onto the platform, not into
// the east lava, and the boss's frontal cone points off-platform.
// The offtank holds the west edge too, one wave lane south in the LEFT wave gap: the
// drakes have frontal breaths just like the boss, so everything tanked at the edge
// faces west with its cone pointing at the lava. Dodge duties are complementary (main
// tank moves only for left waves, offtank only for right waves), adds get tanked near
// the ranged/melee dodge cluster for quick switches with their safe rear arc toward
// the raid, and he is ~30y perpendicular off the boss's axis - clear of its tail lash.
// X 3232 keeps the tanked bodies fully on the platform (their models are 10-15y deep
// and hang into the moat if the tank hugs the edge line itself).
// The ranged camp tucks in south-west behind the offtank's add pile: the boss parks
// angled from his southern approach at the pull, tail sweeping the south-EAST quadrant,
// and the camp must stay out of it (as well as out of the drakes' westward breaths).
const std::pair<float, float> SARTHARION_MAINTANK_POSITION = {3232.0f, 537.0f};
const std::pair<float, float> SARTHARION_OFFTANK_POSITION = {3232.0f, 508.0f};
const std::pair<float, float> SARTHARION_RANGED_POSITION = {3240.0f, 502.0f};

// Flame tsunami helpers shared by the avoidance action and the strategy multiplier
bool IsRightFlameTsunami(Unit* tsunami);
bool IsFlameTsunamiIncoming(Unit* tsunami, Player* bot);

class SartharionTankPositionAction : public AttackAction
{
public:
    SartharionTankPositionAction(PlayerbotAI* botAI, std::string const name = "sartharion tank position")
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

class AvoidTwilightFissureAction : public MovementAction
{
public:
    AvoidTwilightFissureAction(PlayerbotAI* botAI, std::string const name = "avoid twilight fissure")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class AvoidFlameTsunamiAction : public MovementAction
{
public:
    AvoidFlameTsunamiAction(PlayerbotAI* botAI, std::string const name = "avoid flame tsunami")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class SartharionRangedPositionAction : public MovementAction
{
public:
    SartharionRangedPositionAction(PlayerbotAI* botAI, std::string const name = "sartharion ranged position")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class SartharionMeleePositionAction : public MovementAction
{
public:
    SartharionMeleePositionAction(PlayerbotAI* botAI, std::string const name = "sartharion melee position")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class SartharionAttackPriorityAction : public AttackAction
{
public:
    SartharionAttackPriorityAction(PlayerbotAI* botAI, std::string const name = "sartharion attack priority")
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

class EnterTwilightPortalAction : public MovementAction
{
public:
    EnterTwilightPortalAction(PlayerbotAI* botAI, std::string const name = "enter twilight portal")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class ExitTwilightPortalAction : public MovementAction
{
public:
    ExitTwilightPortalAction(PlayerbotAI* botAI, std::string const name = "exit twilight portal")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

#endif
