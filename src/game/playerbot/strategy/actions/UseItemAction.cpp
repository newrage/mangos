#include "../../../pchdef.h"
#include "../../playerbot.h"
#include "UseItemAction.h"

using namespace ai;

bool UseItemAction::Execute(Event event)
{
    string name = event.getParam();
    if (name.empty())
        name = getName();

    list<Item*> items = AI_VALUE2(list<Item*>, "inventory items", name);
    list<ObjectGuid> gos = chat->parseGameobjects(name);

    if (gos.empty() && items.empty())
    {
        ai->TellMaster(LOG_LVL_DEBUG, "Nothing to use");
        return false;
    }

    if (!gos.empty() && items.empty())
    {
        UseGameObject(*gos.begin());
        return true;
    }

    if (gos.empty() && !items.empty())
    {
        UseItem(*items.begin());
        return true;
    }

    Item* item = *items.begin();
    return UseItem(item, *gos.begin());
}

void UseItemAction::UseGameObject(ObjectGuid guid)
{
    GameObject* go = ai->GetGameObject(guid);
    if (!go || !go->isSpawned())
        return;

    go->Use(bot);
    ostringstream out; out << "Using " << chat->formatGameobject(go);
    ai->TellMaster(out);
}

bool UseItemAction::UseItem(Item* item, ObjectGuid goGuid)
{
    if (bot->CanUseItem(item) != EQUIP_ERR_OK)
        return false;

    if (bot->IsNonMeleeSpellCasted(true))
        return false;

    if (bot->isInCombat())
    {
        for(int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            SpellEntry const *spellInfo = sSpellStore.LookupEntry(item->GetProto()->Spells[i].SpellId);
            if (spellInfo && IsNonCombatSpell(spellInfo))
                return false;
        }

        if (item->IsPotion() && bot->GetLastPotionId())
            return false;
    }

    uint8 bagIndex = item->GetBagSlot();
    uint8 slot = item->GetSlot();
    uint8 cast_count = 1;
    uint64 item_guid = item->GetObjectGuid().GetRawValue();
    uint32 glyphIndex = 0;
    uint8 unk_flags = 0;

    WorldPacket* const packet = new WorldPacket(CMSG_USE_ITEM, 1 + 1 + 1 + 4 + 8 + 4 + 1 + 8 + 1);
    *packet << bagIndex << slot << cast_count << uint32(0) << item_guid
        << glyphIndex << unk_flags;

    bool targetSelected = false;
    ostringstream out; out << "Using " << chat->formatItem(item->GetProto());
    if (goGuid)
    {
        GameObject* go = ai->GetGameObject(goGuid);
        if (go && go->isSpawned())
        {
            uint32 targetFlag = TARGET_FLAG_OBJECT;
            *packet << targetFlag << goGuid.WriteAsPacked();
            out << " on " << chat->formatGameobject(go);
            targetSelected = true;
        }
    }

    if (!targetSelected && item->GetProto()->Class != ITEM_CLASS_CONSUMABLE)
    {
        ObjectGuid masterSelection = master->GetSelectionGuid();
        if (masterSelection)
        {
            Unit* unit = ai->GetUnit(masterSelection);
            if (unit)
            {
                uint32 targetFlag = TARGET_FLAG_UNIT;
                *packet << targetFlag << masterSelection.WriteAsPacked();
                out << " on " << unit->GetName();
                targetSelected = true;
            }
        }
    }

    if(uint32 questid = item->GetProto()->StartQuest)
    {
        Quest const* qInfo = sObjectMgr.GetQuestTemplate(questid);
        if (qInfo)
        {
            WorldPacket* const packet = new WorldPacket(CMSG_QUESTGIVER_ACCEPT_QUEST, 8+4+4);
            *packet << item_guid;
            *packet << questid;
            *packet << uint32(0);
            bot->GetSession()->QueuePacket(packet); // queue the packet to get around race condition
            ostringstream out; out << "Got quest " << chat->formatQuest(qInfo);
            ai->TellMaster(out);
            return true;
        }
    }

    MotionMaster &mm = *bot->GetMotionMaster();
    mm.Clear();
    mm.MoveIdle();
    bot->clearUnitState( UNIT_STAT_CHASE );
    bot->clearUnitState( UNIT_STAT_FOLLOW );

    if (bot->isMoving())
        return false;

    bool requiresItem = false;
    for (int i=0; i<MAX_ITEM_PROTO_SPELLS; i++)
    {
        uint32 spellId = item->GetProto()->Spells[i].SpellId;
        if (!spellId)
            continue;

        const SpellEntry* const pSpellInfo = sSpellStore.LookupEntry(spellId);
		requiresItem |= (pSpellInfo->Targets & TARGET_FLAG_ITEM);

        if (!ai->CanCastSpell(spellId, bot, false))
            return false;

        if (!(pSpellInfo->Targets & TARGET_FLAG_ITEM))
            continue;

        Item* itemForSpell = AI_VALUE2(Item*, "item for spell", spellId);
        if (!itemForSpell)
            continue;

        requiresItem = true;

        if (itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
            continue;

        if (bot->GetTrader())
        {
            *packet << TARGET_FLAG_TRADE_ITEM << (uint8)1 << (uint64)TRADE_SLOT_NONTRADED;
            targetSelected = true;
            out << " on traded item";
        }
        else
        {
            *packet << TARGET_FLAG_ITEM;
            *packet << itemForSpell->GetPackGUID();
            targetSelected = true;
            out << " on "<< chat->formatItem(itemForSpell->GetProto());
        }
        ai->WaitForSpellCast(spellId);
        break;
    }
    
    if (!targetSelected && requiresItem)
        return false;

    if (!targetSelected)
    {
        *packet << TARGET_FLAG_SELF;
        out << " on self";
    }

    if (item->GetProto()->Class == ITEM_CLASS_CONSUMABLE && item->GetProto()->SubClass == ITEM_SUBCLASS_FOOD)
    {
        bot->SetStandState(UNIT_STAND_STATE_SIT);
        ai->SetNextCheckDelay(30000);
    }
    else
    {
        bot->SetStandState(UNIT_STAND_STATE_STAND);
    }

    ai->TellMaster(out);
    bot->GetSession()->QueuePacket(packet);
    return true;
}

bool UseItemAction::isPossible()
{
    return getName() == "use" || AI_VALUE2(uint8, "item count", getName()) > 0;
}

bool UseSpellItemAction::isUseful()
{
    return AI_VALUE2(bool, "spell cast useful", getName());
}