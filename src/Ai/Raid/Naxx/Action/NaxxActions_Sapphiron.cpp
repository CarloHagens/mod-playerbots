#include "NaxxActions.h"

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "NaxxBossHelper.h"
#include "NaxxSpellIds.h"

bool SapphironGroundPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    if (botAI->IsMainTank(bot))
    {
        if (AI_VALUE2(bool, "has aggro", "current target"))
            return MoveTo(NAXX_MAP_ID, helper.mainTankPos.first, helper.mainTankPos.second, helper.GENERIC_HEIGHT, false, false, false,
                          false, MovementPriority::MOVEMENT_COMBAT);

        return false;
    }
    if (helper.JustLanded())
    {
        // Index among members of the same distance band, spread over the whole arc.
        // Raw raid-slot indices interleave roles at 0.02*PI apart, which is only ~2yd
        // between neighbours at 35yd — the raid formed small clusters instead of a fan.
        auto bandOf = [this](Player* player) -> int
        {
            if (botAI->IsRanged(player))
                return botAI->IsHeal(player) ? 1 : 2;
            return botAI->IsHeal(player) ? 1 : 0;
        };
        uint32 index = 0;
        uint32 total = 1;
        int myBand = bandOf(bot);
        if (Group* group = bot->GetGroup())
        {
            total = 0;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || bandOf(member) != myBand)
                    continue;

                if (member == bot)
                    index = total;
                ++total;
            }
        }
        float start_angle = 0.85 * M_PI;
        float span = 0.75 * M_PI;
        float angle = start_angle + (total > 1 ? span * index / (total - 1) : span / 2);
        float distance;
        if (myBand == 2)
            distance = 35.0f;
        else if (myBand == 1)
            distance = 30.0f;
        else
            distance = 5.0f;

        float posX = helper.center.first + cos(angle) * distance;
        float posY = helper.center.second + sin(angle) * distance;
        if (MoveTo(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            return true;

        return MoveInside(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, 2.0f, MovementPriority::MOVEMENT_COMBAT);
    }
    else
    {
        std::vector<float> dest;
        if (helper.FindPosToAvoidChill(dest))
            return MoveTo(NAXX_MAP_ID, dest[0], dest[1], dest[2], false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

bool SapphironFlightPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    if (helper.WaitForExplosion())
        return MoveToNearestIcebolt();

    // Not all ice bolts are out yet: hold the spread and only dodge Chill, so bots
    // don't herd onto the first block and get hit by the remaining bolts as a group.
    _hasHidePos = false;
    std::vector<float> dest;
    if (helper.FindPosToAvoidChill(dest))
        return MoveTo(NAXX_MAP_ID, dest[0], dest[1], dest[2], false, false, false, false, MovementPriority::MOVEMENT_COMBAT);

    return false;
}

bool SapphironFlightPositionAction::MoveToNearestIcebolt()
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Keep the spot picked earlier this air phase as long as its ice block is still up.
    if (_hasHidePos)
    {
        Unit* target = botAI->GetUnit(_hideTarget);
        bool stillIced = target && (NaxxSpellIds::HasAnyAura(target, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
                                    botAI->HasAura("icebolt", target, false, false, -1, true));
        if (stillIced)
        {
            if (bot->GetExactDist2d(_hideX, _hideY) <= 1.5f)
                return false;  // already in cover, stand still

            return MoveTo(NAXX_MAP_ID, _hideX, _hideY, helper.GENERIC_HEIGHT, false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
        _hasHidePos = false;
    }

    Player* playerWithIcebolt = nullptr;
    float minDistance;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (NaxxSpellIds::HasAnyAura(member, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
            botAI->HasAura("icebolt", member, false, false, -1, true))
        {
            if (!playerWithIcebolt || minDistance > bot->GetDistance(member))
            {
                playerWithIcebolt = member;
                minDistance = bot->GetDistance(member);
            }
        }
    }
    if (playerWithIcebolt)
    {
        Unit* boss = AI_VALUE2(Unit*, "find target", "sapphiron");
        if (boss)
        {
            // Stagger bots at different depths behind the block: the safe shadow is a cone
            // widening away from the boss, so lining up along the boss->block axis keeps
            // everyone covered instead of bulging out sideways from a single point.
            float depth = 2.0f + 0.5f * (botAI->GetGroupSlotIndex(bot) % 6);
            float angle = boss->GetAngle(playerWithIcebolt);
            _hideTarget = playerWithIcebolt->GetGUID();
            _hideX = playerWithIcebolt->GetPositionX() + cos(angle) * depth;
            _hideY = playerWithIcebolt->GetPositionY() + sin(angle) * depth;
            _hasHidePos = true;
            if (MoveTo(NAXX_MAP_ID, _hideX, _hideY, helper.GENERIC_HEIGHT, false, false, false, false,
                       MovementPriority::MOVEMENT_COMBAT))
                return true;

            return MoveNear(playerWithIcebolt, 3.0f, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}
