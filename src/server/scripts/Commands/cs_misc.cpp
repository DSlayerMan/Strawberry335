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

#include "Chat.h"
#include "ScriptMgr.h"
#include "AccountMgr.h"
#include "ArenaTeamMgr.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "Language.h"
#include "MovementGenerator.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "SpellAuras.h"
#include "TargetedMovementGenerator.h"
#include "WeatherMgr.h"
#include "ace/INET_Addr.h"
#include "Player.h"
#include "Pet.h"
#include "LFG.h"
#include "GroupMgr.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "LFGMgr.h"
#include "Transport.h"
#include "GuildMgr.h"

const uint32 GMRewardItems[] =
{
    50764, 50773, 50774, 50762, 50775, 50772, 50763, 50339, 50771, 50761, 50759, 50760,
    50785, 50782, 50780, 50778, 50783, 50777, 50784, 50779, 50786, 50342, 50781, 50776,
    50791, 50795, 50797, 50792, 50789, 50796, 50788, 50790, 50340, 50793, 50787, 50794,
    50807, 50804, 50799, 50806, 50800, 50801, 50802, 50808, 50809, 50803, 50798, 50805,
    50859, 50988, 50990, 50985, 50858, 50812, 50967, 50811, 50852, 50986, 50810, 50966,
    51007, 51005, 51009, 51002, 51006, 51000, 51008, 51001, 51003, 51004, 50998, 50999,
    51020, 51017, 51013, 51015, 51019, 51014, 51018, 51012, 51016, 50341, 51011, 51010,
    51382, 51379, 51380, 51023, 51325, 51383, 51025, 51381, 51024, 51021, 51022, 51326,
    51554, 51552, 51550, 51551, 51386, 51556, 51555, 51548, 51387, 51384, 51385, 51553,
    51584, 51777, 51585, 51565, 51583, 51566, 51586, 51563, 51564, 51562, 51582, 51561,
    51790, 51783, 51789, 51792, 51785, 51782, 51786, 51787, 51779, 51784, 51788, 51791
};

int32 sbTanks;
int32 sbHeals;
int32 sbDamage;
int32 sbRaidSize = 0;
Player* sbRaidLeader;
std::list<uint64> tanks;
std::list<uint64> heals;
std::list<uint64> damage;

class misc_commandscript : public CommandScript
{
public:
    misc_commandscript() : CommandScript("misc_commandscript") { }

    ChatCommand* GetCommands() const
    {
        static ChatCommand groupCommandTable[] =
        {
            { "leader", SEC_ADMINISTRATOR, false, &HandleGroupLeaderCommand, "", NULL },
            { "disband", SEC_ADMINISTRATOR, false, &HandleGroupDisbandCommand, "", NULL },
            { "remove", SEC_ADMINISTRATOR, false, &HandleGroupRemoveCommand, "", NULL },
            { "join", SEC_ADMINISTRATOR, false, &HandleGroupJoinCommand, "", NULL },
            { "list", SEC_ADMINISTRATOR, false, &HandleGroupListCommand, "", NULL },
            { NULL, 0, false, NULL, "", NULL }
        };
        static ChatCommand petCommandTable[] =
        {
            { "create", SEC_GAMEMASTER, false, &HandleCreatePetCommand, "", NULL },
            { "learn", SEC_GAMEMASTER, false, &HandlePetLearnCommand, "", NULL },
            { "unlearn", SEC_GAMEMASTER, false, &HandlePetUnlearnCommand, "", NULL },
            { NULL, 0, false, NULL, "", NULL }
        };
        static ChatCommand sbCommandTable[] =
        {
            { "new", SEC_GAMEMASTER, false, &HandleSbNewCommand, "", NULL },
            { "join", SEC_PLAYER, false, &HandleSbJoinCommand, "", NULL },
            { "remove", SEC_GAMEMASTER, false, &HandleSbRemoveCommand, "", NULL },
            { "status", SEC_PLAYER, false, &HandleSbStatusCommand, "", NULL },
            { NULL, 0, false, NULL, "", NULL }
        };
        static ChatCommand sendCommandTable[] =
        {
            { "items", SEC_ADMINISTRATOR, true, &HandleSendItemsCommand, "", NULL },
            { "mail", SEC_MODERATOR, true, &HandleSendMailCommand, "", NULL },
            { "message", SEC_ADMINISTRATOR, true, &HandleSendMessageCommand, "", NULL },
            { "money", SEC_ADMINISTRATOR, true, &HandleSendMoneyCommand, "", NULL },
            { "gmreward", SEC_ADMINISTRATOR, true, &HandleSendGMRewardCommand, "", NULL },
            { NULL, 0, false, NULL, "", NULL }
        };
        static ChatCommand commandTable[] =
        {
            { "dev", SEC_ADMINISTRATOR, false, &HandleDevCommand, "", NULL },
            { "gps", SEC_ADMINISTRATOR, false, &HandleGPSCommand, "", NULL },
            { "aura", SEC_ADMINISTRATOR, false, &HandleAuraCommand, "", NULL },
            { "unaura", SEC_ADMINISTRATOR, false, &HandleUnAuraCommand, "", NULL },
            { "appear", SEC_MODERATOR, false, &HandleAppearCommand, "", NULL },
            { "summon", SEC_MODERATOR, false, &HandleSummonCommand, "", NULL },
            { "groupsummon", SEC_MODERATOR, false, &HandleGroupSummonCommand, "", NULL },
            { "commands", SEC_PLAYER, true, &HandleCommandsCommand, "", NULL },
            { "die", SEC_ADMINISTRATOR, false, &HandleDieCommand, "", NULL },
            { "revive", SEC_ADMINISTRATOR, true, &HandleReviveCommand, "", NULL },
            { "dismount", SEC_PLAYER, false, &HandleDismountCommand, "", NULL },
            { "guid", SEC_GAMEMASTER, false, &HandleGUIDCommand, "", NULL },
            { "help", SEC_PLAYER, true, &HandleHelpCommand, "", NULL },
            { "itemmove", SEC_GAMEMASTER, false, &HandleItemMoveCommand, "", NULL },
            { "cooldown", SEC_ADMINISTRATOR, false, &HandleCooldownCommand, "", NULL },
            { "distance", SEC_ADMINISTRATOR, false, &HandleGetDistanceCommand, "", NULL },
            { "recall", SEC_MODERATOR, false, &HandleRecallCommand, "", NULL },
            { "save", SEC_PLAYER, false, &HandleSaveCommand, "", NULL },
            { "saveall", SEC_MODERATOR, true, &HandleSaveAllCommand, "", NULL },
            { "kick", SEC_GAMEMASTER, true, &HandleKickPlayerCommand, "", NULL },
            { "unstuck", SEC_PLAYER, true, &HandleUnstuckCommand, "", NULL },
            { "linkgrave", SEC_ADMINISTRATOR, false, &HandleLinkGraveCommand, "", NULL },
            { "neargrave", SEC_ADMINISTRATOR, false, &HandleNearGraveCommand, "", NULL },
            { "showarea", SEC_ADMINISTRATOR, false, &HandleShowAreaCommand, "", NULL },
            { "hidearea", SEC_ADMINISTRATOR, false, &HandleHideAreaCommand, "", NULL },
            { "additem", SEC_ADMINISTRATOR, false, &HandleAddItemCommand, "", NULL },
            { "additemset", SEC_ADMINISTRATOR, false, &HandleAddItemSetCommand, "", NULL },
            { "bank", SEC_ADMINISTRATOR, false, &HandleBankCommand, "", NULL },
            { "wchange", SEC_ADMINISTRATOR, false, &HandleChangeWeather, "", NULL },
            { "maxskill", SEC_ADMINISTRATOR, false, &HandleMaxSkillCommand, "", NULL },
            { "setskill", SEC_ADMINISTRATOR, false, &HandleSetSkillCommand, "", NULL },
            { "pinfo", SEC_GAMEMASTER, true, &HandlePInfoCommand, "", NULL },
            { "respawn", SEC_ADMINISTRATOR, false, &HandleRespawnCommand, "", NULL },
            { "send", SEC_MODERATOR, true, NULL, "", sendCommandTable },
            { "pet", SEC_GAMEMASTER, false, NULL, "", petCommandTable },
            { "mute", SEC_MODERATOR, true, &HandleMuteCommand, "", NULL },
            { "unmute", SEC_MODERATOR, true, &HandleUnmuteCommand, "", NULL },
            { "movegens", SEC_ADMINISTRATOR, false, &HandleMovegensCommand, "", NULL },
            { "cometome", SEC_ADMINISTRATOR, false, &HandleComeToMeCommand, "", NULL },
            { "damage", SEC_ADMINISTRATOR, false, &HandleDamageCommand, "", NULL },
            { "combatstop", SEC_GAMEMASTER, true, &HandleCombatStopCommand, "", NULL },
            { "flusharenapoints", SEC_ADMINISTRATOR, false, &HandleFlushArenaPointsCommand, "", NULL },
            { "repairitems", SEC_GAMEMASTER, true, &HandleRepairitemsCommand, "", NULL },
            { "freeze", SEC_MODERATOR, false, &HandleFreezeCommand, "", NULL },
            { "unfreeze", SEC_MODERATOR, false, &HandleUnFreezeCommand, "", NULL },
            { "listfreeze", SEC_MODERATOR, false, &HandleListFreezeCommand, "", NULL },
            { "group", SEC_ADMINISTRATOR, false, NULL, "", groupCommandTable },
            { "possess", SEC_ADMINISTRATOR, false, HandlePossessCommand, "", NULL },
            { "unpossess", SEC_ADMINISTRATOR, false, HandleUnPossessCommand, "", NULL },
            { "bindsight", SEC_ADMINISTRATOR, false, HandleBindSightCommand, "", NULL },
            { "unbindsight", SEC_ADMINISTRATOR, false, HandleUnbindSightCommand, "", NULL },
            { "playall", SEC_GAMEMASTER, false, HandlePlayAllCommand, "", NULL },
            { "bgquery",        SEC_PLAYER,         false, &HandleBGQueryCommand,                 "", NULL },
            { "report", SEC_PLAYER, false, &HandleReportCommand, "", NULL},
            { "sb", SEC_PLAYER, false, NULL, "", sbCommandTable },
            { "joinstartguild", SEC_PLAYER, false, &HandleJoinStartGuildCommand, "", NULL },
            { NULL, 0, false, NULL, "", NULL }
        };
        return commandTable;
    }

