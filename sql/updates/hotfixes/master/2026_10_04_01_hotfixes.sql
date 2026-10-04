-- ============================================================================
-- Dame Jaina Portvaillant (68677, Purge de Dalaran) : apparence cote client
-- Les acteurs de scene (SpawnActor par entry : scene 313 a Hurlevent) ne lisent ni
-- creature_template_model ni le spawn : le modele vient de CreatureXDisplayInfo
-- (ligne Blizzard 118400 : 47143, jaina2.m2 de MoP), l'arme de Creature.db2
-- (AlwaysItem : baton 153575). On reprend l'apparence du spawn de fin du scenario
-- (guid 50000237) : modele 6000002 (jaina6.m2), arme 72808 (creature_equip_template 68677/1).
-- Aucune des deux tables n'est chargee par le core : hotfix_blob.
-- ============================================================================

SET @HASH_CREATURE := 3386291891;               -- Creature.db2
SET @HASH_CREATURE_X_DISPLAY_INFO := 600565378; -- CreatureXDisplayInfo.db2

DELETE FROM `hotfix_blob` WHERE `locale`='frFR' AND ((`TableHash`=@HASH_CREATURE AND `RecordId`=68677) OR (`TableHash`=@HASH_CREATURE_X_DISPLAY_INFO AND `RecordId`=118400));
INSERT INTO `hotfix_blob` (`TableHash`,`RecordId`,`locale`,`Blob`,`VerifiedBuild`) VALUES
-- Creature (layout 6E14C900, ID non inline) : Name, NameAlt, Title, TitleAlt (chaines),
--   Classification i8, CreatureType u8, CreatureFamily u16, StartAnimState i8,
--   DisplayID i32[4], DisplayProbability f32[4], AlwaysItem i32[3]
-- "Dame Jaina Portvaillant", "", "Chef du Kirin Tor", "", 1, 7, 0, 0, {6000002, 0, 0, 0}, {1, 0, 0, 0}, {72808, 0, 0}
(@HASH_CREATURE, 68677, 'frFR', 0x44616D65204A61696E6120506F72747661696C6C616E74000043686566206475204B6972696E20546F7200000107000000828D5B000000000000000000000000000000803F000000000000000000000000681C01000000000000000000, -1),
-- CreatureXDisplayInfo (layout 2EA19FCF, ID non inline, CreatureID en relation non inline, en fin de record) :
--   CreatureDisplayInfoID i32, Probability f32, Scale f32, OrderIndex u8, Field_11_0_0_54210_004 u16, CreatureID i32
-- 6000002, 1, 1, 0, 0, 68677 (echelle 1 : celle appliquee au `modelid` d'un spawn)
(@HASH_CREATURE_X_DISPLAY_INFO, 118400, 'frFR', 0x828D5B000000803F0000803F000000450C0100, -1);

SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
SET @UNIQUE_ID  := TO_DAYS('2026-10-04');   -- Un UniqueId par jour : meme valeur pour tous les hotfixes du jour, +1 le lendemain
DELETE FROM `hotfix_data` WHERE (`TableHash`=@HASH_CREATURE AND `RecordId`=68677) OR (`TableHash`=@HASH_CREATURE_X_DISPLAY_INFO AND `RecordId`=118400);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID,     @UNIQUE_ID, @HASH_CREATURE,                68677,  1, -1),
(@HOTFIX_ID + 1, @UNIQUE_ID, @HASH_CREATURE_X_DISPLAY_INFO, 118400, 1, -1);
