/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptPCH.h"
#include "oculus.h"

#define MAX_ENCOUNTER 4

/* The Occulus encounters:
0 - Drakos the Interrogator
1 - Varos Cloudstrider
2 - Mage-Lord Urom
3 - Ley-Guardian Eregos */

enum eAchievements
{
    ACHIEV_TIMED_START_EVENT = 18153,
};

class instance_oculus : public InstanceMapScript
{
public:
    instance_oculus() : InstanceMapScript("instance_oculus", 578) { }

    InstanceScript* GetInstanceScript(InstanceMap* map) const
    {
        return new instance_oculus_InstanceMapScript(map);
    }

    struct instance_oculus_InstanceMapScript : public InstanceScript
    {
        instance_oculus_InstanceMapScript(Map* map) : InstanceScript(map) {}

        void Initialize()
        {
            SetBossNumber(MAX_ENCOUNTER);

            drakosGUID = 0;
            varosGUID = 0;
            uromGUID = 0;
            eregosGUID = 0;

            platformUrom = 0;
            centrifugueConstructCounter = 0;

            eregosCacheGUID = 0;
            lightCacheGUID = 0;

            unusedDrakesList.clear();
            gameObjectList.clear();
        }

        void OnUnitDeath(Unit* unit)
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            unusedDrakesList.remove(creature->GetGUID());
            if (creature->GetEntry() != NPC_CENTRIFUGE_CONSTRUCT)
                return;

             DoUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_AMOUNT, --centrifugueConstructCounter);

