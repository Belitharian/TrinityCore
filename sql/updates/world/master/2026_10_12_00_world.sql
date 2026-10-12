-- ============================================================================
-- Les Ruines de Theramore - refonte d'apres Tides of War
--   1. creature_text : Jaina (64727), Kalecgos (64565), chef des pillards (65442)
--   2. Cadavres custom 500033-500037 (Thaddus, ingenieur nain, Aden, Marcus, rat)
--
-- Les repliques dont le texte change perdent leur son (Sound 0) : elles sont
-- a redoubler, la liste est dans ruins_of_theramore.h (RFTTalks).
-- Les repliques officielles (sons Blizzard) ne sont pas modifiees.
-- ============================================================================

-- ----------------------------------------------------------------------------
-- 1. creature_text
-- ----------------------------------------------------------------------------

-- Jaina 1 : sur l'ilot elle ne sait pas encore que Kinndy et Tervosh sont morts, elle nomme les generaux
UPDATE `creature_text` SET `Sound` = 0, `Text` = 'Mon royaume... tous les généraux... Coup-Robuste, Tiras''alan, Aubrey... Rhonin... ô douce Lumière, Rhonin. Pourquoi a-t-il agi ainsi ? Pourquoi m''a-t-il sauvée ? C''est moi qui suis responsable de tout cela !'
    WHERE `CreatureID` = 64727 AND `GroupID` = 1 AND `ID` = 0;

-- Kalecgos 11 : la bombe etait alimentee par l'iris, le souffle l'a projete en mer
UPDATE `creature_text` SET `Sound` = 0, `Text` = 'Non, Jaina. Vous n''y êtes pour rien. Vous ne devez pas vous en vouloir. Si c''est la faute de quelqu''un, c''est bien la mienne... celle de mon peuple qui n''a su empêcher le vol de ce maudit iris de focalisation. La bombe était alimentée par l''iris. J''étais plus loin que les autres, et le souffle m''a projeté en pleine mer. Personne n''aurait pu arrêter cette explosion.'
    WHERE `CreatureID` = 64565 AND `GroupID` = 11 AND `ID` = 0;

-- Jaina 14 : derniere replique, avant le portail
UPDATE `creature_text` SET `Sound` = 0, `Text` = 'Pas un mot de l''iris à Kalecgos. Je pars pour Hurlevent : Varian doit savoir ce que Garrosh a fait ici.'
    WHERE `CreatureID` = 64727 AND `GroupID` = 14 AND `ID` = 0;

-- Jaina : nouvelles repliques (16-28)
DELETE FROM `creature_text` WHERE `CreatureID` = 64727 AND `GroupID` BETWEEN 16 AND 28;
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(64727,16,0,'Les cheveux de Peine se brisent comme du verre sous les doigts de Jaina.',16,0,100,0,0,0,0,0,0,'Jaina - Traces : les cheveux de Peine (emote)'),
(64727,17,0,'Peine... Tu es morte l''épée à la main. Comme tu as vécu : en défendant Theramore.',12,0,100,0,0,0,0,0,0,'Jaina - Traces : Peine'),
(64727,18,0,'Aubrey... Marcus... Tiras''alan... Coup-Robuste... Ils sont tous là.',12,0,100,0,0,0,0,0,0,'Jaina - Traces : les generaux'),
(64727,19,0,'Même la mort ne les laisse pas en paix...',12,0,100,0,0,0,0,0,0,'Jaina - Traces : l''officier qui flotte'),
(64727,20,0,'Même les rats... Kalec avait raison. Rien n''a survécu.',12,0,100,0,0,0,0,0,0,'Jaina - Traces : le rat'),
(64727,21,0,'Non. Quelqu''un a forcément survécu. Tervosh ? Janene ? Allen ? Où êtes-vous ?!',14,0,100,0,0,0,0,0,0,'Jaina - Traces : elle cherche des survivants'),
(64727,22,0,'Jaina pousse un hurlement de douleur et de rage.',16,0,100,0,0,0,0,0,0,'Jaina - Kinndy tombe en poussiere (emote)'),
(64727,23,0,'Ce n''était pas une bataille. C''était un anéantissement, commis de loin, par des lâches. Garrosh... Et toi, Baine. Tu savais.',12,0,100,0,0,0,0,0,0,'Jaina - Rage apres Kinndy'),
(64727,24,0,'Les docks... Il n''en reste rien. Étrangement, c''est ici que j''ai le moins mal.',12,0,100,0,0,0,0,0,0,'Jaina - Les docks'),
(64727,25,0,'Non... Le cratère. Il faut que je le voie.',12,0,100,0,0,0,0,0,0,'Jaina - Attiree par le cratere'),
(64727,26,0,'Une cité entière réduite en cendres... et pas une égratignure.',12,0,100,0,0,0,0,0,0,'Jaina - Devant l''iris'),
(64727,27,0,'Kalec viendra le chercher. Il ne sentira que moi.',12,0,100,0,0,0,0,0,0,'Jaina - Scelle l''iris de son essence'),
(64727,28,0,'Si la Horde veut la guerre... elle l''aura. Avec plaisir.',12,0,100,0,0,0,0,0,0,'Jaina - L''iris lui appartient');

-- Chef des pillards (65442) : raillerie avant l'officielle 2 ("votre tete sur un plateau")
DELETE FROM `creature_text` WHERE `CreatureID` = 65442 AND `GroupID` = 3;
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(65442,3,0,'Petite dame... sais pas comment toi survivre. Mais nous, on corrige l''erreur.',12,0,100,0,0,0,0,0,0,'Chef des pillards - Decouvre Jaina');

