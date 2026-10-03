-- Prayer of Healing (596) : XSV 457649 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=457649 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(457649, 0, 140681, 1, 0, 2, 0, 0, 0, 0, 0, 0, 596, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=457649;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 457649, 1, -1);

-- Healing Wave (77472) : XSV 312318 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=312318 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(312318, 0, 103175, 1, 0, 2, 0, 0, 0, 0, 0, 0, 77472, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=312318;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 312318, 1, -1);

-- Fire Blast (108853) : XSV 490942 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=490942 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(490942, 0, 166459, 1, 0, 2, 0, 0, 0, 0, 0, 0, 108853, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=490942;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 490942, 1, -1);

-- Arcane Familiar (210126) : XSV 441992 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=441992 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(441992, 0, 152247, 1, 0, 2, 0, 0, 0, 0, 0, 0, 210126, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=441992;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 441992, 1, -1);

-- Flurry (228596) : XSV 490934 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=490934 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(490934, 0, 166455, 1, 0, 2, 0, 0, 0, 0, 0, 0, 228596, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=490934;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 490934, 1, -1);

-- Demonbolt (264178) : XSV 265855 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=265855 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(265855, 0, 83550, 1, 0, 2, 0, 0, 0, 0, 0, 0, 264178, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=265855;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 265855, 1, -1);

-- Ultimate Penitence (421453) : XSV 441182 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=441182 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(441182, 0, 151660, 1, 0, 2, 0, 0, 0, 0, 0, 0, 421453, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=441182;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 441182, 1, -1);

