-- ============================================================================
-- Les Ruines de Theramore - nettoyage apres la refonte Tides of War
--   1. creature_text : repliques plus jouees supprimees, GroupID renumerotes sans trou
--      (constantes RFTTalks de ruins_of_theramore.h alignees)
--   2. spell_script_names : scripts des anciens sorts de Jaina retires du core
--   3. creature_summon_groups : AUTO_INCREMENT remis a MAX(ID) + 1
-- ============================================================================

-- ----------------------------------------------------------------------------
-- 1. creature_text
-- ----------------------------------------------------------------------------
-- Jaina (64727) : 7, 8, 9, 10, 13 (VO de l'ancien deroule : contre-attaque, iris
-- "avant le transport", bombe desactivee). Chef des pillards (65442) : 0 ("rapportez-moi la bombe").
DELETE FROM `creature_text` WHERE `CreatureID` = 64727 AND `GroupID` IN (7, 8, 9, 10, 13);
DELETE FROM `creature_text` WHERE `CreatureID` = 65442 AND `GroupID` = 0;

-- Jaina : 11 -> 7, 12 -> 8, puis 14..35 -> 9..30 (dans l'ordre croissant : chaque cible est deja libre)
UPDATE `creature_text` SET `GroupID` = 7  WHERE `CreatureID` = 64727 AND `GroupID` = 11;
UPDATE `creature_text` SET `GroupID` = 8  WHERE `CreatureID` = 64727 AND `GroupID` = 12;
UPDATE `creature_text` SET `GroupID` = 9  WHERE `CreatureID` = 64727 AND `GroupID` = 14;
UPDATE `creature_text` SET `GroupID` = 10 WHERE `CreatureID` = 64727 AND `GroupID` = 15;
UPDATE `creature_text` SET `GroupID` = 11 WHERE `CreatureID` = 64727 AND `GroupID` = 16;
UPDATE `creature_text` SET `GroupID` = 12 WHERE `CreatureID` = 64727 AND `GroupID` = 17;
UPDATE `creature_text` SET `GroupID` = 13 WHERE `CreatureID` = 64727 AND `GroupID` = 18;
UPDATE `creature_text` SET `GroupID` = 14 WHERE `CreatureID` = 64727 AND `GroupID` = 19;
UPDATE `creature_text` SET `GroupID` = 15 WHERE `CreatureID` = 64727 AND `GroupID` = 20;
UPDATE `creature_text` SET `GroupID` = 16 WHERE `CreatureID` = 64727 AND `GroupID` = 21;
UPDATE `creature_text` SET `GroupID` = 17 WHERE `CreatureID` = 64727 AND `GroupID` = 22;
UPDATE `creature_text` SET `GroupID` = 18 WHERE `CreatureID` = 64727 AND `GroupID` = 23;
UPDATE `creature_text` SET `GroupID` = 19 WHERE `CreatureID` = 64727 AND `GroupID` = 24;
UPDATE `creature_text` SET `GroupID` = 20 WHERE `CreatureID` = 64727 AND `GroupID` = 25;
UPDATE `creature_text` SET `GroupID` = 21 WHERE `CreatureID` = 64727 AND `GroupID` = 26;
UPDATE `creature_text` SET `GroupID` = 22 WHERE `CreatureID` = 64727 AND `GroupID` = 27;
UPDATE `creature_text` SET `GroupID` = 23 WHERE `CreatureID` = 64727 AND `GroupID` = 28;
UPDATE `creature_text` SET `GroupID` = 24 WHERE `CreatureID` = 64727 AND `GroupID` = 29;
UPDATE `creature_text` SET `GroupID` = 25 WHERE `CreatureID` = 64727 AND `GroupID` = 30;
UPDATE `creature_text` SET `GroupID` = 26 WHERE `CreatureID` = 64727 AND `GroupID` = 31;
UPDATE `creature_text` SET `GroupID` = 27 WHERE `CreatureID` = 64727 AND `GroupID` = 32;
UPDATE `creature_text` SET `GroupID` = 28 WHERE `CreatureID` = 64727 AND `GroupID` = 33;
UPDATE `creature_text` SET `GroupID` = 29 WHERE `CreatureID` = 64727 AND `GroupID` = 34;
UPDATE `creature_text` SET `GroupID` = 30 WHERE `CreatureID` = 64727 AND `GroupID` = 35;

-- Chef des pillards : 1 -> 0, 2 -> 1, 3 -> 2
UPDATE `creature_text` SET `GroupID` = 0 WHERE `CreatureID` = 65442 AND `GroupID` = 1;
UPDATE `creature_text` SET `GroupID` = 1 WHERE `CreatureID` = 65442 AND `GroupID` = 2;
UPDATE `creature_text` SET `GroupID` = 2 WHERE `CreatureID` = 65442 AND `GroupID` = 3;

-- ----------------------------------------------------------------------------
-- 2. spell_script_names : Comet Barrage (354938) et Arcane Chaos (406854) ne sont plus
--    lances par personne. Frigid Shards (354933) reste : la Jaina de la Purge s'en sert.
-- ----------------------------------------------------------------------------
DELETE FROM `spell_script_names` WHERE `spell_id` = 354938 AND `ScriptName` = 'spell_ruins_comet_barrage';
DELETE FROM `spell_script_names` WHERE `spell_id` = 406854 AND `ScriptName` = 'spell_arcane_chaos';

-- ----------------------------------------------------------------------------
-- 3. creature_summon_groups : l'ancien groupe 1 (assaut final) a ete supprime
-- ----------------------------------------------------------------------------
SET @AI := (SELECT IFNULL(MAX(`ID`), 0) + 1 FROM `creature_summon_groups`);
SET @SQL := CONCAT('ALTER TABLE `creature_summon_groups` AUTO_INCREMENT = ', @AI);
PREPARE stmt FROM @SQL;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
