-- ============================================================================
-- Les Ruines de Theramore - les pillards (Tides of War)
--   Groupe 0 (ancien nettoyage) : les pillards + leur chef (65442).
--   Groupe 1 (ancien assaut final et sa spawn region 500101) : supprime.
--   Les 3 etendards pilles rejoignent le camp des pillards.
-- ============================================================================

-- Chef des pillards, au milieu du groupe, tourne vers l'arrivee de Jaina (Kinndy)
DELETE FROM `creature_summon_groups` WHERE `summonerId` = 5001 AND `summonerType` = 2 AND `groupId` = 0 AND `entry` = 65442;
INSERT INTO `creature_summon_groups` (`summonerId`,`summonerType`,`groupId`,`entry`,`position_x`,`position_y`,`position_z`,`orientation`,`summonType`,`summonTime`,`SpawnRegionId`,`Comment`) VALUES
(5001, 2, 0, 65442, -3825.00, -4530.00, 10.50, 1.52, 1, 3600000, 0, 'RUINS 1 - Chef des pillards');

-- Plus de vague d'assaut final
DELETE FROM `creature_summon_groups` WHERE `summonerId` = 5001 AND `summonerType` = 2 AND `groupId` = 1;
DELETE FROM `creature_spawn_region` WHERE `SpawnRegionId` = 500101;

-- Etendards pilles, a cote de trois pillards
UPDATE `gameobject` SET `position_x` = -3805.50, `position_y` = -4536.00, `position_z` = 10.04, `orientation` = 0, `rotation2` = 0, `rotation3` = 1 WHERE `guid` = 50000008 AND `id` = 214965;
UPDATE `gameobject` SET `position_x` = -3844.50, `position_y` = -4528.00, `position_z` = 9.70,  `orientation` = 0, `rotation2` = 0, `rotation3` = 1 WHERE `guid` = 50000009 AND `id` = 214965;
UPDATE `gameobject` SET `position_x` = -3829.50, `position_y` = -4557.50, `position_z` = 8.88,  `orientation` = 0, `rotation2` = 0, `rotation3` = 1 WHERE `guid` = 50000010 AND `id` = 214965;
