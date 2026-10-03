-- Dalaran Purge : le groupe de la prison (map 5002, groupe 0) forme la region 500201,
-- suivie par le critere cache 100056 (KilledAllUnitsInSpawnRegion) de l'etape What happened?
UPDATE `creature_summon_groups` SET `SpawnRegionId`=500201 WHERE `summonerId`=5002 AND `summonerType`=2 AND `groupId`=0;

-- Dalaran Purge - Galerie d'Aegwynn : arrivee devant Narasi (remplace le watchdog EVT_PRISON_NARASI_CHECKER).
-- Sphere de 10 m pour couvrir toute la largeur du couloir.
DELETE FROM `areatrigger_template` WHERE `Id`=500007 AND `IsCustom`=1;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`, `VerifiedBuild`) VALUES
(500007, 1, 1, 0, 0, 0);

DELETE FROM `areatrigger_create_properties` WHERE `Id`=500007 AND `IsCustom`=1;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `MoveCurveId`, `ScaleCurveId`, `MorphCurveId`, `FacingCurveId`, `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`, `PositionalSoundKitId`, `TimeToTargetScale`, `Speed`, `SpeedIsTime`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`, `ShapeData5`, `ShapeData6`, `ShapeData7`, `Roll`, `Pitch`, `Yaw`, `TargetRoll`, `TargetPitch`, `TargetYaw`, `ScriptName`, `VerifiedBuild`) VALUES
(500007, 1, 500007, 1, 0, 0, 0, 0, 0, -1, 0, 0, NULL, 0, 0, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, '', 0);

DELETE FROM `areatrigger` WHERE `SpawnId`=500007 AND `IsCustom`=1;
INSERT INTO `areatrigger` (`SpawnId`, `AreaTriggerCreatePropertiesId`, `IsCustom`, `MapId`, `SpawnDifficulties`, `PosX`, `PosY`, `PosZ`, `Orientation`, `PhaseUseFlags`, `PhaseId`, `PhaseGroup`, `ScriptName`, `Comment`, `VerifiedBuild`) VALUES
(500007, 500007, 1, 5002, '12', -805.385, 4435.86, 598.488, 2.46528, 0, 0, 0, 'areatrigger_purge_narasi', 'Dalaran Purge - Narasi (Galerie d''Aegwynn)', 0);
