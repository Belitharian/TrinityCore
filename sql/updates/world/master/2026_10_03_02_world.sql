-- Spawn regions des creatures invoquees par SummonCreatureGroup (criteria type 64)
ALTER TABLE `creature_summon_groups` ADD COLUMN `SpawnRegionId` int unsigned NOT NULL DEFAULT '0' AFTER `summonTime`;

-- Ruins of Theramore (5001) : assaut final de l'iris, warlord Rok'nah exclu (Jaina l'acheve apres)
UPDATE `creature_summon_groups` SET `SpawnRegionId`=500101 WHERE `summonerId`=5001 AND `summonerType`=2 AND `groupId`=1 AND `entry`<>65442;
