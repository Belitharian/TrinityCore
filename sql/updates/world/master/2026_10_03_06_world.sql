-- Dalaran Purge - High Arcanist Savor : l'intro (anciennement MoveInLineOfSight) est declenchee a l'entree de l'arene des egouts.
-- Sphere de 5 m reprise des proprietes 500005 (Dalaran Purge - Captain).
DELETE FROM `areatrigger` WHERE `SpawnId`=500006 AND `IsCustom`=1;
INSERT INTO `areatrigger` (`SpawnId`, `AreaTriggerCreatePropertiesId`, `IsCustom`, `MapId`, `SpawnDifficulties`, `PosX`, `PosY`, `PosZ`, `Orientation`, `PhaseUseFlags`, `PhaseId`, `PhaseGroup`, `ScriptName`, `Comment`, `VerifiedBuild`) VALUES
(500006, 500005, 1, 5002, '12', -897.535, 4465.72, 659.475, 5.60356, 0, 0, 0, 'areatrigger_purge_savor', 'Dalaran Purge - High Arcanist Savor', 0);
