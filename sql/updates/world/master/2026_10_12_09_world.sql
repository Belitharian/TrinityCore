-- ============================================================================
-- Les Ruines de Theramore - Jaina hurle quand Kinndy tombe en poussiere
--   Emote 64727 / 17 : SoundKit 800002 (characterroarshumanfemale.ogg, fdid 542513),
--   cree par sql/custom/hotfixes/2026_10_12_06_hotfixes_ruins_jaina_scream.sql
-- ============================================================================

UPDATE `creature_text` SET `Sound` = 800002 WHERE `CreatureID` = 64727 AND `GroupID` = 17 AND `ID` = 0;
