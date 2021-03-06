#include "tristam_catacombs.h"
#include "AreaBoundary.h"
#include "CreatureAI.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "WorldStatePackets.h"

class instance_tristam_catacombs : public InstanceMapScript
{
    public:
        instance_tristam_catacombs() : InstanceMapScript(TCScriptName, 732) { }

        struct instance_tristam_catacombs_InstanceMapScript : public InstanceScript
        {
            instance_tristam_catacombs_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
            {
                SetHeaders(DataHeader);
                SetBossNumber(EncounterCount);
            }

            void OnPlayerEnter(Player* /*player*/) override
            {
                if (Creature* netristrasza = instance->GetCreature(netristraszaGUID))
                    netristrasza->SummonCreature(NPC_TIME_RIFT, *netristrasza);
            }

            void ProcessEvent(WorldObject* obj, uint32 eventId) override
            {
                switch (eventId)
                {
                    case EVENT_OPEN_WIND_DOORS:
                        obj->PlayDirectSound(15374);
                        for (ObjectGuid doorGUID : windsGUID)
                            HandleGameObject(doorGUID, true);
                        break;
                    default:
                        break;
                }
            }

            void OnGameObjectCreate(GameObject* go) override
            {
                switch (go->GetEntry())
                {
                    case GOB_DARK_GATE:
                        HandleGameObject(ObjectGuid::Empty, false, go);
                        go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
                        windsGUID.push_back(go->GetGUID());
                        break;
                    case GOB_IRON_GATE:
                        HandleGameObject(ObjectGuid::Empty, false, go);
                        go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
                        break;
                    default:
                        break;
                }
            }

            void OnCreatureCreate(Creature* creature) override
            {
                switch (creature->GetEntry())
                {
                    case NPC_NETRISTRASZA:
                        creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                        creature->SetVisible(false);
                        netristraszaGUID = creature->GetGUID();
                        break;
                    case NPC_ANTONN_GRAVE:
                        creature->SetControlled(true, UNIT_STATE_ROOT);
                        antonnGUID = creature->GetGUID();
                        break;
                    case NPC_TIME_RIFT:
                        riftGUID = creature->GetGUID();
                        break;
                    case NPC_LEORIC:
                        leoricGUID = creature->GetGUID();
                        break;
                    case NPC_KORMAC:
                        kormacGUID = creature->GetGUID();
                        creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPELLCLICK);
                        creature->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_STRANGULATE);
                        creature->SetHealth(creature->GetMaxHealth() / 2.f);
                        creature->AddAura(SPELL_STASIS_FIELD, creature);
                        break;
                    case NPC_GHEZRIM:
                        ghezrimGUID = creature->GetGUID();
                        break;
                    default:
                        break;
                }
            }

            ObjectGuid GetGuidData(uint32 type) const override
            {
                switch (type)
                {
                    case DATA_NETRISTRASZA:
                        return netristraszaGUID;
                    case DATA_ANTONN_GRAVE:
                        return antonnGUID;
                    case DATA_TIME_RIFT:
                        return riftGUID;
                    case DATA_LEORIC:
                        return leoricGUID;
                    case DATA_KORMAC:
                        return kormacGUID;
                    case DATA_GHEZRIM:
                        return ghezrimGUID;
                    default:
                        break;
                }

                return ObjectGuid::Empty;
            }

            bool SetBossState(uint32 type, EncounterState state) override
            {
                if (!InstanceScript::SetBossState(type, state))
                    return false;

                switch (type)
                {
                    case DATA_NETRISTRASZA:
                        switch (state)
                        {
                            case IN_PROGRESS:
                                if (Creature* netristrasza = instance->GetCreature(netristraszaGUID))
                                    netristrasza->AI()->DoAction(ACTION_START_DUNGEON);
                                break;
                            case FAIL:
                                if (Creature* netristrasza = instance->GetCreature(netristraszaGUID))
                                    netristrasza->DespawnOrUnsummon();
                                break;
                            default:
                                break;
                        }
                        break;
                    case DATA_ANTONN_GRAVE:
                        switch (state)
                        {
                            case DONE:
                                if (Creature* netristrasza = instance->GetCreature(netristraszaGUID))
                                    netristrasza->AI()->DoAction(ACTION_AG_END);
                                break;
                            default:
                                break;
                        }
                        break;
                    default:
                        break;
                }

                return true;
            }

            private:
            ObjectGuid riftGUID;
            ObjectGuid netristraszaGUID;
            ObjectGuid antonnGUID;
            ObjectGuid leoricGUID;
            ObjectGuid kormacGUID;
            ObjectGuid ghezrimGUID;
            std::vector<ObjectGuid> windsGUID;
        };

        InstanceScript* GetInstanceScript(InstanceMap* map) const override
        {
            return new instance_tristam_catacombs_InstanceMapScript(map);
        }
};

void AddSC_instance_tristam_catacombs()
{
    new instance_tristam_catacombs();
}
