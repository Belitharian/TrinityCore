-- Nettoyage DBErrors (contenu custom)

-- PNJ inutilises : 500033 (Archmage Lan'dalock, brouillon sans spawn ni modele) et 550001 (TEST, DisplayID invalide)
DELETE FROM `creature_template` WHERE `entry` IN (500033, 550001);
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (500033, 550001);
DELETE FROM `creature_template_difficulty` WHERE `Entry` IN (500033, 550001);
DELETE FROM `creature_template_locale` WHERE `entry` IN (500033, 550001);

-- Bataille de Theramore : faction 0 (remplacee par 35 au chargement) -> 150 comme Tervosh et Kinndy
UPDATE `creature_template` SET `faction`=150 WHERE `entry` IN (500012, 500013);

-- Flags refuses par le core (deja retires au chargement, aucun changement en jeu)
UPDATE `creature_template` SET `unit_flags`=`unit_flags` & ~(0x2 | 0x4) WHERE `entry`=500017;   -- Sunreaver Citizen : NON_ATTACKABLE, REMOVE_CLIENT_CONTROL
UPDATE `creature_template` SET `unit_flags`=`unit_flags` & ~0x20000 WHERE `entry`=500032;        -- Dungeoneer's Training Dummy : PACIFIED
UPDATE `creature` SET `unit_flags`=NULL, `unit_flags2`=NULL WHERE `guid`=50000529 AND `id`=600009; -- Loup sauvage : IN_COMBAT, INFINITE_AOI (reste = template)
UPDATE `creature` SET `unit_flags`=NULL WHERE `guid`=50000545 AND `id`=700000;                     -- Wanda Maximoff : CAN_SWIM (reste = template)

-- Conversation 40000 : acteurs designes par GUID, CreatureId / CreatureDisplayInfoId ignores par le core
UPDATE `conversation_actors` SET `CreatureId`=0, `CreatureDisplayInfoId`=0 WHERE `ConversationId`=40000 AND `Idx` IN (0, 1, 2, 3);

-- Area trigger 5637 : le donjon custom Pit of Saron n'existe plus, retour a la destination officielle
UPDATE `areatrigger_teleport` SET `PortLocID`=3922, `Name`='Icecrown Dungeon - Pit of Saron - Entrance Target' WHERE `ID`=5637;
