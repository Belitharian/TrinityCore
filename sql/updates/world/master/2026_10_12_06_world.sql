-- ============================================================================
-- Flaming Twisters (Embertalon, Midnight) - donne a Jaina dans les Ruines de Theramore
--   1283566 : 2 s d'incantation, dummy "server-side script" -> spell_ruins_flaming_twisters
--             (3 ennemis au hasard a 60 m, missile 1283561 sur chacun)
--   1283561 : Trigger Missile -> 1283558
--   1283558 : Create AreaTrigger 41584, 20 s -> at_ruins_flaming_twister
--   1283559 : DoT de feu (4 % toutes les 1,5 s) + effet d'ecran, pose par la tornade
--
-- L'areatrigger 41584 n'a aucune donnee cote serveur : forme et deplacement sont
-- notre version (sphere de 4 m qui suit le terrain et erre au sol, cf. le script).
-- Le visuel vient du sort createur (XSV 511028).
-- ============================================================================

DELETE FROM `spell_script_names` WHERE `spell_id` = 1283566 AND `ScriptName` = 'spell_ruins_flaming_twisters';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(1283566, 'spell_ruins_flaming_twisters');

DELETE FROM `areatrigger_template` WHERE `Id` = 41584 AND `IsCustom` = 0;
INSERT INTO `areatrigger_template` (`Id`,`IsCustom`,`Flags`,`ActionSetId`,`ActionSetFlags`,`VerifiedBuild`) VALUES
(41584, 0, 0, 0, 0, 0);

-- Flags 24 = HasFaceMovementDir | HasFollowsTerrain (comme l'Orbe des arcanes, 32530)
DELETE FROM `areatrigger_create_properties` WHERE `Id` = 41584 AND `IsCustom` = 0;
INSERT INTO `areatrigger_create_properties` (`Id`,`IsCustom`,`AreaTriggerId`,`IsAreatriggerCustom`,`Flags`,`MoveCurveId`,`ScaleCurveId`,`MorphCurveId`,`FacingCurveId`,`AnimId`,`AnimKitId`,`DecalPropertiesId`,`SpellForVisuals`,`PositionalSoundKitId`,`TimeToTargetScale`,`Speed`,`SpeedIsTime`,`Shape`,`ShapeData0`,`ShapeData1`,`ShapeData2`,`ShapeData3`,`ShapeData4`,`ShapeData5`,`ShapeData6`,`ShapeData7`,`Roll`,`Pitch`,`Yaw`,`TargetRoll`,`TargetPitch`,`TargetYaw`,`ScriptName`,`VerifiedBuild`) VALUES
(41584, 0, 41584, 0, 24, 0, 0, 0, 0, -1, 0, 0, NULL, 0, 0, 4, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 'at_ruins_flaming_twister', 0);