    static bool HandleDevCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            if (handler->GetSession()->GetPlayer()->isDeveloper())
                handler->GetSession()->SendNotification(LANG_DEV_ON);
            else
                handler->GetSession()->SendNotification(LANG_DEV_OFF);
            if (handler->GetSession()->GetPlayer()->isGameMaster())
                handler->GetSession()->SendNotification(LANG_GM_ON);
            else
                handler->GetSession()->SendNotification(LANG_GM_OFF);
            return true;
        }

        std::string argstr = (char*)args;

        if (argstr == "on")
        {
            handler->GetSession()->GetPlayer()->SetDeveloper(true);
            handler->GetSession()->SendNotification(LANG_DEV_ON);
            handler->GetSession()->GetPlayer()->UpdateTriggerVisibility();
            if (handler->GetSession()->GetPlayer()->isGameMaster())
                handler->GetSession()->SendNotification(LANG_GM_ON);
            else
                handler->GetSession()->SendNotification(LANG_GM_OFF);
            return true;
        }

        if (argstr == "off")
        {
            handler->GetSession()->GetPlayer()->SetDeveloper(false);
            handler->GetSession()->SendNotification(LANG_DEV_OFF);
            handler->GetSession()->GetPlayer()->UpdateTriggerVisibility();
            if (handler->GetSession()->GetPlayer()->isGameMaster())
                handler->GetSession()->SendNotification(LANG_GM_ON);
            else
                handler->GetSession()->SendNotification(LANG_GM_OFF);
            return true;
        }

        handler->SendSysMessage(LANG_USE_BOL);
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleGPSCommand(ChatHandler* handler, char const* args)
    {
        WorldObject* object = NULL;
        if (*args)
        {
            uint64 guid = handler->extractGuidFromLink((char*)args);
            if (guid)
                object = (WorldObject*)ObjectAccessor::GetObjectByTypeMask(*handler->GetSession()->GetPlayer(), guid, TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT);

            if (!object)
            {
                handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            object = handler->getSelectedUnit();

            if (!object)
            {
                handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        CellCoord cellCoord = Trinity::ComputeCellCoord(object->GetPositionX(), object->GetPositionY());
        Cell cell(cellCoord);

        uint32 zoneId, areaId;
        object->GetZoneAndAreaId(zoneId, areaId);

        MapEntry const* mapEntry = sMapStore.LookupEntry(object->GetMapId());
        AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zoneId);
        AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(areaId);

        float zoneX = object->GetPositionX();
        float zoneY = object->GetPositionY();

        Map2ZoneCoordinates(zoneX, zoneY, zoneId);

        Map const* map = object->GetMap();
        float groundZ = map->GetHeight(object->GetPhaseMask(), object->GetPositionX(), object->GetPositionY(), MAX_HEIGHT);
        float floorZ = map->GetHeight(object->GetPhaseMask(), object->GetPositionX(), object->GetPositionY(), object->GetPositionZ());

        GridCoord gridCoord = Trinity::ComputeGridCoord(object->GetPositionX(), object->GetPositionY());

        // 63? WHY?
        int gridX = 63 - gridCoord.x_coord;
        int gridY = 63 - gridCoord.y_coord;

        uint32 haveMap = Map::ExistMap(object->GetMapId(), gridX, gridY) ? 1 : 0;
        uint32 haveVMap = Map::ExistVMap(object->GetMapId(), gridX, gridY) ? 1 : 0;

        if (haveVMap)
        {
            if (map->IsOutdoors(object->GetPositionX(), object->GetPositionY(), object->GetPositionZ()))
                handler->PSendSysMessage("You are outdoors");
            else
                handler->PSendSysMessage("You are indoors");
        }
        else
            handler->PSendSysMessage("no VMAP available for area info");

        handler->PSendSysMessage(LANG_MAP_POSITION,
            object->GetMapId(), (mapEntry ? mapEntry->name[handler->GetSessionDbcLocale()] : "<unknown>"),
            zoneId, (zoneEntry ? zoneEntry->area_name[handler->GetSessionDbcLocale()] : "<unknown>"),
            areaId, (areaEntry ? areaEntry->area_name[handler->GetSessionDbcLocale()] : "<unknown>"),
            object->GetPhaseMask(),
            object->GetPositionX(), object->GetPositionY(), object->GetPositionZ(), object->GetOrientation(),
            cell.GridX(), cell.GridY(), cell.CellX(), cell.CellY(), object->GetInstanceId(),
            zoneX, zoneY, groundZ, floorZ, haveMap, haveVMap);

        LiquidData liquidStatus;
        ZLiquidStatus status = map->getLiquidStatus(object->GetPositionX(), object->GetPositionY(), object->GetPositionZ(), MAP_ALL_LIQUIDS, &liquidStatus);

        if (status)
            handler->PSendSysMessage(LANG_LIQUID_STATUS, liquidStatus.level, liquidStatus.depth_level, liquidStatus.entry, liquidStatus.type_flags, status);

        if (Transport* pTransport = object->GetTransport())
        {
            Position objectPosition;
            objectPosition.Relocate(object);
            pTransport->CalculatePassengerPosition(objectPosition.m_positionX, objectPosition.m_positionY,
                                                   objectPosition.m_positionZ, objectPosition.m_orientation);

            handler->PSendSysMessage("Your are on Transport. TransX: %f TransY: %f TransZ: %f TransO: %f", objectPosition.GetPositionX(),
                                     objectPosition.GetPositionY(), objectPosition.GetPositionZ(), objectPosition.GetOrientation());
        }

        return true;
    }

    static bool HandleAuraCommand(ChatHandler* handler, char const* args)
    {
        Unit* target = handler->getSelectedUnit();
        if (!target)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* spellId_str = strtok((char*)args, " ");
        if (!spellId_str)
            return false;

        char* auraStack_str = strtok(NULL, " ");

        // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or Htalent form
        uint32 spellId = handler->extractSpellIdFromLink(spellId_str);
        uint32 auraStack = 1;

        if (auraStack_str)
            auraStack = atoi(auraStack_str);

        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
            target->SetAuraStack(spellId, target, auraStack);

        return true;
    }

    static bool HandleUnAuraCommand(ChatHandler* handler, char const* args)
    {
        Unit* target = handler->getSelectedUnit();
        if (!target)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string argstr = args;
        if (argstr == "all")
        {
            target->RemoveAllAuras();
            return true;
        }

        // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or Htalent form
        uint32 spellId = handler->extractSpellIdFromLink((char*)args);
        if (!spellId)
            return false;

        target->RemoveAurasDueToSpell(spellId);

        return true;
    }
    // Teleport to Player
    static bool HandleAppearCommand(ChatHandler* handler, char const* args)
    {
        Player* target;
        uint64 targetGuid;
        std::string targetName;
        if (!handler->extractPlayerTarget((char*)args, &target, &targetGuid, &targetName))
            return false;

        Player* _player = handler->GetSession()->GetPlayer();
        if (target == _player || targetGuid == _player->GetGUID())
        {
            handler->SendSysMessage(LANG_CANT_TELEPORT_SELF);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (target)
        {
            // check online security
            if (handler->HasLowerSecurity(target, 0))
                return false;

            std::string chrNameLink = handler->playerLink(targetName);

            Map* map = target->GetMap();
            if (map->IsBattlegroundOrArena())
            {
                // only allow if gm mode is on
                if (!_player->isGameMaster())
                {
                    handler->PSendSysMessage(LANG_CANNOT_GO_TO_BG_GM, chrNameLink.c_str());
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                // if both players are in different bgs
                else if (_player->GetBattlegroundId() && _player->GetBattlegroundId() != target->GetBattlegroundId())
                    _player->LeaveBattleground(false); // Note: should be changed so _player gets no Deserter debuff

                // all's well, set bg id
                // when porting out from the bg, it will be reset to 0
                _player->SetBattlegroundId(target->GetBattlegroundId(), target->GetBattlegroundTypeId());
                // remember current position as entry point for return at bg end teleportation
                if (!_player->GetMap()->IsBattlegroundOrArena())
                    _player->SetBattlegroundEntryPoint();
            }
            else if (map->IsDungeon())
            {
                // we have to go to instance, and can go to player only if:
                // 1) we are in his group (either as leader or as member)
                // 2) we are not bound to any group and have GM mode on
                if (_player->GetGroup())
                {
                    // we are in group, we can go only if we are in the player group
                    if (_player->GetGroup() != target->GetGroup())
                    {
                        handler->PSendSysMessage(LANG_CANNOT_GO_TO_INST_PARTY, chrNameLink.c_str());
                        handler->SetSentErrorMessage(true);
                        return false;
                    }
                }
                else
                {
                    // we are not in group, let's verify our GM mode
                    if (!_player->isGameMaster())
                    {
                        handler->PSendSysMessage(LANG_CANNOT_GO_TO_INST_GM, chrNameLink.c_str());
                        handler->SetSentErrorMessage(true);
                        return false;
                    }
                }

                // if the player or the player's group is bound to another instance
                // the player will not be bound to another one
                InstancePlayerBind* bind = _player->GetBoundInstance(target->GetMapId(), target->GetDifficulty(map->IsRaid()));
                if (!bind)
                {
                    Group* group = _player->GetGroup();
                    // if no bind exists, create a solo bind
                    InstanceGroupBind* gBind = group ? group->GetBoundInstance(target) : NULL; // if no bind exists, create a solo bind
                    if (!gBind)
                        if (InstanceSave* save = sInstanceSaveMgr->GetInstanceSave(target->GetInstanceId()))
                            _player->BindToInstance(save, !save->CanReset());
                }

                if (map->IsRaid())
                    _player->SetRaidDifficulty(target->GetRaidDifficulty());
                else
                    _player->SetDungeonDifficulty(target->GetDungeonDifficulty());
            }

            handler->PSendSysMessage(LANG_APPEARING_AT, chrNameLink.c_str());

            // stop flight if need
            if (_player->isInFlight())
            {
                _player->GetMotionMaster()->MovementExpired();
                _player->CleanupAfterTaxiFlight();
            }
            // save only in non-flight case
            else
                _player->SaveRecallPosition();

            // to point to see at target with same orientation
            float x, y, z;
            target->GetContactPoint(_player, x, y, z);

            _player->TeleportTo(target->GetMapId(), x, y, z, _player->GetAngle(target), TELE_TO_GM_MODE);
            _player->SetPhaseMask(target->GetPhaseMask(), true);
        }
        else
        {
            // check offline security
            if (handler->HasLowerSecurity(NULL, targetGuid))
                return false;

            std::string nameLink = handler->playerLink(targetName);

            handler->PSendSysMessage(LANG_APPEARING_AT, nameLink.c_str());

            // to point where player stay (if loaded)
            float x, y, z, o;
            uint32 map;
            bool in_flight;
            if (!Player::LoadPositionFromDB(map, x, y, z, o, in_flight, targetGuid))
                return false;

            // stop flight if need
            if (_player->isInFlight())
            {
                _player->GetMotionMaster()->MovementExpired();
                _player->CleanupAfterTaxiFlight();
            }
            // save only in non-flight case
            else
                _player->SaveRecallPosition();

            _player->TeleportTo(map, x, y, z, _player->GetOrientation());
        }

        return true;
    }
    // Summon Player
    static bool HandleSummonCommand(ChatHandler* handler, char const* args)
    {
        Player* target;
        uint64 targetGuid;
        std::string targetName;
        if (!handler->extractPlayerTarget((char*)args, &target, &targetGuid, &targetName))
            return false;

        Player* _player = handler->GetSession()->GetPlayer();
        if (target == _player || targetGuid == _player->GetGUID())
        {
            handler->PSendSysMessage(LANG_CANT_TELEPORT_SELF);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (target)
        {
            std::string nameLink = handler->playerLink(targetName);
            // check online security
            if (handler->HasLowerSecurity(target, 0))
                return false;

            if (target->IsBeingTeleported())
            {
                handler->PSendSysMessage(LANG_IS_TELEPORTED, nameLink.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }

            Map* map = handler->GetSession()->GetPlayer()->GetMap();

            if (map->IsBattlegroundOrArena())
            {
                // only allow if gm mode is on
                if (!_player->isGameMaster())
                {
                    handler->PSendSysMessage(LANG_CANNOT_GO_TO_BG_GM, nameLink.c_str());
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                // if both players are in different bgs
                else if (target->GetBattlegroundId() && handler->GetSession()->GetPlayer()->GetBattlegroundId() != target->GetBattlegroundId())
                    target->LeaveBattleground(false); // Note: should be changed so target gets no Deserter debuff

                // all's well, set bg id
                // when porting out from the bg, it will be reset to 0
                target->SetBattlegroundId(handler->GetSession()->GetPlayer()->GetBattlegroundId(), handler->GetSession()->GetPlayer()->GetBattlegroundTypeId());
                // remember current position as entry point for return at bg end teleportation
                if (!target->GetMap()->IsBattlegroundOrArena())
                    target->SetBattlegroundEntryPoint();
            }
            else if (map->IsDungeon())
            {
                Map* destMap = target->GetMap();

                if (destMap->Instanceable() && destMap->GetInstanceId() != map->GetInstanceId())
                    target->UnbindInstance(map->GetInstanceId(), target->GetDungeonDifficulty(), true);

                // we are in an instance, and can only summon players in our group with us as leader
                if (!handler->GetSession()->GetPlayer()->GetGroup() || !target->GetGroup() ||
                    (target->GetGroup()->GetLeaderGUID() != handler->GetSession()->GetPlayer()->GetGUID()) ||
                    (handler->GetSession()->GetPlayer()->GetGroup()->GetLeaderGUID() != handler->GetSession()->GetPlayer()->GetGUID()))
                    // the last check is a bit excessive, but let it be, just in case
                {
                    handler->PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST, nameLink.c_str());
                    handler->SetSentErrorMessage(true);
                    return false;
                }
            }

            handler->PSendSysMessage(LANG_SUMMONING, nameLink.c_str(), "");
            if (handler->needReportToTarget(target))
                ChatHandler(target->GetSession()).PSendSysMessage(LANG_SUMMONED_BY, handler->playerLink(_player->GetName()).c_str());

            // stop flight if need
            if (target->isInFlight())
            {
                target->GetMotionMaster()->MovementExpired();
                target->CleanupAfterTaxiFlight();
            }
            // save only in non-flight case
            else
                target->SaveRecallPosition();

            // before GM
            float x, y, z;
            handler->GetSession()->GetPlayer()->GetClosePoint(x, y, z, target->GetObjectSize());
            target->TeleportTo(handler->GetSession()->GetPlayer()->GetMapId(), x, y, z, target->GetOrientation());
            target->SetPhaseMask(handler->GetSession()->GetPlayer()->GetPhaseMask(), true);
        }
        else
        {
            // check offline security
            if (handler->HasLowerSecurity(NULL, targetGuid))
                return false;

            std::string nameLink = handler->playerLink(targetName);

            handler->PSendSysMessage(LANG_SUMMONING, nameLink.c_str(), handler->GetTrinityString(LANG_OFFLINE));

            // in point where GM stay
            Player::SavePositionInDB(handler->GetSession()->GetPlayer()->GetMapId(),
                handler->GetSession()->GetPlayer()->GetPositionX(),
                handler->GetSession()->GetPlayer()->GetPositionY(),
                handler->GetSession()->GetPlayer()->GetPositionZ(),
                handler->GetSession()->GetPlayer()->GetOrientation(),
                handler->GetSession()->GetPlayer()->GetZoneId(),
                targetGuid);
        }

        return true;
    }
    // Summon group of player
    static bool HandleGroupSummonCommand(ChatHandler* handler, char const* args)
    {
        Player* target;
        if (!handler->extractPlayerTarget((char*)args, &target))
            return false;

        // check online security
        if (handler->HasLowerSecurity(target, 0))
            return false;

        Group* group = target->GetGroup();

        std::string nameLink = handler->GetNameLink(target);

        if (!group)
        {
            handler->PSendSysMessage(LANG_NOT_IN_GROUP, nameLink.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        Map* gmMap = handler->GetSession()->GetPlayer()->GetMap();
        bool toInstance = gmMap->Instanceable();

        // we are in instance, and can summon only player in our group with us as lead
        if (toInstance && (
            !handler->GetSession()->GetPlayer()->GetGroup() || (group->GetLeaderGUID() != handler->GetSession()->GetPlayer()->GetGUID()) ||
            (handler->GetSession()->GetPlayer()->GetGroup()->GetLeaderGUID() != handler->GetSession()->GetPlayer()->GetGUID())))
            // the last check is a bit excessive, but let it be, just in case
        {
            handler->SendSysMessage(LANG_CANNOT_SUMMON_TO_INST);
            handler->SetSentErrorMessage(true);
            return false;
        }

        for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* player = itr->getSource();

            if (!player || player == handler->GetSession()->GetPlayer() || !player->GetSession())
                continue;

            // check online security
            if (handler->HasLowerSecurity(player, 0))
                return false;

            std::string plNameLink = handler->GetNameLink(player);

            if (player->IsBeingTeleported() == true)
            {
                handler->PSendSysMessage(LANG_IS_TELEPORTED, plNameLink.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }

            if (toInstance)
            {
                Map* playerMap = player->GetMap();

                if (playerMap->Instanceable() && playerMap->GetInstanceId() != gmMap->GetInstanceId())
                {
                    // cannot summon from instance to instance
                    handler->PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST, plNameLink.c_str());
                    handler->SetSentErrorMessage(true);
                    return false;
                }
            }

            handler->PSendSysMessage(LANG_SUMMONING, plNameLink.c_str(), "");
            if (handler->needReportToTarget(player))
                ChatHandler(player->GetSession()).PSendSysMessage(LANG_SUMMONED_BY, handler->GetNameLink().c_str());

            // stop flight if need
            if (player->isInFlight())
            {
                player->GetMotionMaster()->MovementExpired();
                player->CleanupAfterTaxiFlight();
            }
            // save only in non-flight case
            else
                player->SaveRecallPosition();

            // before GM
            float x, y, z;
            handler->GetSession()->GetPlayer()->GetClosePoint(x, y, z, player->GetObjectSize());
            player->TeleportTo(handler->GetSession()->GetPlayer()->GetMapId(), x, y, z, player->GetOrientation());
        }

        return true;
    }

    static bool HandleCommandsCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->ShowHelpForCommand(handler->getCommandTable(), "");
        return true;
    }

    static bool HandleDieCommand(ChatHandler* handler, char const* /*args*/)
    {
        Unit* target = handler->getSelectedUnit();

        if (!target || !handler->GetSession()->GetPlayer()->GetSelection())
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            if (handler->HasLowerSecurity((Player*)target, 0, false))
                return false;
        }

        if (target->isAlive())
        {
            if (sWorld->getBoolConfig(CONFIG_DIE_COMMAND_MODE))
                handler->GetSession()->GetPlayer()->Kill(target);
            else
                handler->GetSession()->GetPlayer()->DealDamage(target, target->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
        }

        return true;
    }

    static bool HandleReviveCommand(ChatHandler* handler, char const* args)
    {
        Player* target;
        uint64 targetGuid;
        if (!handler->extractPlayerTarget((char*)args, &target, &targetGuid))
            return false;

        if (target)
        {
            target->ResurrectPlayer(!AccountMgr::IsPlayerAccount(target->GetSession()->GetSecurity()) ? 1.0f : 0.5f);
            target->SpawnCorpseBones();
            target->SaveToDB();
        }
        else
            // will resurrected at login without corpse
            sObjectAccessor->ConvertCorpseForPlayer(targetGuid);

        return true;
    }

    static bool HandleDismountCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        // If player is not mounted, so go out :)
        if (!player->IsMounted())
        {
            handler->SendSysMessage(LANG_CHAR_NON_MOUNTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (player->isInFlight())
        {
            handler->SendSysMessage(LANG_YOU_IN_FLIGHT);
            handler->SetSentErrorMessage(true);
            return false;
        }

        player->Dismount();
        player->RemoveAurasByType(SPELL_AURA_MOUNTED);
        return true;
    }

    static bool HandleGUIDCommand(ChatHandler* handler, char const* /*args*/)
    {
        uint64 guid = handler->GetSession()->GetPlayer()->GetSelection();

        if (guid == 0)
        {
            handler->SendSysMessage(LANG_NO_SELECTION);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage(LANG_OBJECT_GUID, GUID_LOPART(guid), GUID_HIPART(guid));
        return true;
    }

    static bool HandleHelpCommand(ChatHandler* handler, char const* args)
    {
        char const* cmd = strtok((char*)args, " ");
        if (!cmd)
        {
            handler->ShowHelpForCommand(handler->getCommandTable(), "help");
            handler->ShowHelpForCommand(handler->getCommandTable(), "");
        }
        else
        {
            if (!handler->ShowHelpForCommand(handler->getCommandTable(), cmd))
                handler->SendSysMessage(LANG_NO_HELP_CMD);
        }

        return true;
    }
    // move item to other slot
    static bool HandleItemMoveCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char const* param1 = strtok((char*)args, " ");
        if (!param1)
            return false;

        char const* param2 = strtok(NULL, " ");
        if (!param2)
            return false;

        uint8 srcSlot = uint8(atoi(param1));
        uint8 dstSlot = uint8(atoi(param2));

        if (srcSlot == dstSlot)
            return true;

        if (!handler->GetSession()->GetPlayer()->IsValidPos(INVENTORY_SLOT_BAG_0, srcSlot, true))
            return false;

        if (!handler->GetSession()->GetPlayer()->IsValidPos(INVENTORY_SLOT_BAG_0, dstSlot, false))
            return false;

        uint16 src = ((INVENTORY_SLOT_BAG_0 << 8) | srcSlot);
        uint16 dst = ((INVENTORY_SLOT_BAG_0 << 8) | dstSlot);

        handler->GetSession()->GetPlayer()->SwapItem(src, dst);

        return true;
    }

    static bool HandleCooldownCommand(ChatHandler* handler, char const* args)
    {
        Player* target = handler->getSelectedPlayer();
        if (!target)
        {
            handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string nameLink = handler->GetNameLink(target);

        if (!*args)
        {
            target->RemoveAllSpellCooldown();
            handler->PSendSysMessage(LANG_REMOVEALL_COOLDOWN, nameLink.c_str());
        }
        else
        {
            // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or Htalent form
            uint32 spellIid = handler->extractSpellIdFromLink((char*)args);
            if (!spellIid)
                return false;

            if (!sSpellMgr->GetSpellInfo(spellIid))
            {
                handler->PSendSysMessage(LANG_UNKNOWN_SPELL, target == handler->GetSession()->GetPlayer() ? handler->GetTrinityString(LANG_YOU) : nameLink.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }

            target->RemoveSpellCooldown(spellIid, true);
            handler->PSendSysMessage(LANG_REMOVE_COOLDOWN, spellIid, target == handler->GetSession()->GetPlayer() ? handler->GetTrinityString(LANG_YOU) : nameLink.c_str());
        }
        return true;
    }

    static bool HandleGetDistanceCommand(ChatHandler* handler, char const* args)
    {
        WorldObject* obj = NULL;

        if (*args)
        {
            uint64 guid = handler->extractGuidFromLink((char*)args);
            if (guid)
                obj = (WorldObject*)ObjectAccessor::GetObjectByTypeMask(*handler->GetSession()->GetPlayer(), guid, TYPEMASK_UNIT|TYPEMASK_GAMEOBJECT);

            if (!obj)
            {
                handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            obj = handler->getSelectedUnit();

            if (!obj)
            {
                handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        handler->PSendSysMessage(LANG_DISTANCE, handler->GetSession()->GetPlayer()->GetDistance(obj), handler->GetSession()->GetPlayer()->GetDistance2d(obj), handler->GetSession()->GetPlayer()->GetExactDist(obj), handler->GetSession()->GetPlayer()->GetExactDist2d(obj));
        return true;
    }
    // Teleport player to last position
    static bool HandleRecallCommand(ChatHandler* handler, char const* args)
    {
        Player* target;
        if (!handler->extractPlayerTarget((char*)args, &target))
            return false;

        // check online security
        if (handler->HasLowerSecurity(target, 0))
            return false;

        if (target->IsBeingTeleported())
        {
            handler->PSendSysMessage(LANG_IS_TELEPORTED, handler->GetNameLink(target).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // stop flight if need
        if (target->isInFlight())
        {
            target->GetMotionMaster()->MovementExpired();
            target->CleanupAfterTaxiFlight();
        }

        target->TeleportTo(target->m_recallMap, target->m_recallX, target->m_recallY, target->m_recallZ, target->m_recallO);
        return true;
    }

    static bool HandleSaveCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        // save GM account without delay and output message
        if (!AccountMgr::IsPlayerAccount(handler->GetSession()->GetSecurity()))
        {
            if (Player* target = handler->getSelectedPlayer())
                target->SaveToDB();
            else
                player->SaveToDB();
            handler->SendSysMessage(LANG_PLAYER_SAVED);
            return true;
        }

        // save if the player has last been saved over 20 seconds ago
        uint32 saveInterval = sWorld->getIntConfig(CONFIG_INTERVAL_SAVE);
        if (saveInterval == 0 || (saveInterval > 20 * IN_MILLISECONDS && player->GetSaveTimer() <= saveInterval - 20 * IN_MILLISECONDS))
            player->SaveToDB();

        return true;
    }

    // Save all players in the world
    static bool HandleSaveAllCommand(ChatHandler* handler, char const* /*args*/)
    {
        sObjectAccessor->SaveAllPlayers();
        handler->SendSysMessage(LANG_PLAYERS_SAVED);
        return true;
    }

    // kick player
    static bool HandleKickPlayerCommand(ChatHandler* handler, char const* args)
    {
        Player* target = NULL;
        std::string playerName;
        if (!handler->extractPlayerTarget((char*)args, &target, NULL, &playerName))
            return false;

        if (handler->GetSession() && target == handler->GetSession()->GetPlayer())
        {
            handler->SendSysMessage(LANG_COMMAND_KICKSELF);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // check online security
        if (handler->HasLowerSecurity(target, 0))
            return false;

        if (sWorld->getBoolConfig(CONFIG_SHOW_KICK_IN_WORLD))
            sWorld->SendWorldText(LANG_COMMAND_KICKMESSAGE, playerName.c_str());
        else
            handler->PSendSysMessage(LANG_COMMAND_KICKMESSAGE, playerName.c_str());

        target->GetSession()->KickPlayer();

        return true;
    }

    static bool HandleUnstuckCommand(ChatHandler* handler, char const* args)
    {
        //No args required for players
        if (handler->GetSession() && AccountMgr::IsPlayerAccount(handler->GetSession()->GetSecurity()))
        {
            // 7355: "Stuck"
            if (Player* player = handler->GetSession()->GetPlayer())
                player->CastSpell(player, 7355, false);
            return true;
        }

        if (!*args)
            return false;

        char* player_str = strtok((char*)args, " ");
        if (!player_str)
            return false;

        std::string location_str = "inn";
        if (char const* loc = strtok(NULL, " "))
            location_str = loc;

        Player* player = NULL;
        if (!handler->extractPlayerTarget(player_str, &player))
            return false;

        if (player->isInFlight() || player->isInCombat())
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(7355);
            if (!spellInfo)
                return false;

            if (Player* caster = handler->GetSession()->GetPlayer())
                Spell::SendCastResult(caster, spellInfo, 0, SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW);

            return false;
        }

        if (location_str == "inn")
        {
            player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ, player->GetOrientation());
            return true;
        }

        if (location_str == "graveyard")
        {
            player->RepopAtGraveyard();
            return true;
        }

        if (location_str == "startzone")
        {
            player->TeleportTo(player->GetStartPosition());
            return true;
        }

        //Not a supported argument
        return false;

    }

    static bool HandleLinkGraveCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char* px = strtok((char*)args, " ");
        if (!px)
            return false;

        uint32 graveyardId = uint32(atoi(px));

        uint32 team;

        char* px2 = strtok(NULL, " ");

        if (!px2)
            team = 0;
        else if (strncmp(px2, "horde", 6) == 0)
            team = HORDE;
        else if (strncmp(px2, "alliance", 9) == 0)
            team = ALLIANCE;
        else
            return false;

        WorldSafeLocsEntry const* graveyard = sWorldSafeLocsStore.LookupEntry(graveyardId);

        if (!graveyard)
        {
            handler->PSendSysMessage(LANG_COMMAND_GRAVEYARDNOEXIST, graveyardId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        uint32 zoneId = player->GetZoneId();

        AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(zoneId);
        if (!areaEntry || areaEntry->zone !=0)
        {
            handler->PSendSysMessage(LANG_COMMAND_GRAVEYARDWRONGZONE, graveyardId, zoneId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (sObjectMgr->AddGraveYardLink(graveyardId, zoneId, team))
            handler->PSendSysMessage(LANG_COMMAND_GRAVEYARDLINKED, graveyardId, zoneId);
        else
            handler->PSendSysMessage(LANG_COMMAND_GRAVEYARDALRLINKED, graveyardId, zoneId);

        return true;
    }

    static bool HandleNearGraveCommand(ChatHandler* handler, char const* args)
    {
        uint32 team;

        size_t argStr = strlen(args);

        if (!*args)
            team = 0;
        else if (strncmp((char*)args, "horde", argStr) == 0)
            team = HORDE;
        else if (strncmp((char*)args, "alliance", argStr) == 0)
            team = ALLIANCE;
        else
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        uint32 zone_id = player->GetZoneId();

        WorldSafeLocsEntry const* graveyard = sObjectMgr->GetClosestGraveYard(
            player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetMapId(), team);

        if (graveyard)
        {
            uint32 graveyardId = graveyard->ID;

            GraveYardData const* data = sObjectMgr->FindGraveYardData(graveyardId, zone_id);
            if (!data)
            {
                handler->PSendSysMessage(LANG_COMMAND_GRAVEYARDERROR, graveyardId);
                handler->SetSentErrorMessage(true);
                return false;
            }

            team = data->team;

            std::string team_name = handler->GetTrinityString(LANG_COMMAND_GRAVEYARD_NOTEAM);

            if (team == 0)
                team_name = handler->GetTrinityString(LANG_COMMAND_GRAVEYARD_ANY);
            else if (team == HORDE)
                team_name = handler->GetTrinityString(LANG_COMMAND_GRAVEYARD_HORDE);
            else if (team == ALLIANCE)
                team_name = handler->GetTrinityString(LANG_COMMAND_GRAVEYARD_ALLIANCE);

            handler->PSendSysMessage(LANG_COMMAND_GRAVEYARDNEAREST, graveyardId, team_name.c_str(), zone_id);
        }
        else
        {
            std::string team_name;

            if (team == 0)
                team_name = handler->GetTrinityString(LANG_COMMAND_GRAVEYARD_ANY);
            else if (team == HORDE)
                team_name = handler->GetTrinityString(LANG_COMMAND_GRAVEYARD_HORDE);
            else if (team == ALLIANCE)
                team_name = handler->GetTrinityString(LANG_COMMAND_GRAVEYARD_ALLIANCE);

            if (team == ~uint32(0))
                handler->PSendSysMessage(LANG_COMMAND_ZONENOGRAVEYARDS, zone_id);
            else
                handler->PSendSysMessage(LANG_COMMAND_ZONENOGRAFACTION, zone_id, team_name.c_str());
        }

        return true;
    }

    static bool HandleShowAreaCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        int32 area = GetAreaFlagByAreaID(atoi((char*)args));
        int32 offset = area / 32;
        uint32 val = uint32((1 << (area % 32)));

        if (area<0 || offset >= PLAYER_EXPLORED_ZONES_SIZE)
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 currFields = playerTarget->GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);
        playerTarget->SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset, uint32((currFields | val)));

        handler->SendSysMessage(LANG_EXPLORE_AREA);
        return true;
    }

    static bool HandleHideAreaCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        int32 area = GetAreaFlagByAreaID(atoi((char*)args));
        int32 offset = area / 32;
        uint32 val = uint32((1 << (area % 32)));

        if (area < 0 || offset >= PLAYER_EXPLORED_ZONES_SIZE)
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 currFields = playerTarget->GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);
        playerTarget->SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset, uint32((currFields ^ val)));

        handler->SendSysMessage(LANG_UNEXPLORE_AREA);
        return true;
    }

    static bool HandleAddItemCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        uint32 itemId = 0;

        if (args[0] == '[') // [name] manual form
        {
            char const* itemNameStr = strtok((char*)args, "]");

            if (itemNameStr && itemNameStr[0])
            {
                std::string itemName = itemNameStr+1;
                WorldDatabase.EscapeString(itemName);

                PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_ITEM_TEMPLATE_BY_NAME);
                stmt->setString(0, itemName);
                PreparedQueryResult result = WorldDatabase.Query(stmt);

                if (!result)
                {
                    handler->PSendSysMessage(LANG_COMMAND_COULDNOTFIND, itemNameStr+1);
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                itemId = result->Fetch()->GetUInt32();
            }
            else
                return false;
        }
        else // item_id or [name] Shift-click form |color|Hitem:item_id:0:0:0|h[name]|h|r
        {
            char const* id = handler->extractKeyFromLink((char*)args, "Hitem");
            if (!id)
                return false;
            itemId = uint32(atol(id));
        }

        char const* ccount = strtok(NULL, " ");

        int32 count = 1;

        if (ccount)
            count = strtol(ccount, NULL, 10);

        if (count == 0)
            count = 1;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        sLog->outDebug(LOG_FILTER_NONE, handler->GetTrinityString(LANG_ADDITEM), itemId, count);

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            handler->PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Subtract
        if (count < 0)
        {
            playerTarget->DestroyItemCount(itemId, -count, true, false);
            handler->PSendSysMessage(LANG_REMOVEITEM, itemId, -count, handler->GetNameLink(playerTarget).c_str());
            return true;
        }

        // Adding items
        uint32 noSpaceForCount = 0;

        // check space and find places
        ItemPosCountVec dest;
        InventoryResult msg = playerTarget->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpaceForCount);
        if (msg != EQUIP_ERR_OK) // convert to possible store amount
            count -= noSpaceForCount;

        if (count == 0 || dest.empty()) // can't add any
        {
            handler->PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, noSpaceForCount);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 securityTarget = 0;
        uint32 securityHandler = 0;

        if (WorldSession *session = playerTarget->GetSession())
            securityTarget = session->GetSecurity();

        if (WorldSession *session = handler->GetSession())
            securityHandler = session->GetSecurity();

        if (AccountMgr::IsVIPorPlayer(securityTarget))
        {
            switch (itemTemplate->Quality)
            {
                case ITEM_QUALITY_EPIC:
                {
                    if (securityHandler < SEC_ADMINISTRATOR && itemTemplate->ItemLevel > 245)
                    {
                        handler->PSendSysMessage("Diese Handlung ist Euch nicht erlaubt. Sollte es wichtig sein, wendet euch an einen Vorgesetzten.");
                        handler->SetSentErrorMessage(true);
                        return false;
                    }
                    break;
                }

                case ITEM_QUALITY_LEGENDARY:
                case ITEM_QUALITY_ARTIFACT:
                {
                    handler->PSendSysMessage("Diese Handlung ist Euch nicht erlaubt, und kann auch von keinem durchgefuehrt werden.");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
            }
        }

        Item* item = playerTarget->StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));

        // remove binding (let GM give it to another player later) - only admin and above
        if (player == playerTarget && securityHandler > SEC_GAMEMASTER)
            for (ItemPosCountVec::const_iterator itr = dest.begin(); itr != dest.end(); ++itr)
                if (Item* item1 = player->GetItemByPos(itr->pos))
                    item1->SetBinding(false);

        if (count > 0 && item)
        {
            player->SendNewItem(item, count, false, true);
            if (player != playerTarget)
                playerTarget->SendNewItem(item, count, true, false);
        }

        if (noSpaceForCount > 0)
            handler->PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, noSpaceForCount);

        return true;
    }

    static bool HandleAddItemSetCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char const* id = handler->extractKeyFromLink((char*)args, "Hitemset"); // number or [name] Shift-click form |color|Hitemset:itemset_id|h[name]|h|r
        if (!id)
            return false;

        uint32 itemSetId = atol(id);

        // prevent generation all items with itemset field value '0'
        if (itemSetId == 0)
        {
            handler->PSendSysMessage(LANG_NO_ITEMS_FROM_ITEMSET_FOUND, itemSetId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        sLog->outDebug(LOG_FILTER_NONE, handler->GetTrinityString(LANG_ADDITEMSET), itemSetId);

        bool found = false;
        ItemTemplateContainer const* its = sObjectMgr->GetItemTemplateStore();
        for (ItemTemplateContainer::const_iterator itr = its->begin(); itr != its->end(); ++itr)
        {
            if (itr->second.ItemSet == itemSetId)
            {
                found = true;
                ItemPosCountVec dest;
                InventoryResult msg = playerTarget->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itr->second.ItemId, 1);
                if (msg == EQUIP_ERR_OK)
                {
                    Item* item = playerTarget->StoreNewItem(dest, itr->second.ItemId, true);

                    // remove binding (let GM give it to another player later)
                    if (player == playerTarget && AccountMgr::IsAdminAccount(handler->GetSession()->GetSecurity()))
                        item->SetBinding(false);

                    player->SendNewItem(item, 1, false, true);
                    if (player != playerTarget)
                        playerTarget->SendNewItem(item, 1, true, false);
                }
                else
                {
                    player->SendEquipError(msg, NULL, NULL, itr->second.ItemId);
                    handler->PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itr->second.ItemId, 1);
                }
            }
        }

        if (!found)
        {
            handler->PSendSysMessage(LANG_NO_ITEMS_FROM_ITEMSET_FOUND, itemSetId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

    static bool HandleBankCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->GetSession()->SendShowBank(handler->GetSession()->GetPlayer()->GetGUID());
        return true;
    }

    static bool HandleChangeWeather(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        // Weather is OFF
        if (!sWorld->getBoolConfig(CONFIG_WEATHER))
        {
            handler->SendSysMessage(LANG_WEATHER_DISABLED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // *Change the weather of a cell
        char const* px = strtok((char*)args, " ");
        char const* py = strtok(NULL, " ");

        if (!px || !py)
            return false;

        uint32 type = uint32(atoi(px)); //0 to 3, 0: fine, 1: rain, 2: snow, 3: sand
        float grade = float(atof(py)); //0 to 1, sending -1 is instand good weather

        Player* player = handler->GetSession()->GetPlayer();
        uint32 zoneid = player->GetZoneId();

        Weather* weather = WeatherMgr::FindWeather(zoneid);

        if (!weather)
            weather = WeatherMgr::AddWeather(zoneid);
        if (!weather)
        {
            handler->SendSysMessage(LANG_NO_WEATHER);
            handler->SetSentErrorMessage(true);
            return false;
        }

        weather->SetWeather(WeatherType(type), grade);

        return true;
    }


    static bool HandleMaxSkillCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* SelectedPlayer = handler->getSelectedPlayer();
        if (!SelectedPlayer)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // each skills that have max skill value dependent from level seted to current level max skill value
        SelectedPlayer->UpdateSkillsToMaxSkillsForLevel();
        return true;
    }

    static bool HandleSetSkillCommand(ChatHandler* handler, char const* args)
    {
        // number or [name] Shift-click form |color|Hskill:skill_id|h[name]|h|r
        char const* skillStr = handler->extractKeyFromLink((char*)args, "Hskill");
        if (!skillStr)
            return false;

        char const* levelStr = strtok(NULL, " ");
        if (!levelStr)
            return false;

        char const* maxPureSkill = strtok(NULL, " ");

        int32 skill = atoi(skillStr);
        if (skill <= 0)
        {
            handler->PSendSysMessage(LANG_INVALID_SKILL_ID, skill);
            handler->SetSentErrorMessage(true);
            return false;
        }

        int32 level = uint32(atol(levelStr));

        Player* target = handler->getSelectedPlayer();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        SkillLineEntry const* skillLine = sSkillLineStore.LookupEntry(skill);
        if (!skillLine)
        {
            handler->PSendSysMessage(LANG_INVALID_SKILL_ID, skill);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string tNameLink = handler->GetNameLink(target);

        if (!target->GetSkillValue(skill))
        {
            handler->PSendSysMessage(LANG_SET_SKILL_ERROR, tNameLink.c_str(), skill, skillLine->name[handler->GetSessionDbcLocale()]);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint16 max = maxPureSkill ? atol (maxPureSkill) : target->GetPureMaxSkillValue(skill);

        if (level <= 0 || level > max || max <= 0)
            return false;

        target->SetSkill(skill, target->GetSkillStep(skill), level, max);
        handler->PSendSysMessage(LANG_SET_SKILL, skill, skillLine->name[handler->GetSessionDbcLocale()], tNameLink.c_str(), level, max);

        return true;
    }
    // show info of player
    static bool HandlePInfoCommand(ChatHandler* handler, char const* args)
    {
        Player* target;
        uint64 target_guid;
        std::string target_name;

        uint32 parseGUID = MAKE_NEW_GUID(atol((char*)args), 0, HIGHGUID_PLAYER);

        if (sObjectMgr->GetPlayerNameByGUID(parseGUID, target_name))
        {
            target = sObjectMgr->GetPlayerByLowGUID(parseGUID);
            target_guid = parseGUID;
        }
        else if (!handler->extractPlayerTarget((char*)args, &target, &target_guid, &target_name))
            return false;

        uint32 accId = 0;
        uint32 money = 0;
        uint32 total_player_time = 0;
        uint8 level = 0;
        uint32 latency = 0;
        uint8 race;
        uint8 Class;
        int64 muteTime = 0;
        int64 banTime = -1;
        uint32 mapId;
        uint32 areaId;
        uint32 phase = 0;

        // get additional information from Player object
        if (target)
        {
            // check online security
            if (handler->HasLowerSecurity(target, 0))
            return false;

            accId = target->GetSession()->GetAccountId();
            money = target->GetMoney();
            total_player_time = target->GetTotalPlayedTime();
            level = target->getLevel();
            latency = target->GetSession()->GetLatency();
            race = target->getRace();
            Class = target->getClass();
            muteTime = target->GetSession()->m_muteTime;
            mapId = target->GetMapId();
            areaId = target->GetAreaId();
            phase = target->GetPhaseMask();
        }
        // get additional information from DB
        else
        {
            // check offline security
            if (handler->HasLowerSecurity(NULL, target_guid))
            return false;

            // 0 1 2 3 4 5 6 7
            QueryResult result = CharacterDatabase.PQuery("SELECT totaltime, level, money, account, race, class, map, zone FROM characters "
            "WHERE guid = '%u'", GUID_LOPART(target_guid));
            if (!result)
            return false;

            Field* fields = result->Fetch();
            total_player_time = fields[0].GetUInt32();
            level = fields[1].GetUInt32();
            money = fields[2].GetUInt32();
            accId = fields[3].GetUInt32();
            race = fields[4].GetUInt8();
            Class = fields[5].GetUInt8();
            mapId = fields[6].GetUInt16();
            areaId = fields[7].GetUInt16();
        }

        std::string username = handler->GetTrinityString(LANG_ERROR);
        std::string email = handler->GetTrinityString(LANG_ERROR);
        std::string last_ip = handler->GetTrinityString(LANG_ERROR);
        uint32 security = 0;
        std::string last_login = handler->GetTrinityString(LANG_ERROR);

        QueryResult result = LoginDatabase.PQuery("SELECT a.username, aa.gmlevel, a.email, a.last_ip, a.last_login, a.mutetime "
        "FROM account a "
        "LEFT JOIN account_access aa "
        "ON (a.id = aa.id AND (aa.RealmID = -1 OR aa.RealmID = %u)) "
        "WHERE a.id = '%u'", realmID, accId);
        if (result)
        {
            Field* fields = result->Fetch();
            username = fields[0].GetString();
            security = fields[1].GetUInt32();
            email = fields[2].GetString();
            muteTime = fields[5].GetInt64();

            if (email.empty())
                email = "-";

            if (!handler->GetSession() || handler->GetSession()->GetSecurity() >= AccountTypes(security))
            {
                last_ip = fields[3].GetString();
                last_login = fields[4].GetString();
            }
            else
            {
                last_ip = "-";
                last_login = "-";
            }
        }

        std::string nameLink = handler->playerLink(target_name);

        handler->PSendSysMessage(LANG_PINFO_ACCOUNT, (target?"":handler->GetTrinityString(LANG_OFFLINE)), nameLink.c_str(), GUID_LOPART(target_guid), username.c_str(), accId, email.c_str(), security, last_ip.c_str(), last_login.c_str(), latency);

        std::string bannedby = "unknown";
        std::string banreason = "";
        if (QueryResult result2 = LoginDatabase.PQuery("SELECT unbandate, bandate = unbandate, bannedby, banreason FROM account_banned "
        "WHERE id = '%u' AND active ORDER BY bandate ASC LIMIT 1", accId))
        {
            Field* fields = result2->Fetch();
            banTime = fields[1].GetBool() ? 0 : fields[0].GetUInt64();
            bannedby = fields[2].GetString();
            banreason = fields[3].GetString();
        }
        else if (QueryResult result3 = CharacterDatabase.PQuery("SELECT unbandate, bandate = unbandate, bannedby, banreason FROM character_banned "
        "WHERE guid = '%u' AND active ORDER BY bandate ASC LIMIT 1", GUID_LOPART(target_guid)))
        {
            Field* fields = result3->Fetch();
            banTime = fields[1].GetBool() ? 0 : fields[0].GetUInt64();
            bannedby = fields[2].GetString();
            banreason = fields[3].GetString();
        }

        if (muteTime > 0)
            handler->PSendSysMessage(LANG_PINFO_MUTE, secsToTimeString(muteTime - time(NULL), true).c_str());

        if (banTime >= 0)
            handler->PSendSysMessage(LANG_PINFO_BAN, banTime > 0 ? secsToTimeString(banTime - time(NULL), true).c_str() : "permanently", bannedby.c_str(), banreason.c_str());

        std::string race_s, Class_s;
        switch (race)
        {
            case RACE_HUMAN: race_s = "Human"; break;
            case RACE_ORC: race_s = "Orc"; break;
            case RACE_DWARF: race_s = "Dwarf"; break;
            case RACE_NIGHTELF: race_s = "Night Elf"; break;
            case RACE_UNDEAD_PLAYER: race_s = "Undead"; break;
            case RACE_TAUREN: race_s = "Tauren"; break;
            case RACE_GNOME: race_s = "Gnome"; break;
            case RACE_TROLL: race_s = "Troll"; break;
            case RACE_BLOODELF: race_s = "Blood Elf"; break;
            case RACE_DRAENEI: race_s = "Draenei"; break;
        }
        switch (Class)
        {
            case CLASS_WARRIOR: Class_s = "Warrior"; break;
            case CLASS_PALADIN: Class_s = "Paladin"; break;
            case CLASS_HUNTER: Class_s = "Hunter"; break;
            case CLASS_ROGUE: Class_s = "Rogue"; break;
            case CLASS_PRIEST: Class_s = "Priest"; break;
            case CLASS_DEATH_KNIGHT: Class_s = "Death Knight"; break;
            case CLASS_SHAMAN: Class_s = "Shaman"; break;
            case CLASS_MAGE: Class_s = "Mage"; break;
            case CLASS_WARLOCK: Class_s = "Warlock"; break;
            case CLASS_DRUID: Class_s = "Druid"; break;
        }

        std::string timeStr = secsToTimeString(total_player_time, true, true);
        uint32 gold = money /GOLD;
        uint32 silv = (money % GOLD) / SILVER;
        uint32 copp = (money % GOLD) % SILVER;
        handler->PSendSysMessage(LANG_PINFO_LEVEL, race_s.c_str(), Class_s.c_str(), timeStr.c_str(), level, gold, silv, copp);

        // Add map, zone, subzone and phase to output
        int locale = handler->GetSessionDbcLocale();
        std::string areaName = "<unknown>";
        std::string zoneName = "";

        MapEntry const* map = sMapStore.LookupEntry(mapId);

        AreaTableEntry const* area = GetAreaEntryByAreaID(areaId);
        if (area)
        {
            areaName = area->area_name[locale];

            AreaTableEntry const* zone = GetAreaEntryByAreaID(area->zone);

            if (zone)
            zoneName = zone->area_name[locale];
        }

        if (target)
        {
            if (!zoneName.empty())
                handler->PSendSysMessage(LANG_PINFO_MAP_ONLINE, map->name[locale], zoneName.c_str(), areaName.c_str(), phase);
            else
                handler->PSendSysMessage(LANG_PINFO_MAP_ONLINE, map->name[locale], areaName.c_str(), "<unknown>", phase);
        }
        else
        {
            handler->PSendSysMessage(LANG_PINFO_MAP_OFFLINE, map->name[locale], areaName.c_str());
        }

        result = LoginDatabase.PQuery ("SELECT id,username FROM account WHERE last_ip = '%s'", last_ip.c_str ());
        if (!result)
        {
            handler->PSendSysMessage(LANG_NO_PLAYERS_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }
        else
        {
            int i = 0;
            int32 limit = -1;
            uint32 count = 0;
            uint32 maxResults = sWorld->getIntConfig(CONFIG_MAX_RESULTS_LOOKUP_COMMANDS);
            do
            {
                if (maxResults && count++ == maxResults)
                {
                    handler->PSendSysMessage(LANG_COMMAND_LOOKUP_MAX_RESULTS, maxResults);
                    return true;
                }
                Field* fields = result->Fetch();
                uint32 acc_id = fields[0].GetUInt32();
                std::string acc_name = fields[1].GetString();

                QueryResult chars = CharacterDatabase.PQuery("SELECT guid,name,online FROM characters WHERE account = '%u'", acc_id);
                if (chars)
                {
                    handler->PSendSysMessage(LANG_LOOKUP_PLAYER_ACCOUNT,acc_name.c_str(),acc_id);
                    uint64 guid = 0;
                    std::string name;
                    bool online = false;
                    do
                    {
                        Field* charfields = chars->Fetch();
                        guid = charfields[0].GetUInt64();
                        name = charfields[1].GetString();
                        online = charfields[2].GetBool();

                        handler->PSendSysMessage(online ? LANG_LOOKUP_PLAYER_CHARACTER_ONLINE : LANG_LOOKUP_PLAYER_CHARACTER,name.c_str(),guid);
                        ++i;
                    } while (chars->NextRow() && (limit == -1 || i < limit));
                }
            } while (result->NextRow());
        }

        return true;
    }

    static bool HandleRespawnCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        // accept only explicitly selected target (not implicitly self targeting case)
        Unit* target = handler->getSelectedUnit();
        if (player->GetSelection() && target)
        {
            if (target->GetTypeId() != TYPEID_UNIT || target->isPet())
            {
                handler->SendSysMessage(LANG_SELECT_CREATURE);
                handler->SetSentErrorMessage(true);
                return false;
            }

            if (target->isDead())
                target->ToCreature()->Respawn();
            return true;
        }

        CellCoord p(Trinity::ComputeCellCoord(player->GetPositionX(), player->GetPositionY()));
        Cell cell(p);
        cell.SetNoCreate();

        Trinity::RespawnDo u_do;
        Trinity::WorldObjectWorker<Trinity::RespawnDo> worker(player, u_do);

        TypeContainerVisitor<Trinity::WorldObjectWorker<Trinity::RespawnDo>, GridTypeMapContainer > obj_worker(worker);
        cell.Visit(p, obj_worker, *player->GetMap(), *player, player->GetGridActivationRange());

        return true;
    }
    // mute player for some times
    static bool HandleMuteCommand(ChatHandler* handler, char const* args)
    {
        char* nameStr;
        char* delayStr;
        handler->extractOptFirstArg((char*)args, &nameStr, &delayStr);
        if (!delayStr)
            return false;

        char const* muteReason = strtok(NULL, "\r");
        std::string muteReasonStr = "No reason";
        if (muteReason != NULL)
            muteReasonStr = muteReason;

        Player* target;
        uint64 targetGuid;
        std::string targetName;
        if (!handler->extractPlayerTarget(nameStr, &target, &targetGuid, &targetName))
            return false;

        uint32 accountId = target ? target->GetSession()->GetAccountId() : sObjectMgr->GetPlayerAccountIdByGUID(targetGuid);

        // find only player from same account if any
        if (!target)
            if (WorldSession* session = sWorld->FindSession(accountId))
                target = session->GetPlayer();

        uint32 notSpeakTime = uint32(atoi(delayStr));

        // must have strong lesser security level
        if (handler->HasLowerSecurity (target, targetGuid, true))
            return false;

        PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_MUTE_TIME);
        std::string muteBy = "";
        if (handler->GetSession())
            muteBy = handler->GetSession()->GetPlayerName();
        else
            muteBy = "Console";

        if (target)
        {
            // Target is online, mute will be in effect right away.
            int64 muteTime = time(NULL) + notSpeakTime * MINUTE;
            target->GetSession()->m_muteTime = muteTime;
            stmt->setInt64(0, muteTime);
            ChatHandler(target->GetSession()).PSendSysMessage(LANG_YOUR_CHAT_DISABLED, notSpeakTime, muteBy.c_str(), muteReasonStr.c_str());
        }
        else
        {
            // Target is offline, mute will be in effect starting from the next login.
            int32 muteTime = -int32(notSpeakTime * MINUTE);
            stmt->setInt64(0, muteTime);
        }

        stmt->setString(1, muteReasonStr.c_str());
        stmt->setString(2, muteBy.c_str());
        stmt->setUInt32(3, accountId);
        LoginDatabase.Execute(stmt);
        std::string nameLink = handler->playerLink(targetName);

        handler->PSendSysMessage(target ? LANG_YOU_DISABLE_CHAT : LANG_COMMAND_DISABLE_CHAT_DELAYED, nameLink.c_str(), notSpeakTime, muteReasonStr.c_str());

        return true;
    }

    // unmute player
    static bool HandleUnmuteCommand(ChatHandler* handler, char const* args)
    {
        Player* target;
        uint64 targetGuid;
        std::string targetName;
        if (!handler->extractPlayerTarget((char*)args, &target, &targetGuid, &targetName))
            return false;

        uint32 accountId = target ? target->GetSession()->GetAccountId() : sObjectMgr->GetPlayerAccountIdByGUID(targetGuid);

        // find only player from same account if any
        if (!target)
            if (WorldSession* session = sWorld->FindSession(accountId))
                target = session->GetPlayer();

        // must have strong lesser security level
        if (handler->HasLowerSecurity (target, targetGuid, true))
            return false;

        if (target)
        {
            if (target->CanSpeak())
            {
                handler->SendSysMessage(LANG_CHAT_ALREADY_ENABLED);
                handler->SetSentErrorMessage(true);
                return false;
            }

            target->GetSession()->m_muteTime = 0;
        }

        PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_MUTE_TIME);
        stmt->setInt64(0, 0);
        stmt->setString(1, "");
        stmt->setString(2, "");
        stmt->setUInt32(3, accountId);
        LoginDatabase.Execute(stmt);

        if (target)
            ChatHandler(target->GetSession()).PSendSysMessage(LANG_YOUR_CHAT_ENABLED);

        std::string nameLink = handler->playerLink(targetName);

        handler->PSendSysMessage(LANG_YOU_ENABLE_CHAT, nameLink.c_str());

        return true;
    }


    static bool HandleMovegensCommand(ChatHandler* handler, char const* /*args*/)
    {
        Unit* unit = handler->getSelectedUnit();
        if (!unit)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage(LANG_MOVEGENS_LIST, (unit->GetTypeId() == TYPEID_PLAYER ? "Player" : "Creature"), unit->GetGUIDLow());

        MotionMaster* motionMaster = unit->GetMotionMaster();
        float x, y, z;
        motionMaster->GetDestination(x, y, z);

        for (uint8 i = 0; i < MAX_MOTION_SLOT; ++i)
        {
            MovementGenerator* movementGenerator = motionMaster->GetMotionSlot(i);
            if (!movementGenerator)
            {
                handler->SendSysMessage("Empty");
                continue;
            }

            switch (movementGenerator->GetMovementGeneratorType())
            {
                case IDLE_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_IDLE);
                    break;
                case RANDOM_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_RANDOM);
                    break;
                case WAYPOINT_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_WAYPOINT);
                    break;
                case ANIMAL_RANDOM_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_ANIMAL_RANDOM);
                    break;
                case CONFUSED_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_CONFUSED);
                    break;
                case CHASE_MOTION_TYPE:
                {
                    Unit* target = NULL;
                    if (unit->GetTypeId() == TYPEID_PLAYER)
                        target = static_cast<ChaseMovementGenerator<Player> const*>(movementGenerator)->GetTarget();
                    else
                        target = static_cast<ChaseMovementGenerator<Creature> const*>(movementGenerator)->GetTarget();

                    if (!target)
                        handler->SendSysMessage(LANG_MOVEGENS_CHASE_NULL);
                    else if (target->GetTypeId() == TYPEID_PLAYER)
                        handler->PSendSysMessage(LANG_MOVEGENS_CHASE_PLAYER, target->GetName(), target->GetGUIDLow());
                    else
                        handler->PSendSysMessage(LANG_MOVEGENS_CHASE_CREATURE, target->GetName(), target->GetGUIDLow());
                    break;
                }
                case FOLLOW_MOTION_TYPE:
                {
                    Unit* target = NULL;
                    if (unit->GetTypeId() == TYPEID_PLAYER)
                        target = static_cast<FollowMovementGenerator<Player> const*>(movementGenerator)->GetTarget();
                    else
                        target = static_cast<FollowMovementGenerator<Creature> const*>(movementGenerator)->GetTarget();

                    if (!target)
                        handler->SendSysMessage(LANG_MOVEGENS_FOLLOW_NULL);
                    else if (target->GetTypeId() == TYPEID_PLAYER)
                        handler->PSendSysMessage(LANG_MOVEGENS_FOLLOW_PLAYER, target->GetName(), target->GetGUIDLow());
                    else
                        handler->PSendSysMessage(LANG_MOVEGENS_FOLLOW_CREATURE, target->GetName(), target->GetGUIDLow());
                    break;
                }
                case HOME_MOTION_TYPE:
                {
                    if (unit->GetTypeId() == TYPEID_UNIT)
                        handler->PSendSysMessage(LANG_MOVEGENS_HOME_CREATURE, x, y, z);
                    else
                        handler->SendSysMessage(LANG_MOVEGENS_HOME_PLAYER);
                    break;
                }
                case FLIGHT_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_FLIGHT);
                    break;
                case POINT_MOTION_TYPE:
                {
                    handler->PSendSysMessage(LANG_MOVEGENS_POINT, x, y, z);
                    break;
                }
                case FLEEING_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_FEAR);
                    break;
                case DISTRACT_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_DISTRACT);
                    break;
                case EFFECT_MOTION_TYPE:
                    handler->SendSysMessage(LANG_MOVEGENS_EFFECT);
                    break;
                default:
                    handler->PSendSysMessage(LANG_MOVEGENS_UNKNOWN, movementGenerator->GetMovementGeneratorType());
                    break;
            }
        }
        return true;
    }
    /*
ComeToMe command REQUIRED for 3rd party scripting library to have access to PointMovementGenerator
Without this function 3rd party scripting library will get linking errors (unresolved external)
when attempting to use the PointMovementGenerator
*/
    static bool HandleComeToMeCommand(ChatHandler* handler, char const* args)
    {
        char const* newFlagStr = strtok((char*)args, " ");
        if (!newFlagStr)
            return false;

        Creature* caster = handler->getSelectedCreature();
        if (!caster)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        caster->GetMotionMaster()->MovePoint(0, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

        return true;
    }

    static bool HandleDamageCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Unit* target = handler->getSelectedUnit();
        if (!target || !handler->GetSession()->GetPlayer()->GetSelection())
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            if (handler->HasLowerSecurity((Player*)target, 0, false))
                return false;
        }

        if (!target->isAlive())
            return true;

        char* damageStr = strtok((char*)args, " ");
        if (!damageStr)
            return false;

        int32 damage_int = atoi((char*)damageStr);
        if (damage_int <= 0)
            return true;

        uint32 damage = damage_int;

        char* schoolStr = strtok((char*)NULL, " ");

        // flat melee damage without resistence/etc reduction
        if (!schoolStr)
        {
            handler->GetSession()->GetPlayer()->DealDamage(target, damage, NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            if (target != handler->GetSession()->GetPlayer())
                handler->GetSession()->GetPlayer()->SendAttackStateUpdate (HITINFO_AFFECTS_VICTIM, target, 1, SPELL_SCHOOL_MASK_NORMAL, damage, 0, 0, VICTIMSTATE_HIT, 0);
            return true;
        }

        uint32 school = schoolStr ? atoi((char*)schoolStr) : SPELL_SCHOOL_NORMAL;
        if (school >= MAX_SPELL_SCHOOL)
            return false;

        SpellSchoolMask schoolmask = SpellSchoolMask(1 << school);

        if (Unit::IsDamageReducedByArmor(schoolmask))
            damage = handler->GetSession()->GetPlayer()->CalcArmorReducedDamage(target, damage, NULL, BASE_ATTACK);

        char* spellStr = strtok((char*)NULL, " ");

        // melee damage by specific school
        if (!spellStr)
        {
            uint32 absorb = 0;
            uint32 resist = 0;

            handler->GetSession()->GetPlayer()->CalcAbsorbResist(target, schoolmask, SPELL_DIRECT_DAMAGE, damage, &absorb, &resist);

            if (damage <= absorb + resist)
                return true;

            damage -= absorb + resist;

            handler->GetSession()->GetPlayer()->DealDamageMods(target, damage, &absorb);
            handler->GetSession()->GetPlayer()->DealDamage(target, damage, NULL, DIRECT_DAMAGE, schoolmask, NULL, false);
            handler->GetSession()->GetPlayer()->SendAttackStateUpdate (HITINFO_AFFECTS_VICTIM, target, 1, schoolmask, damage, absorb, resist, VICTIMSTATE_HIT, 0);
            return true;
        }

        // non-melee damage

        // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or Htalent form
        uint32 spellid = handler->extractSpellIdFromLink((char*)args);
        if (!spellid || !sSpellMgr->GetSpellInfo(spellid))
            return false;

        handler->GetSession()->GetPlayer()->SpellNonMeleeDamageLog(target, spellid, damage);
        return true;
    }

    static bool HandleCombatStopCommand(ChatHandler* handler, char const* args)
    {
        Player* target = NULL;

        if (args && strlen(args) > 0)
        {
            target = sObjectAccessor->FindPlayerByName(args);
            if (!target)
            {
                handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        if (!target)
        {
            if (!handler->extractPlayerTarget((char*)args, &target))
                return false;
        }

        // check online security
        if (handler->HasLowerSecurity(target, 0))
            return false;

        target->CombatStop();
        target->getHostileRefManager().deleteReferences();
        return true;
    }

    static bool HandleFlushArenaPointsCommand(ChatHandler* /*handler*/, char const* /*args*/)
    {
        sArenaTeamMgr->DistributeArenaPoints();
        return true;
    }

    static bool HandleRepairitemsCommand(ChatHandler* handler, char const* args)
    {
        Player* target;
        if (!handler->extractPlayerTarget((char*)args, &target))
            return false;

        // check online security
        if (handler->HasLowerSecurity(target, 0))
            return false;

        // Repair items
        target->DurabilityRepairAll(false, 0, false);

        handler->PSendSysMessage(LANG_YOU_REPAIR_ITEMS, handler->GetNameLink(target).c_str());
        if (handler->needReportToTarget(target))
            ChatHandler(target->GetSession()).PSendSysMessage(LANG_YOUR_ITEMS_REPAIRED, handler->GetNameLink().c_str());

        return true;
    }

    // Send mail by command
    static bool HandleSendMailCommand(ChatHandler* handler, char const* args)
    {
        // format: name "subject text" "mail text"
        Player* target;
        uint64 targetGuid;
        std::string targetName;
        if (!handler->extractPlayerTarget((char*)args, &target, &targetGuid, &targetName))
            return false;

        char* tail1 = strtok(NULL, "");
        if (!tail1)
            return false;

        char const* msgSubject = handler->extractQuotedArg(tail1);
        if (!msgSubject)
            return false;

        char* tail2 = strtok(NULL, "");
        if (!tail2)
            return false;

        char const* msgText = handler->extractQuotedArg(tail2);
        if (!msgText)
            return false;

        // msgSubject, msgText isn't NUL after prev. check
        std::string subject = msgSubject;
        std::string text = msgText;

        // from console show not existed sender
        MailSender sender(MAIL_NORMAL, handler->GetSession() ? handler->GetSession()->GetPlayer()->GetGUIDLow() : 0, MAIL_STATIONERY_GM);

        //- TODO: Fix poor design
        SQLTransaction trans = CharacterDatabase.BeginTransaction();
        MailDraft(subject, text)
            .SendMailTo(trans, MailReceiver(target, GUID_LOPART(targetGuid)), sender);

        CharacterDatabase.CommitTransaction(trans);

        std::string nameLink = handler->playerLink(targetName);
        handler->PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
        return true;
    }

    // Send items by mail
    static bool HandleSendItemsCommand(ChatHandler* handler, char const* args)
    {
        // format: name "subject text" "mail text" item1[:count1] item2[:count2] ... item12[:count12]
        Player* receiver;
        uint64 receiverGuid;
        std::string receiverName;
        if (!handler->extractPlayerTarget((char*)args, &receiver, &receiverGuid, &receiverName))
            return false;

        char* tail1 = strtok(NULL, "");
        if (!tail1)
            return false;

        char const* msgSubject = handler->extractQuotedArg(tail1);
        if (!msgSubject)
            return false;

        char* tail2 = strtok(NULL, "");
        if (!tail2)
            return false;

        char const* msgText = handler->extractQuotedArg(tail2);
        if (!msgText)
            return false;

        // msgSubject, msgText isn't NUL after prev. check
        std::string subject = msgSubject;
        std::string text = msgText;

        // extract items
        typedef std::pair<uint32, uint32> ItemPair;
        typedef std::list< ItemPair > ItemPairs;
        ItemPairs items;

        // get all tail string
        char* tail = strtok(NULL, "");

        uint32 securityTarget = 0;
        uint32 securityHandler = 4;

        uint32 accountId = 0;

        if (receiver)
            if (WorldSession *session = receiver->GetSession())
            {
                accountId = session->GetAccountId();
                securityTarget = session->GetSecurity();
            }

        if (!accountId)
        {
            accountId = sObjectMgr->GetPlayerAccountIdByGUID(receiverGuid);
            securityTarget = AccountMgr::GetSecurity(accountId);
        }

        // The only way, session is NULL, is when we are the CLI
        if (WorldSession *session = handler->GetSession())
            securityHandler = session->GetSecurity();

        // get from tail next item str
        while (char* itemStr = strtok(tail, " "))
        {
            // and get new tail
            tail = strtok(NULL, "");

            // parse item str
            char const* itemIdStr = strtok(itemStr, ":");
            char const* itemCountStr = strtok(NULL, " ");

            uint32 itemId = atoi(itemIdStr);
            if (!itemId)
                return false;

            ItemTemplate const* item_proto = sObjectMgr->GetItemTemplate(itemId);
            if (!item_proto)
            {
                handler->PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemId);
                handler->SetSentErrorMessage(true);
                return false;
            }

            if (AccountMgr::IsVIPorPlayer(securityTarget))
            {
                switch (item_proto->Quality)
                {
                    if (securityHandler < SEC_ADMINISTRATOR && item_proto->ItemLevel > 245)
                    {
                        handler->PSendSysMessage("Diese Handlung ist Euch nicht erlaubt. Sollte es wichtig sein, wendet euch an einen Vorgesetzten.");
                        handler->SetSentErrorMessage(true);
                        return false;
                    }
                    break;

                    case ITEM_QUALITY_LEGENDARY:
                    case ITEM_QUALITY_ARTIFACT:
                    {
                        handler->PSendSysMessage("Diese Handlung ist Euch nicht erlaubt, und kann auch von keinem durchgefuehrt werden.");
                        handler->SetSentErrorMessage(true);
                        return false;
                    }
                }
            }

            uint32 itemCount = itemCountStr ? atoi(itemCountStr) : 1;
            if (itemCount < 1 || (item_proto->MaxCount > 0 && itemCount > uint32(item_proto->MaxCount)))
            {
                handler->PSendSysMessage(LANG_COMMAND_INVALID_ITEM_COUNT, itemCount, itemId);
                handler->SetSentErrorMessage(true);
                return false;
            }

            while (itemCount > item_proto->GetMaxStackSize())
            {
                items.push_back(ItemPair(itemId, item_proto->GetMaxStackSize()));
                itemCount -= item_proto->GetMaxStackSize();
            }

            items.push_back(ItemPair(itemId, itemCount));

            if (items.size() > MAX_MAIL_ITEMS)
            {
                handler->PSendSysMessage(LANG_COMMAND_MAIL_ITEMS_LIMIT, MAX_MAIL_ITEMS);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        // from console show not existed sender
        MailSender sender(MAIL_NORMAL, handler->GetSession() ? handler->GetSession()->GetPlayer()->GetGUIDLow() : 0, MAIL_STATIONERY_GM);

        // fill mail
        MailDraft draft(subject, text);

        SQLTransaction trans = CharacterDatabase.BeginTransaction();

        for (ItemPairs::const_iterator itr = items.begin(); itr != items.end(); ++itr)
        {
            if (Item* item = Item::CreateItem(itr->first, itr->second, handler->GetSession() ? handler->GetSession()->GetPlayer() : 0))
            {
                item->SaveToDB(trans); // save for prevent lost at next mail load, if send fail then item will deleted
                draft.AddItem(item);
            }
        }

        draft.SendMailTo(trans, MailReceiver(receiver, GUID_LOPART(receiverGuid)), sender);
        CharacterDatabase.CommitTransaction(trans);

        std::string nameLink = handler->playerLink(receiverName);
        handler->PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
        return true;
    }

    // Send GM monthly reward by mail
    static bool HandleSendGMRewardCommand(ChatHandler* handler, char const* args)
    {
        Player* receiver;
        uint64 receiverGuid;
        std::string receiverName;
        if (!handler->extractPlayerTarget((char*)args, &receiver, &receiverGuid, &receiverName))
            return false;

        uint32 itemEntry[2];
        itemEntry[0] = 49426; // Emblem of Frost
        itemEntry[1] = GMRewardItems[urand(0, (sizeof(GMRewardItems) / sizeof(uint32)))]; // Get Random item from array

        const ItemTemplate* pItemTemplate[2];

        for (uint8 i = 0; i < 2; ++i)
        {
            pItemTemplate[i] = sObjectMgr->GetItemTemplate(itemEntry[i]);

            if (!pItemTemplate[i])
            {
                handler->PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemEntry[i]);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        // From console show not existed sender
        MailSender sender(MAIL_NORMAL, handler->GetSession() ? handler->GetSession()->GetPlayer()->GetGUIDLow() : 0, MAIL_STATIONERY_GM);

        // Fill mail
        MailDraft draft("GM Belohnung", "Viel Erfolg mit den gewonnenen Items!");

        SQLTransaction trans = CharacterDatabase.BeginTransaction();

        // Create 10 Emblem of Frost
        if (Item* pItem = Item::CreateItem(itemEntry[0], 10, handler->GetSession() ? handler->GetSession()->GetPlayer() : 0))
        {
            pItem->SaveToDB(trans); // save for prevent lost at next mail load, if send fail then item will deleted
            draft.AddItem(pItem);
        }

        // Create random item
        if (Item* pItem = Item::CreateItem(itemEntry[1], 1, handler->GetSession() ? handler->GetSession()->GetPlayer() : 0))
        {
            pItem->SaveToDB(trans); // save for prevent lost at next mail load, if send fail then item will deleted
            draft.AddItem(pItem);
        }

        draft.SendMailTo(trans, MailReceiver(receiver, GUID_LOPART(receiverGuid)), sender);
        CharacterDatabase.CommitTransaction(trans);

        int localeIndex = handler->GetSessionDbLocaleIndex();
        std::string links[2];

        if (localeIndex >= 0)
            for (uint8 i = 0; i < 2; ++i)
                if (const ItemLocale* pItemLocale = sObjectMgr->GetItemLocale(itemEntry[i]))
                    links[i] = pItemLocale->Name[localeIndex];

        for (uint8 i = 0; i < 2; ++i)
            if (links[i].empty())
                links[i] = pItemTemplate[i]->Name1;

        handler->PSendSysMessage("Die GM Belohnung mit dem Inhalt:");
        handler->PSendSysMessage(" - %u x |cffffffff|Hitem:%d:0:0:0:0:0:0:0:0|h[%s]|h|r ", 10, itemEntry[0], links[0].c_str());
        handler->PSendSysMessage(" - %u x |cffffffff|Hitem:%d:0:0:0:0:0:0:0:0|h[%s]|h|r ", 1, itemEntry[1], links[1].c_str());
        handler->PSendSysMessage("wurde erfolgreich an %s gesendet.", receiverName.c_str());
        return true;
    }

    /// Send money by mail
    static bool HandleSendMoneyCommand(ChatHandler* handler, char const* args)
    {
        /// format: name "subject text" "mail text" money

        Player* receiver;
        uint64 receiverGuid;
        std::string receiverName;
        if (!handler->extractPlayerTarget((char*)args, &receiver, &receiverGuid, &receiverName))
            return false;

        char* tail1 = strtok(NULL, "");
        if (!tail1)
            return false;

        char* msgSubject = handler->extractQuotedArg(tail1);
        if (!msgSubject)
            return false;

        char* tail2 = strtok(NULL, "");
        if (!tail2)
            return false;

        char* msgText = handler->extractQuotedArg(tail2);
        if (!msgText)
            return false;

        char* moneyStr = strtok(NULL, "");
        int32 money = moneyStr ? atoi(moneyStr) : 0;
        if (money <= 0)
            return false;

        // msgSubject, msgText isn't NUL after prev. check
        std::string subject = msgSubject;
        std::string text = msgText;

        // from console show not existed sender
        MailSender sender(MAIL_NORMAL, handler->GetSession() ? handler->GetSession()->GetPlayer()->GetGUIDLow() : 0, MAIL_STATIONERY_GM);

        SQLTransaction trans = CharacterDatabase.BeginTransaction();

        MailDraft(subject, text)
            .AddMoney(money)
            .SendMailTo(trans, MailReceiver(receiver, GUID_LOPART(receiverGuid)), sender);

        CharacterDatabase.CommitTransaction(trans);

        std::string nameLink = handler->playerLink(receiverName);
        handler->PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
        return true;
    }

    /// Send a message to a player in game
    static bool HandleSendMessageCommand(ChatHandler* handler, char const* args)
    {
        /// - Find the player
        Player* player;
        if (!handler->extractPlayerTarget((char*)args, &player))
            return false;

        char* msgStr = strtok(NULL, "");
        if (!msgStr)
            return false;

        ///- Check that he is not logging out.
        if (player->GetSession()->isLogingOut())
        {
            handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        /// - Send the message
        // Use SendAreaTriggerMessage for fastest delivery.
        player->GetSession()->SendAreaTriggerMessage("%s", msgStr);
        player->GetSession()->SendAreaTriggerMessage("|cffff0000[Message from administrator]:|r");

        // Confirmation message
        std::string nameLink = handler->GetNameLink(player);
        handler->PSendSysMessage(LANG_SENDMESSAGE, nameLink.c_str(), msgStr);

        return true;
    }

    static bool HandleCreatePetCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Creature* creatureTarget = handler->getSelectedCreature();

        if (!creatureTarget || creatureTarget->isPet() || creatureTarget->GetTypeId() == TYPEID_PLAYER)
        {
            handler->PSendSysMessage(LANG_SELECT_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        CreatureTemplate const* creatrueTemplate = sObjectMgr->GetCreatureTemplate(creatureTarget->GetEntry());
        // Creatures with family 0 crashes the server
        if (!creatrueTemplate->family)
        {
            handler->PSendSysMessage("This creature cannot be tamed. (family id: 0).");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (player->GetPetGUID())
        {
            handler->PSendSysMessage("You already have a pet");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Everything looks OK, create new pet
        Pet* pet = new Pet(player, HUNTER_PET);
        if (!pet->CreateBaseAtCreature(creatureTarget))
        {
            delete pet;
            handler->PSendSysMessage("Error 1");
            return false;
        }

        creatureTarget->setDeathState(JUST_DIED);
        creatureTarget->RemoveCorpse();
        creatureTarget->SetHealth(0); // just for nice GM-mode view

        pet->SetUInt64Value(UNIT_FIELD_CREATEDBY, player->GetGUID());
        pet->SetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE, player->getFaction());

        if (!pet->InitStatsForLevel(creatureTarget->getLevel()))
        {
            sLog->outCommand(handler->GetSession()->GetAccountId(), "InitStatsForLevel() in EffectTameCreature failed! Pet deleted.");
            handler->PSendSysMessage("Error 2");
            delete pet;
            return false;
        }

        // prepare visual effect for levelup
        pet->SetUInt32Value(UNIT_FIELD_LEVEL, creatureTarget->getLevel()-1);

        pet->GetCharmInfo()->SetPetNumber(sObjectMgr->GeneratePetNumber(), true);
        // this enables pet details window (Shift+P)
        pet->InitPetCreateSpells();
        pet->SetFullHealth();

        pet->GetMap()->AddToMap(pet->ToCreature());

        // visual effect for levelup
        pet->SetUInt32Value(UNIT_FIELD_LEVEL, creatureTarget->getLevel());

        player->SetMinion(pet, true);
        pet->SavePetToDB(PET_SAVE_AS_CURRENT);
        player->PetSpellInitialize();

        return true;
    }

    static bool HandlePetLearnCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Pet* pet = player->GetPet();

        if (!pet)
        {
            handler->PSendSysMessage("You have no pet");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 spellId = handler->extractSpellIdFromLink((char*)args);

        if (!spellId || !sSpellMgr->GetSpellInfo(spellId))
            return false;

        // Check if pet already has it
        if (pet->HasSpell(spellId))
        {
            handler->PSendSysMessage("Pet already has spell: %u", spellId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Check if spell is valid
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo))
        {
            handler->PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spellId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        pet->learnSpell(spellId);

        handler->PSendSysMessage("Pet has learned spell %u", spellId);
        return true;
    }

    static bool HandlePetUnlearnCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Pet* pet = player->GetPet();
        if (!pet)
        {
            handler->PSendSysMessage("You have no pet");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 spellId = handler->extractSpellIdFromLink((char*)args);

        if (pet->HasSpell(spellId))
            pet->removeSpell(spellId, false);
        else
            handler->PSendSysMessage("Pet doesn't have that spell");

        return true;
    }

    static bool HandleFreezeCommand(ChatHandler* handler, char const* args)
    {
        std::string name;
        Player* player;
        char const* TargetName = strtok((char*)args, " "); // get entered name
        if (!TargetName) // if no name entered use target
        {
            player = handler->getSelectedPlayer();
            if (player) //prevent crash with creature as target
            {
                name = player->GetName();
                normalizePlayerName(name);
            }
        }
        else // if name entered
        {
            name = TargetName;
            normalizePlayerName(name);
            player = sObjectAccessor->FindPlayerByName(name.c_str());
        }

        if (!player)
        {
            handler->SendSysMessage(LANG_COMMAND_FREEZE_WRONG);
            return true;
        }

        if (player == handler->GetSession()->GetPlayer())
        {
            handler->SendSysMessage(LANG_COMMAND_FREEZE_ERROR);
            return true;
        }

        // effect
        if (player && (player != handler->GetSession()->GetPlayer()))
        {
            handler->PSendSysMessage(LANG_COMMAND_FREEZE, name.c_str());

            // stop combat + make player unattackable + duel stop + stop some spells
            player->setFaction(35);
            player->CombatStop();
            if (player->IsNonMeleeSpellCasted(true))
                player->InterruptNonMeleeSpells(true);
            player->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

            // if player class = hunter || warlock remove pet if alive
            if ((player->getClass() == CLASS_HUNTER) || (player->getClass() == CLASS_WARLOCK))
            {
                if (Pet* pet = player->GetPet())
                {
                    pet->SavePetToDB(PET_SAVE_AS_CURRENT);
                    // not let dismiss dead pet
                    if (pet->isAlive())
                        player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
                }
            }

            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(9454))
                Aura::TryRefreshStackOrCreate(spellInfo, MAX_EFFECT_MASK, player, player);

            // save player
            player->SaveToDB();
        }

        return true;
    }

    static bool HandleUnFreezeCommand(ChatHandler* handler, char const*args)
    {
        std::string name;
        Player* player;
        char* targetName = strtok((char*)args, " "); // Get entered name

        if (targetName)
        {
            name = targetName;
            normalizePlayerName(name);
            player = sObjectAccessor->FindPlayerByName(name.c_str());
        }
        else // If no name was entered - use target
        {
            player = handler->getSelectedPlayer();
            if (player)
                name = player->GetName();
        }

        if (player)
        {
            handler->PSendSysMessage(LANG_COMMAND_UNFREEZE, name.c_str());

            // Reset player faction + allow combat + allow duels
            player->setFactionForRace(player->getRace());
            player->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

            // Remove Freeze spell (allowing movement and spells)
            player->RemoveAurasDueToSpell(9454);

            // Save player
            player->SaveToDB();
        }
        else
        {
            if (targetName)
            {
                // Check for offline players
                PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_GUID_BY_NAME);
                stmt->setString(0, name);
                PreparedQueryResult result = CharacterDatabase.Query(stmt);

                if (!result)
                {
                    handler->SendSysMessage(LANG_COMMAND_FREEZE_WRONG);
                    return true;
                }

                // If player found: delete his freeze aura
                Field* fields = result->Fetch();
                uint32 lowGuid = fields[0].GetUInt32();

                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_AURA_FROZEN);
                stmt->setUInt32(0, lowGuid);
                CharacterDatabase.Execute(stmt);

                handler->PSendSysMessage(LANG_COMMAND_UNFREEZE, name.c_str());
                return true;
            }
            else
            {
                handler->SendSysMessage(LANG_COMMAND_FREEZE_WRONG);
                return true;
            }
        }

        return true;
    }

    static bool HandleListFreezeCommand(ChatHandler* handler, char const* /*args*/)
    {
        // Get names from DB
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURA_FROZEN);
        PreparedQueryResult result = CharacterDatabase.Query(stmt);
        if (!result)
        {
            handler->SendSysMessage(LANG_COMMAND_NO_FROZEN_PLAYERS);
            return true;
        }

        // Header of the names
        handler->PSendSysMessage(LANG_COMMAND_LIST_FREEZE);

        // Output of the results
        do
        {
            Field* fields = result->Fetch();
            std::string player = fields[0].GetString();
            handler->PSendSysMessage(LANG_COMMAND_FROZEN_PLAYERS, player.c_str());
        }
        while (result->NextRow());

        return true;
    }

    static bool HandleGroupLeaderCommand(ChatHandler* handler, char const* args)
    {
        Player* player = NULL;
        Group* group = NULL;
        uint64 guid = 0;
        char* nameStr = strtok((char*)args, " ");

        if (handler->GetPlayerGroupAndGUIDByName(nameStr, player, group, guid))
            if (group && group->GetLeaderGUID() != guid)
            {
                group->ChangeLeader(guid);
                group->SendUpdate();
            }

            return true;
    }

    static bool HandleSbNewCommand(ChatHandler* handler, char const* args)
    {
        if (sbRaidSize > 0)
        {
            handler->PSendSysMessage("Schattenbestienraid bereits erstellt!");
            handler->PSendSysMessage("Status: %u/%u Tanks %u/%u Heals %u/%u DDs - Raidsize: %u", tanks.size(), sbTanks, heals.size(), sbHeals, damage.size(), sbDamage, sbRaidSize);
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!*args)
        {
            handler->PSendSysMessage("Syntax: .sb new #tanks #heals #dds");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // User MUST KNOW WHAT TO DO: 3 PARAMETER NOT LESS
        // jk only Kito will use
        char* dtanks = strtok((char*)args, " ");
        int32 dtanks_int = atoi((char*)dtanks);

        char* dHeals = strtok((char*)NULL, " ");
        int32 dHeals_int = atoi((char*)dHeals);

        char* dDamage = strtok((char*)NULL, " ");
        int32 dDamage_int = atoi((char*)dDamage);

        if (dDamage_int <= 0 || dHeals_int <= 0 || dDamage_int <= 0)
        {
            handler->PSendSysMessage("Syntax: .sb new #tanks #heals #dds");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sbHeals = dHeals_int;
        sbTanks = dtanks_int;
        sbDamage = dDamage_int;
        sbRaidSize = sbHeals + sbTanks + sbDamage;

        sbRaidLeader = handler->GetSession()->GetPlayer();

        handler->PSendSysMessage("Neuer Schattenbestien Raid: %u Tanks %u Heals %u DDs - Raidsize: %u", sbTanks, sbHeals, sbDamage, sbRaidSize);

        return true;
    }

    static bool HandleSbStatusCommand(ChatHandler* handler, char const* args)
    {
        if (sbRaidSize > 0)
            handler->PSendSysMessage("Status: %u/%u Tanks %u/%u Heals %u/%u DDs - Raidsize: %u", tanks.size(), sbTanks, heals.size(), sbHeals, damage.size(), sbDamage, sbRaidSize);
        else
            handler->PSendSysMessage("Es ist kein Schattenbestienraid im Gange!");

        return true;
    }

    static bool HandleSbRemoveCommand(ChatHandler* handler, char const* args)
    {
        sbHeals = 0;
        sbDamage = 0;
        sbTanks = 0;
        sbRaidSize = 0;
        tanks.clear();
        heals.clear();
        damage.clear();

        handler->PSendSysMessage("Schattenbestienraid abgebrochen");

        return true;
    }

    static bool HandleSbJoinCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("Syntax: .sb join [tank/heiler/schaden]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (sbRaidSize == 0)
        {
            handler->PSendSysMessage("Es ist kein Schattenbestienraid im Gange!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string spec = (char*)args;
        Player* player = handler->GetSession()->GetPlayer();

        std::list<uint64>::iterator result = std::find(tanks.begin(), tanks.end(), player->GetGUID());
        std::list<uint64>::iterator result2 = std::find(heals.begin(), heals.end(), player->GetGUID());
        std::list<uint64>::iterator result3 = std::find(damage.begin(), damage.end(), player->GetGUID());

        if (result != tanks.end() || result2 != heals.end() || result3 != damage.end())
        {
            handler->PSendSysMessage("Du bist bereits angemeldet und definitv dabei, gedulde dich und bleib online!");
            handler->PSendSysMessage("Deine Auswahl kannst du nun nicht mehr aendern!  Den Status kannst du mir .sb status checken!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (spec == "tank")
        {
            if (player->getClass() == CLASS_DEATH_KNIGHT || player->getClass() == CLASS_DRUID || player->getClass() == CLASS_WARRIOR || player->getClass() == CLASS_PALADIN)
            {
                if (tanks.size() < sbTanks)
                {
                    tanks.push_back(player->GetGUID());
                    handler->PSendSysMessage("Du bist definitv als Tank dabei! WICHTIG: Bleibe nun online und warte bis alles gefunden wurde!");
                    handler->PSendSysMessage("Status: %u/%u Tanks %u/%u Heals %u/%u DDs - Raidsize: %u", tanks.size(), sbTanks, heals.size(), sbHeals, damage.size(), sbDamage, sbRaidSize);
                }
                else
                {
                    handler->PSendSysMessage("Leider sind alle Tank Slots belegt. Wir haben bereits: %u/%u Tanks %u/%u Heals %u/%u DDs", tanks.size(), sbTanks, heals.size(), sbHeals, damage.size(), sbDamage);
                }
            }
            else
            {
                handler->PSendSysMessage("Deine Klasse kann kein Tank sein!");
            }
        }
        else if (spec == "heiler")
        {
            if (player->getClass() == CLASS_SHAMAN || player->getClass() == CLASS_DRUID || player->getClass() == CLASS_PRIEST || player->getClass() == CLASS_PALADIN)
            {
                if (heals.size() < sbHeals)
                {
                    heals.push_back(player->GetGUID());
                    handler->PSendSysMessage("Du bist definitv als Heiler dabei! WICHTIG: Bleibe nun online und warte bis alles gefunden wurde!");
                    handler->PSendSysMessage("Status: %u/%u Tanks %u/%u Heals %u/%u DDs - Raidsize: %u", tanks.size(), sbTanks, heals.size(), sbHeals, damage.size(), sbDamage, sbRaidSize);
                }
                else
                {
                    handler->PSendSysMessage("Leider sind alle Heiler Slots belegt. Wir haben bereits: %u/%u Tanks %u/%u Heals %u/%u DDs", tanks.size(), sbTanks, heals.size(), sbHeals, damage.size(), sbDamage);
                }
            }
            else
            {
                handler->PSendSysMessage("Deine Klasse kann kein Heiler sein!");
            }
        }
        else if (spec == "schaden")
        {
            if (damage.size() < sbDamage)
            {
                damage.push_back(player->GetGUID());
                handler->PSendSysMessage("Du bist definitv als DD dabei! WICHTIG: Bleibe nun online und warte bis alles gefunden wurde!");
                handler->PSendSysMessage("Status: %u/%u Tanks %u/%u Heals %u/%u DDs - Raidsize: %u", tanks.size(), sbTanks, heals.size(), sbHeals, damage.size(), sbDamage, sbRaidSize);
            }
            else
            {
                handler->PSendSysMessage("Leider sind alle DD Slots belegt. Wir haben bereits: %u/%u Tanks %u/%u Heals %u/%u DDs", tanks.size(), sbTanks, heals.size(), sbHeals, damage.size(), sbDamage);
            }
        }
        else
        {
            handler->PSendSysMessage("Falsche Auswahl: Es gibt nur tank, heiler oder schaden!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Check if all spots full
        if (damage.size() == sbDamage && heals.size() == sbHeals && tanks.size() == sbTanks)
        {
            std::list<uint64> needMarks = heals;
            std::list<uint64> needMarks1 = tanks;
            needMarks.merge(needMarks1);

            tanks.merge(damage);
            tanks.merge(heals);

            Group* sbRaid = new Group();

            if (sbRaidLeader->GetGroup())
                sbRaidLeader->GetGroup()->RemoveMember(sbRaidLeader->GetGUID());

            sbRaid->RemoveInvite(sbRaidLeader);

            if (!sbRaid->Create(sbRaidLeader))
            {
                sbHeals = 0;
                sbDamage = 0;
                sbTanks = 0;
                sbRaidSize = 0;
                tanks.clear();
                heals.clear();
                damage.clear();

                delete sbRaid;
                return false;
            }
            sGroupMgr->AddGroup(sbRaid);

            sbRaid->ConvertToRaid();
            uint8 counter = 0;

            WorldPacket data(SMSG_MESSAGECHAT, 200);
            sbRaidLeader->BuildPlayerChat(&data, CHAT_MSG_WHISPER_INFORM, "Der Raid beginnt in kuerze, bereite dich vor und mach dich vertraut mit deiner Gruppe! Alle Spieler wurden zum Assistent heraufgestuft. Viel Spass!", LANG_UNIVERSAL);

            for (std::list<uint64>::iterator itr = tanks.begin(); itr != tanks.end(); ++itr)
            {
                if (Player* pPlayer = sObjectAccessor->FindPlayer((*itr)))
                {
                    if (pPlayer->GetGroup())
                        pPlayer->GetGroup()->RemoveMember(*itr);
                    sbRaid->AddMember(pPlayer);
                    sbRaid->BroadcastGroupUpdate();
                    sbRaid->SetGroupMemberFlag((*itr), true, MEMBER_FLAG_ASSISTANT);
                    pPlayer->GetSession()->SendPacket(&data);
                }
            }

            for (std::list<uint64>::iterator itr = needMarks.begin(); itr != needMarks.end(); ++itr)
            {
                if (counter <= RAID_TARGET_ICON_MAX)
                    if (Player* pPlayer = sObjectAccessor->FindPlayer((*itr)))
                        sbRaid->SetTargetIcon(counter, sbRaidLeader->GetGUID(), (*itr));

                counter++;
            }

            sbHeals = 0;
            sbDamage = 0;
            sbTanks = 0;
            sbRaidSize = 0;
            tanks.clear();
            heals.clear();
            damage.clear();
        }

        return true;
    }

    static bool HandleGroupDisbandCommand(ChatHandler* handler, char const* args)
    {
        Player* player = NULL;
        Group* group = NULL;
        uint64 guid = 0;
        char* nameStr = strtok((char*)args, " ");

        if (handler->GetPlayerGroupAndGUIDByName(nameStr, player, group, guid))
            if (group)
                group->Disband();

        return true;
    }

    static bool HandleGroupRemoveCommand(ChatHandler* handler, char const* args)
    {
        Player* player = NULL;
        Group* group = NULL;
        uint64 guid = 0;
        char* nameStr = strtok((char*)args, " ");

        if (handler->GetPlayerGroupAndGUIDByName(nameStr, player, group, guid, true))
            if (group)
                group->RemoveMember(guid);

        return true;
    }

    static bool HandleGroupJoinCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* playerSource = NULL;
        Player* playerTarget = NULL;
        Group* groupSource = NULL;
        Group* groupTarget = NULL;
        uint64 guidSource = 0;
        uint64 guidTarget = 0;
        char* nameplgrStr = strtok((char*)args, " ");
        char* nameplStr = strtok(NULL, " ");

        if (handler->GetPlayerGroupAndGUIDByName(nameplgrStr, playerSource, groupSource, guidSource, true))
        {
            if (groupSource)
            {
                if (handler->GetPlayerGroupAndGUIDByName(nameplStr, playerTarget, groupTarget, guidTarget, true))
                {
                    if (!groupTarget && playerTarget->GetGroup() != groupSource)
                    {
                        if (!groupSource->IsFull())
                        {
                            groupSource->AddMember(playerTarget);
                            groupSource->BroadcastGroupUpdate();
                            handler->PSendSysMessage(LANG_GROUP_PLAYER_JOINED, playerTarget->GetName(), playerSource->GetName());
                            return true;
                        }
                        else
                        {
                            // group is full
                            handler->PSendSysMessage(LANG_GROUP_FULL);
                            return true;
                        }
                    }
                    else
                    {
                        // group is full or target player already in a group
                        handler->PSendSysMessage(LANG_GROUP_ALREADY_IN_GROUP, playerTarget->GetName());
                        return true;
                    }
                }
            }
            else
            {
                // specified source player is not in a group
                handler->PSendSysMessage(LANG_GROUP_NOT_IN_GROUP, playerSource->GetName());
                return true;
            }
        }

        return true;
    }

    static bool HandleGroupListCommand(ChatHandler* handler, char const* args)
    {
        Player* playerTarget;
        uint64 guidTarget;
        std::string nameTarget;

        uint32 parseGUID = MAKE_NEW_GUID(atol((char*)args), 0, HIGHGUID_PLAYER);

        if (sObjectMgr->GetPlayerNameByGUID(parseGUID, nameTarget))
        {
            playerTarget = sObjectMgr->GetPlayerByLowGUID(parseGUID);
            guidTarget = parseGUID;
        }
        else if (!handler->extractPlayerTarget((char*)args, &playerTarget, &guidTarget, &nameTarget))
            return false;

        Group* groupTarget = NULL;
        if (playerTarget)
            groupTarget = playerTarget->GetGroup();

        if (!groupTarget)
        {
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GROUP_MEMBER);
            stmt->setUInt32(0, guidTarget);
            PreparedQueryResult resultGroup = CharacterDatabase.Query(stmt);
            if (resultGroup)
                groupTarget = sGroupMgr->GetGroupByDbStoreId((*resultGroup)[0].GetUInt32());
        }

        if (groupTarget)
        {
            handler->PSendSysMessage(LANG_GROUP_TYPE, (groupTarget->isRaidGroup() ? "raid" : "party"));
            Group::MemberSlotList const& members = groupTarget->GetMemberSlots();
            for (Group::MemberSlotList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
            {
                Group::MemberSlot const& slot = *itr;

                std::string flags;
                if (slot.flags & MEMBER_FLAG_ASSISTANT)
                    flags = "Assistant";

                if (slot.flags & MEMBER_FLAG_MAINTANK)
                {
                    if (!flags.empty())
                        flags.append(", ");
                    flags.append("MainTank");
                }

                if (slot.flags & MEMBER_FLAG_MAINASSIST)
                {
                    if (!flags.empty())
                        flags.append(", ");
                    flags.append("MainAssist");
                }

                if (flags.empty())
                    flags = "None";

                Player* p = ObjectAccessor::FindPlayer((*itr).guid);
                const char* onlineState = (p && p->IsInWorld()) ? "online" : "offline";

                handler->PSendSysMessage(LANG_GROUP_PLAYER_NAME_GUID, slot.name.c_str(), onlineState,
                    GUID_LOPART(slot.guid), flags.c_str(), sLFGMgr->GetRolesString(slot.roles).c_str());
            }
        }
        else
            handler->PSendSysMessage(LANG_GROUP_NOT_IN_GROUP, nameTarget.c_str());

        return true;
    }

    static bool HandlePlayAllCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        uint32 soundId = atoi((char*)args);

        if (!sSoundEntriesStore.LookupEntry(soundId))
        {
            handler->PSendSysMessage(LANG_SOUND_NOT_EXIST, soundId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        WorldPacket data(SMSG_PLAY_SOUND, 4);
        data << uint32(soundId) << handler->GetSession()->GetPlayer()->GetGUID();
        sWorld->SendGlobalMessage(&data);

        handler->PSendSysMessage(LANG_COMMAND_PLAYED_TO_ALL, soundId);
        return true;
    }

    static bool HandlePossessCommand(ChatHandler* handler, char const* /*args*/)
    {
        Unit* unit = handler->getSelectedUnit();
        if (!unit)
            return false;

        handler->GetSession()->GetPlayer()->CastSpell(unit, 530, true);
        return true;
    }

    static bool HandleUnPossessCommand(ChatHandler* handler, char const* /*args*/)
    {
        Unit* unit = handler->getSelectedUnit();
        if (!unit)
            unit = handler->GetSession()->GetPlayer();

        unit->RemoveCharmAuras();

        return true;
    }

    static bool HandleBindSightCommand(ChatHandler* handler, char const* /*args*/)
    {
        Unit* unit = handler->getSelectedUnit();
        if (!unit)
            return false;

        handler->GetSession()->GetPlayer()->CastSpell(unit, 6277, true);
        return true;
    }

    static bool HandleUnbindSightCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (player->isPossessing())
            return false;

        player->StopCastingBindSight();
        return true;
    }

    typedef BattlegroundQueue::QueuedPlayersMap::const_iterator BGQ_cIter;

    static bool HandleBGQueryCommand(ChatHandler* handler, const char* /*args*/)
    {
        uint64 guid;

        if (Player* player = handler->GetSession()->GetPlayer())
        {
            guid = player->GetGUID();

            int32 position = sBattlegroundMgr->GetPosition(guid);
            // not going to implement for NOW
            int32 statusH = sBattlegroundMgr->GetHStatus(guid);
            int32 statusA = sBattlegroundMgr->GetAStatus(guid);
            handler->PSendSysMessage("Schlachtfeld Warteschlange");
            handler->PSendSysMessage("===========================================");
            handler->PSendSysMessage("Aktuell sind %d Horde Spieler in der Warteschlange",statusH);
            handler->PSendSysMessage("Aktuell sind %d Allianz Spieler in der Warteschlange",statusA);
            if (position != -1)
                handler->PSendSysMessage("Du bist an Position %d",position);
            else
                handler->PSendSysMessage("Du bist nicht in der Warteschlange f�r zufaelliges Schlachtfeld!");
            return true;
        }
        return false;
    }

    static bool HandleReportCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (!*args)
        {
            handler->PSendSysMessage("Syntax: .report [QUEST-LINK]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // number or [name] Shift-click form |color|Hquest:quest_id:quest_level|h[name]|h|r
        char* cId = handler->extractKeyFromLink((char*)args, "Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);
        Quest const* quest = sObjectMgr->GetQuestTemplate(entry);

        // TODO: Add ban if fail rate 70%

        // If player doesn't have the quest
        if (!quest || player->GetQuestStatus(entry) == QUEST_STATUS_NONE)
        {
            handler->PSendSysMessage("Du musst die Quest vorher annehmen!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (quest->IsDailyOrWeekly() || quest->IsRaidQuest() || quest->IsDFQuest())
        {
                handler->PSendSysMessage("Quest darf keine Weekly-, Daily-, Raid- oder Dungeonquest sein!");
                handler->SetSentErrorMessage(true);
                return false;
        }

        QueryResult result = LoginDatabase.PQuery("SELECT approve, blocked FROM `quest_report` WHERE `questEntry` = %u", entry);

        if (result) // Quest gefunden
        {
            Field* field = result->Fetch();
            if (field[0].GetUInt32() == 1) // Quest gefunden und approved
            {
                // Add quest items for quests that require items
                for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
                {
                    uint32 id = quest->RequiredItemId[x];
                    uint32 count = quest->RequiredItemCount[x];
                    if (!id || !count)
                        continue;

                    uint32 curItemCount = player->GetItemCount(id, true);

                    ItemPosCountVec dest;
                    uint8 msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, count-curItemCount);
                    if (msg == EQUIP_ERR_OK)
                    {
                        Item* item = player->StoreNewItem(dest, id, true);
                        player->SendNewItem(item, count-curItemCount, true, false);
                    }
                }

                // All creature/GO slain/casted (not required, but otherwise it will display "Creature slain 0/10")
                for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
                {
                    int32 creature = quest->RequiredNpcOrGo[i];
                    uint32 creaturecount = quest->RequiredNpcOrGoCount[i];

                    if (uint32 spell_id = quest->RequiredSpellCast[i])
                    {
                        for (uint16 z = 0; z < creaturecount; ++z)
                            player->CastedCreatureOrGO(creature, 0, spell_id);
                    }
                    else if (creature > 0)
                    {
                        if (CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(creature))
                            for (uint16 z = 0; z < creaturecount; ++z)
                                player->KilledMonster(cInfo, 0);
                    }
                    else if (creature < 0)
                    {
                        for (uint16 z = 0; z < creaturecount; ++z)
                            player->CastedCreatureOrGO(creature, 0, 0);
                    }
                }
                player->CompleteQuest(entry);
                handler->PSendSysMessage("Quest Report: Erfolg");
                handler->PSendSysMessage("====================================");
                handler->PSendSysMessage("Die Quest wurde dir vervollstaendigt");
                LoginDatabase.DirectPExecute("UPDATE `quest_report` SET count = count + 1 WHERE realmID = %u AND questEntry = %u", realmID, entry);
                return true;
            } else { // Quest gefunden und nicht approved
                if (field[1].GetUInt32() == 1) // quest gefunden nicht approved und blockiert
                {
                    handler->PSendSysMessage("Quest Report: Abgelehnt");
                    handler->PSendSysMessage("====================================");
                    handler->PSendSysMessage("Diese Quest wurde von uns als funktionstuechtig markiert");
                    LoginDatabase.DirectPExecute("UPDATE `quest_report` SET count = count + 1 WHERE realmID = %u AND questEntry = %u", realmID, entry);
                    handler->SetSentErrorMessage(true);
                    return false;
                } else {
                    handler->PSendSysMessage("Quest Report: Noch nicht verifiziert");
                    handler->PSendSysMessage("====================================");
                    handler->PSendSysMessage("Diese Quest wurde von uns nocht nicht verifiziert! Schreibe bitte ein Ticket");
                    handler->SetSentErrorMessage(true);
                    LoginDatabase.DirectPExecute("UPDATE `quest_report` SET count = count + 1 WHERE realmID = %u AND questEntry = %u", realmID, entry);
                    return false;
                }
            }
        } else { // Quest nicht gefunden
            LoginDatabase.DirectPExecute("INSERT INTO `quest_report` (realmID, questEntry, approve, count) VALUES (%u, %u, 0, 1)", realmID, entry);
            LoginDatabase.DirectPExecute("INSERT INTO `quest_report_log` (realmID, questEntry, account) VALUES (%u, %u, %u)", realmID, entry, handler->GetSession()->GetAccountId());
            handler->PSendSysMessage("Quest Report: Neu Eintrag");
            handler->PSendSysMessage("====================================");
            handler->PSendSysMessage(LANG_COMMAND_QUEST_REPORT_INSERT);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

    static bool HandleJoinStartGuildCommand(ChatHandler *handler,
                                            const char *args) {
        if (Player *player = handler->GetSession()->GetPlayer()) {
            if (player->GetGuildId()) {
                handler->PSendSysMessage("Please leave your guild before you "
                                         "try to join the Starter Guild");
                return false;
            }

            uint32_t guild_id =
                sWorld->getIntConfig((player->GetTeamId() == TEAM_ALLIANCE)
                                         ? CONFIG_STARTER_GUILD_ALLIANCE
                                         : CONFIG_STARTER_GUILD_HORDE);

            if (auto starter_guild = sGuildMgr->GetGuildById(guild_id))
                starter_guild->AddMember(player->GetGUID());
        }

        return true;
    }
};

void AddSC_misc_commandscript()
{
    new misc_commandscript();
}