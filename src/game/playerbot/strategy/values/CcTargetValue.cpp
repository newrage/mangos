#include "../../../pchdef.h"
#include "../../playerbot.h"
#include "CcTargetValue.h"
#include "../../PlayerbotAIConfig.h"
#include "../Action.h"

using namespace ai;

class FindTargetForCcStrategy : public FindTargetStrategy
{
public:
    FindTargetForCcStrategy(PlayerbotAI* ai, string spell) : FindTargetStrategy(ai)
    {
        this->spell = spell;
        maxDistance = 0;
    }

protected:
    virtual void CheckAttacker(Player* bot, Player* player, ThreatManager* threatManager)
    {
        float threat = threatManager->getThreat(bot);
        Unit* creature = threatManager->getOwner();
        if (!creature || creature->isDead())
            return;

        uint8 health = creature->GetHealthPercent();
        if (health < sPlayerbotAIConfig.mediumHealth)
            return;

        if (!ai->CanCastSpell(spell, creature))
            return;

        if (*ai->GetAiObjectContext()->GetValue<Unit*>("rti target") == creature)
        {
            result = creature;
            return;
        }

        float minDistance = sPlayerbotAIConfig.reactDistance;
        Group* group = bot->GetGroup();
        if (group)
        {
            if (group->GetTargetIcon(4) == creature->GetObjectGuid())
            {
                result = creature;
                return;
            }

            Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
            for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
            {
                Player *member = sObjectMgr.GetPlayer(itr->guid);
                if( !member || !member->isAlive() || member == bot)
                    continue;

                if (!ai->IsTank(member))
                    continue;

                float distance = member->GetDistance(creature);
                if (distance < minDistance)
                    minDistance = distance;
            }
        }

        if (minDistance < 10.0f)
            return;

        if (!result || minDistance > maxDistance)
        {
            result = creature;
            maxDistance = minDistance;
        }
    }

private:
    string spell;
    float maxDistance;
};

Unit* CcTargetValue::Calculate()
{
    FindTargetForCcStrategy strategy(ai, qualifier);
    return FindTarget(&strategy);
}