             if (!centrifugueConstructCounter)
                if (Creature* varos = instance->GetCreature(varosGUID))
                    varos->RemoveAllAuras();
        }

        void OnPlayerEnter(Player* player)
        {
            if (GetBossState(DATA_DRAKOS_EVENT) == DONE && GetBossState(DATA_VAROS_EVENT) != DONE)
            {
                player->SendUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_SHOW, 1);
                player->SendUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_AMOUNT, centrifugueConstructCounter);
            } else
            {
                player->SendUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_SHOW, 0);
                player->SendUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_AMOUNT, 0);
            }
        }

        void ProcessEvent(WorldObject* /*unit*/, uint32 eventId)
        {
            if (eventId != EVENT_CALL_DRAGON)
                return;

            Creature* varos = instance->GetCreature(varosGUID);

            if (!varos)
                return;

            if (Creature* drake = varos->SummonCreature(NPC_AZURE_RING_GUARDIAN, varos->GetPositionX(), varos->GetPositionY(), varos->GetPositionZ()+40))
                drake->AI()->DoAction(ACTION_CALL_DRAGON_EVENT);
        }

        void OnCreatureCreate(Creature* creature)
        {
            switch (creature->GetEntry())
            {
                case NPC_DRAKOS:
                    drakosGUID = creature->GetGUID();
                    break;
                case NPC_VAROS:
                    varosGUID = creature->GetGUID();
                    break;
                case NPC_UROM:
                    if (GetBossState(DATA_VAROS_EVENT) != DONE)
                    {
                        creature->SetReactState(REACT_PASSIVE);
                        creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
                    }
                    uromGUID = creature->GetGUID();
                    break;
                case NPC_EREGOS:
                    if (GetBossState(DATA_UROM_EVENT) != DONE)
                    {
                        creature->SetReactState(REACT_PASSIVE);
                        creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
                    }
                    eregosGUID = creature->GetGUID();
                    break;
                case NPC_CENTRIFUGE_CONSTRUCT:
                    if (creature->isAlive())
                        DoUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_AMOUNT, ++centrifugueConstructCounter);
                    break;
                case NPC_RUBY_DRAKE_VEHICLE:
                case NPC_EMERALD_DRAKE_VEHICLE:
                case NPC_AMBER_DRAKE_VEHICLE:
                    unusedDrakesList.push_back(creature->GetGUID());
                    break;
            }
        }

        void OnGameObjectCreate(GameObject* go)
        {
            switch (go->GetEntry())
            {
                case GO_DRAGON_CAGE_DOOR:
                    if (GetBossState(DATA_DRAKOS_EVENT) == DONE)
                        go->SetGoState(GO_STATE_ACTIVE);
                    else
                        go->SetGoState(GO_STATE_READY);
                    gameObjectList.push_back(go->GetGUID());
                    break;
                case GO_EREGOS_CACHE_N:
                case GO_EREGOS_CACHE_H:
                    eregosCacheGUID = go->GetGUID();
                    if (GetBossState(DATA_EREGOS_EVENT) != DONE)
                        go->SetPhaseMask(2, true);
                    break;
                case GO_CACHE_SPOTLIGHT:
                    lightCacheGUID = go->GetGUID();
                    if (GetBossState(DATA_EREGOS_EVENT) != DONE)
                        go->SetPhaseMask(2, true);
                    break;
                default:
                    break;
            }
        }

        bool SetBossState(uint32 type, EncounterState state)
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            switch (type)
            {
                case DATA_DRAKOS_EVENT:
                    if (state == DONE)
                    {
                        DoUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_SHOW, 1);
                        DoUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_AMOUNT, centrifugueConstructCounter);
                        DoStartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, ACHIEV_TIMED_START_EVENT);
                        OpenCageDoors();
                    }
                    break;
                case DATA_VAROS_EVENT:
                    if (state == DONE)
                    {
                        DoUpdateWorldState(WORLD_STATE_CENTRIFUGE_CONSTRUCT_SHOW, 0);
                        if (Creature* urom = instance->GetCreature(uromGUID))
                        {
                            urom->SetReactState(REACT_AGGRESSIVE);
                            urom->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
                            urom->RemoveAllAuras();
                        }
                    }
                    break;
                case DATA_UROM_EVENT:
                    if (state == DONE)
                    {
                        if (Creature* eregos = instance->GetCreature(eregosGUID))
                        {
                            eregos->SetReactState(REACT_AGGRESSIVE);
                            eregos->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
                        }
                    }
                    break;
                case DATA_EREGOS_EVENT:
                    if (state == IN_PROGRESS)
                        DespawnUnusedDrakes();

                    if (state == DONE)
                    {
                        if (GameObject* cache = instance->GetGameObject(eregosCacheGUID))
                            cache->SetPhaseMask(1, true);
                        if (GameObject* light = instance->GetGameObject(lightCacheGUID))
                            light->SetPhaseMask(1, true);
                    }
                    break;
            }

            return true;
        }

        void SetData(uint32 type, uint32 data)
        {
            switch (type)
            {
                case DATA_UROM_PLATAFORM:
                    platformUrom = data;
                    break;
            }
        }

        uint32 GetData(uint32 type)
        {
            switch (type)
            {
                case DATA_UROM_PLATAFORM:              return platformUrom;
                // used by condition system
                case DATA_UROM_EVENT:                  return GetBossState(DATA_UROM_EVENT);
            }

            return 0;
        }

        uint64 GetData64(uint32 identifier)
        {
            switch (identifier)
            {
                case DATA_DRAKOS:                 return drakosGUID;
                case DATA_VAROS:                  return varosGUID;
                case DATA_UROM:                   return uromGUID;
                case DATA_EREGOS:                 return eregosGUID;
            }

            return 0;
        }

        void SetData64(uint32 type, uint64 data)
        {
            switch (type)
            {
                case DATA_DRAKE_IN_USE:
                    unusedDrakesList.remove(data);
                    break;
                case DATA_DRAKE_NOT_IN_USE:
                    unusedDrakesList.push_back(data);
                    break;
            }
        }
        void OpenCageDoors()
        {
            if (gameObjectList.empty())
                return;

            for (std::list<uint64>::const_iterator itr = gameObjectList.begin(); itr != gameObjectList.end(); ++itr)
            {
                if (GameObject* go = instance->GetGameObject(*itr))
                    go->SetGoState(GO_STATE_ACTIVE);
            }
        }

        std::string GetSaveData()
        {
            OUT_SAVE_INST_DATA;

            std::ostringstream saveStream;
            saveStream << "T O " << GetBossSaveData();

            str_data = saveStream.str();

            OUT_SAVE_INST_DATA_COMPLETE;
            return str_data;
        }

        void Load(const char* in)
        {
            if (!in)
            {
                OUT_LOAD_INST_DATA_FAIL;
                return;
            }

            OUT_LOAD_INST_DATA(in);

            char dataHead1, dataHead2;

            std::istringstream loadStream(in);
            loadStream >> dataHead1 >> dataHead2;

            if (dataHead1 == 'T' && dataHead2 == 'O')
            {
                for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
                {
                    uint32 tmpState;
                    loadStream >> tmpState;
                    if (tmpState == IN_PROGRESS || tmpState > SPECIAL)
                        tmpState = NOT_STARTED;
                    SetBossState(i, EncounterState(tmpState));
                }
            } else OUT_LOAD_INST_DATA_FAIL;

            OUT_LOAD_INST_DATA_COMPLETE;
        }

        private:
            uint64 drakosGUID;
            uint64 varosGUID;
            uint64 uromGUID;
            uint64 eregosGUID;

            uint8 platformUrom;
            uint8 centrifugueConstructCounter;

            uint64 eregosCacheGUID;
            uint64 lightCacheGUID;

            std::string str_data;

            std::list<uint64> gameObjectList;
            std::list<uint64> unusedDrakesList;

            void DespawnUnusedDrakes()
            {
                while (!unusedDrakesList.empty())
                {
                    Creature *summon = instance->GetCreature(*unusedDrakesList.begin());
                    if (!summon)
                        unusedDrakesList.erase(unusedDrakesList.begin());
                    else
                    {
                        unusedDrakesList.erase(unusedDrakesList.begin());
                        if (TempSummon* summ = summon->ToTempSummon())
                        {
                            summon->DestroyForNearbyPlayers();
                            summ->UnSummon();
                        }
                        else
                            summon->DisappearAndDie();
                    }
                }
            }
    };

};

void AddSC_instance_oculus()
{
    new instance_oculus();
}
