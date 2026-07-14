#include "OSStrategy.h"
#include "OSMultipliers.h"
#include "Strategy.h"

void RaidOsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("sartharion tank",
                        { NextAction("sartharion tank position", ACTION_MOVE) }));
    triggers.push_back(
        new TriggerNode("twilight fissure",
                        { NextAction("avoid twilight fissure", ACTION_RAID + 2) }));
    triggers.push_back(
        new TriggerNode("flame tsunami",
                        { NextAction("avoid flame tsunami", ACTION_RAID + 1) }));
    triggers.push_back(
        new TriggerNode("sartharion dps",
                        { NextAction("sartharion attack priority", ACTION_RAID) }));
    // South flank positioning: out of the boss arcs, stand-still for right waves,
    // and a short southward dodge (with the ranged group) for left waves
    triggers.push_back(new TriggerNode("sartharion melee positioning",
        { NextAction("sartharion melee position", ACTION_MOVE + 4) }));
    // Anchor ranged/healers in a wave gap so tsunami dodges start from a known spot
    triggers.push_back(new TriggerNode("sartharion ranged positioning",
        { NextAction("sartharion ranged position", ACTION_MOVE + 4) }));

    triggers.push_back(new TriggerNode("twilight portal enter",
        { NextAction("enter twilight portal", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("twilight portal exit",
        { NextAction("exit twilight portal", ACTION_RAID + 1) }));
}

void RaidOsStrategy::InitMultipliers(std::vector<Multiplier*> &multipliers)
{
    multipliers.push_back(new SartharionMultiplier(botAI));
}
