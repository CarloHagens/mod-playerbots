#ifndef PLAYERBOTS_EOEACTIONCONTEXT_H
#define PLAYERBOTS_EOEACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "EoEActions.h"

class RaidEoEActionContext : public NamedObjectContext<Action>
{
public:
    RaidEoEActionContext()
    {
        creators["malygos position"] = &RaidEoEActionContext::position;
        creators["malygos target"] = &RaidEoEActionContext::target;
        creators["malygos grip spark"] = &RaidEoEActionContext::grip_spark;
        creators["eoe fly drake"] = &RaidEoEActionContext::eoe_fly_drake;
        creators["eoe drake attack"] = &RaidEoEActionContext::eoe_drake_attack;
    }

private:
    static Action* position(PlayerbotAI* ai) { return new MalygosPositionAction(ai); }
    static Action* target(PlayerbotAI* ai) { return new MalygosTargetAction(ai); }
    static Action* grip_spark(PlayerbotAI* ai) { return new MalygosGripSparkAction(ai); }
    static Action* eoe_fly_drake(PlayerbotAI* ai) { return new EoEFlyDrakeAction(ai); }
    static Action* eoe_drake_attack(PlayerbotAI* ai) { return new EoEDrakeAttackAction(ai); }
};

#endif