-- ----------------------------------------------------------------------------
-- 2. Cadavres custom (copies des PNJ officiels, PV alignes sur les unites du scenario)
--   500033 Thaddus Coup-Robuste   <- 132404 Fusilier nain (display 78306)   PV x4 (general)
--   500034 Ingenieur nain         <- 94263  Ingenieur nain (display 63913)  PV x3
--   500035 Lieutenant Aden        <- 23951                                  PV x3
--   500036 General Marcus Jonathan<- 466                                    PV x4 (general)
--   500037 Rat noir               <- 2110                                   PV x0.01
-- Pas de creature_template_addon : les montures des originaux sont retirees.
-- ----------------------------------------------------------------------------
DELETE FROM `creature_template`            WHERE `entry`      BETWEEN 500033 AND 500037;
DELETE FROM `creature_template_difficulty` WHERE `Entry`      BETWEEN 500033 AND 500037;
DELETE FROM `creature_template_model`      WHERE `CreatureID` BETWEEN 500033 AND 500037;
DELETE FROM `creature_template_locale`     WHERE `entry`      BETWEEN 500033 AND 500037;
DELETE FROM `creature_equip_template`      WHERE `CreatureID` BETWEEN 500033 AND 500037;
DELETE FROM `creature_template_addon`      WHERE `entry`      BETWEEN 500033 AND 500037;

-- creature_template
DROP TEMPORARY TABLE IF EXISTS `tmp_ruins_ct`;
CREATE TEMPORARY TABLE `tmp_ruins_ct` SELECT * FROM `creature_template` WHERE `entry` IN (132404, 94263, 23951, 466, 2110);
UPDATE `tmp_ruins_ct` SET `entry` = 500033, `name` = 'Thaddus Stoutblow',        `Classification` = 1 WHERE `entry` = 132404;
UPDATE `tmp_ruins_ct` SET `entry` = 500034, `name` = 'Dwarven Engineer'                                WHERE `entry` = 94263;
UPDATE `tmp_ruins_ct` SET `entry` = 500035, `name` = 'Lieutenant Aden'                                 WHERE `entry` = 23951;
UPDATE `tmp_ruins_ct` SET `entry` = 500036, `name` = 'General Marcus Jonathan'                         WHERE `entry` = 466;
UPDATE `tmp_ruins_ct` SET `entry` = 500037, `name` = 'Black Rat', `MovementType` = 0, `movementId` = 0 WHERE `entry` = 2110;
UPDATE `tmp_ruins_ct` SET `KillCredit1` = 0, `KillCredit2` = 0, `subname` = NULL, `TitleAlt` = NULL, `IconName` = NULL,
    `npcflag` = 0, `unit_flags` = 0, `unit_flags2` = 2048, `unit_flags3` = 0, `flags_extra` = 0,
    `AIName` = '', `ScriptName` = '', `StringId` = NULL, `VerifiedBuild` = -1;
UPDATE `tmp_ruins_ct` SET `faction` = 150 WHERE `entry` <> 500037;
INSERT INTO `creature_template` SELECT * FROM `tmp_ruins_ct`;
DROP TEMPORARY TABLE `tmp_ruins_ct`;

-- creature_template_difficulty : DifficultyID 0 et 12 (scenario), ContentTuning 3025 comme les autres cadavres
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) VALUES
(500033,  0, 0, 0, 3025, 11, 4,    1, 1, 1, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500033, 12, 0, 0, 3025, 11, 4,    1, 1, 1, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500034,  0, 0, 0, 3025, 11, 3,    1, 1, 1, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500034, 12, 0, 0, 3025, 11, 3,    1, 1, 1, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500035,  0, 0, 0, 3025, 11, 3,    1, 1, 1, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500035, 12, 0, 0, 3025, 11, 3,    1, 1, 1, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500036,  0, 0, 0, 3025, 11, 4,    1, 1, 1, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500036, 12, 0, 0, 3025, 11, 4,    1, 1, 1, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500037,  0, 0, 0, 3025, 11, 0.01, 1, 1, 1, 0, 2097224, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1),
(500037, 12, 0, 0, 3025, 11, 0.01, 1, 1, 1, 0, 2097224, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1);

-- creature_template_model
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(500033, 0, 78306, 1, 1, -1),
(500034, 0, 63913, 1, 1, -1),
(500035, 0, 4839,  1, 1, -1),
(500036, 0, 1688,  1, 1, -1),
(500037, 0, 1141,  1, 1, -1);

-- creature_template_locale
INSERT INTO `creature_template_locale` (`entry`,`locale`,`Name`,`NameAlt`,`Title`,`TitleAlt`,`VerifiedBuild`) VALUES
(500033, 'frFR', 'Thaddus Coup-Robuste',     NULL, NULL, NULL, -1),
(500034, 'frFR', 'Ingénieur nain',           NULL, NULL, NULL, -1),
(500035, 'frFR', 'Lieutenant Aden',          NULL, NULL, NULL, -1),
(500036, 'frFR', 'Général Marcus Jonathan',  NULL, NULL, NULL, -1),
(500037, 'frFR', 'Rat noir',                 NULL, NULL, NULL, -1);

-- creature_equip_template (armes des originaux)
INSERT INTO `creature_equip_template` (`CreatureID`,`ID`,`ItemID1`,`AppearanceModID1`,`ItemVisual1`,`ItemID2`,`AppearanceModID2`,`ItemVisual2`,`ItemID3`,`AppearanceModID3`,`ItemVisual3`,`VerifiedBuild`) VALUES
(500033, 1, 0,     0, 0, 0,   0, 0, 150586, 0, 0, -1),
(500035, 1, 2179,  0, 0, 0,   0, 0, 2551,   0, 0, -1),
(500036, 1, 46987, 0, 0, 143, 0, 0, 0,      0, 0, -1);
