#include "NaxxActions.h"

#include "Playerbots.h"

bool GothikChooseTargetAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    // Ground phase: default assist logic handles the boss and any leftover adds.
    if (!helper.IsBalconyPhase())
        return false;

    Unit* boss = helper.GetBoss();
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    Unit* target = nullptr;
    for (auto i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit || !unit->IsAlive() || unit == boss)
            continue;

        if (!target || bot->GetDistance(unit) < bot->GetDistance(target))
            target = unit;
    }
    // Between waves: wait instead of turning to the balcony.
    if (!target)
        return false;

    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;

    return Attack(target);
}
