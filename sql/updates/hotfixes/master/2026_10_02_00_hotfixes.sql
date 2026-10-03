-- Sorts propres aux PNJ Rok'nah (et a ceux qui partagent leurs scripts) : degats ramenes a l'echelle
-- d'une puissance "joueur ilvl 344" (~3400 de SP/AP, voir creature_template_ilvl).
-- Les sorts de classe joueur ne sont pas touches : ils scalent desormais sur cette SP/AP.
-- Valeur = EffectBasePoints x ExpectedStat CreatureSpellDamage (3085 au niveau 90).
-- Partages : 283410/283419 avec Theramore Footman, 283424/283426/299995 avec Warlord Rok'nah,
-- 284879 avec les mages de la Purge de Dalaran.

-- Guerrier : Frappe mortelle 18,5k -> 9,3k ; Pourfendre 15,4k + 17,3k/tick -> 6,2k + 3,1k/tick
UPDATE `spell_effect` SET `EffectBasePoints`=3   WHERE `ID`=743734; -- 283410 Mortal Strike
UPDATE `spell_effect` SET `EffectBasePoints`=2   WHERE `ID`=743747; -- 283419 Rend (direct)
UPDATE `spell_effect` SET `EffectBasePoints`=1   WHERE `ID`=743748; -- 283419 Rend (periodique)
UPDATE `spell_effect` SET `EffectBasePoints`=3   WHERE `ID`=743755; -- 283424 Execute
UPDATE `spell_effect` SET `EffectBasePoints`=2.5 WHERE `ID`=743758; -- 283426 Overpower
UPDATE `spell_effect` SET `EffectBasePoints`=3.5 WHERE `ID`=773170; -- 299995 Slam

-- Chante-loa : Eclair 37k -> 10,8k ; Chaine 21,6k -> 7,7k ; Lave 21,6k -> 9,3k ; Horions 9,3k -> 4,6k
UPDATE `spell_effect` SET `EffectBasePoints`=2.5 WHERE `ID`=755696; -- 290411 Chain Lightning
UPDATE `spell_effect` SET `EffectBasePoints`=3   WHERE `ID`=755712; -- 290423 Lava Burst
UPDATE `spell_effect` SET `EffectBasePoints`=1.5 WHERE `ID`=755710; -- 290422 Flame Shock (direct)
UPDATE `spell_effect` SET `EffectBasePoints`=0.5 WHERE `ID`=755711; -- 290422 Flame Shock (periodique)
UPDATE `spell_effect` SET `EffectBasePoints`=1.5 WHERE `ID`=755743; -- 290441 Frost Shock

-- Megere : Rafale 3x18,5k -> 3x6,2k ; Nova 23k -> 9,3k ; Cone 24,7k -> 9,3k
UPDATE `spell_effect` SET `EffectBasePoints`=2   WHERE `ID`=746252; -- 284860 Flurry (projectile)
UPDATE `spell_effect` SET `EffectBasePoints`=3   WHERE `ID`=746286; -- 284879 Frost Nova
UPDATE `spell_effect` SET `EffectBasePoints`=3   WHERE `ID`=759083; -- 292294 Cone of Cold

-- Gangre-lanceur : Drain de vie 771k/tick (et soin 1,17M/tick) -> 4,6k ; Corruption 5,6k -> 3,1k
UPDATE `spell_effect` SET `EffectBasePoints`=1.5 WHERE `ID`=206281; -- 149992 Drain Life (degats)
UPDATE `spell_effect` SET `EffectBasePoints`=1.5 WHERE `ID`=206282; -- 149992 Drain Life (soin)
UPDATE `spell_effect` SET `EffectBasePoints`=1   WHERE `ID`=470789; -- 251406 Corruption

-- Nouvelles lignes (valeurs DB2 12.1.5.70077, seul EffectBasePoints change)
DELETE FROM `spell_effect` WHERE `ID` IN (1248377, 746276, 356166);
INSERT INTO `spell_effect` (`ID`, `EffectAura`, `DifficultyID`, `EffectIndex`, `Effect`, `EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`, `EffectBonusCoefficient`, `EffectChainAmplitude`, `EffectChainTargets`, `EffectItemType`, `EffectMechanic`, `EffectPointsPerResource`, `EffectPosFacing`, `EffectRealPointsPerLevel`, `EffectTriggerSpell`, `BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`, `Variance`, `ResourceCoefficient`, `GroupSizeBasePointsCoefficient`, `EffectBasePoints`, `ScalingClass`, `TargetNodeGraph`, `EffectMiscValue1`, `EffectMiscValue2`, `EffectRadiusIndex1`, `EffectRadiusIndex2`, `EffectSpellClassMask1`, `EffectSpellClassMask2`, `EffectSpellClassMask3`, `EffectSpellClassMask4`, `ImplicitTarget1`, `ImplicitTarget2`, `SpellID`, `VerifiedBuild`) VALUES
(1248377, 0, 0, 0, 2, 0, 0,      0, 0, 1,   0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0.05, 0, 1, 3.5, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 6,  0,  1246687, -1), -- Lightning Bolt 37k -> 10,8k
(746276,  0, 0, 0, 2, 0, 131072, 0, 0, 0.8, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,    0, 1, 2,   0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 6,  0,  284874,  -1), -- Ice Lance 16k -> 6,2k
(356166,  0, 0, 0, 2, 0, 0,      0, 0, 1,   0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,    0, 1, 2.5, 0, 0, 0, 0, 14, 0, 0, 0, 0, 0, 22, 15, 235662,  -1); -- Bladestorm 25,9k/s -> 7,7k/s

-- Nouveau push pour que le client recharge les tooltips
SET @TABLE_HASH := 4030871717; -- SpellEffect.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId` IN (743734, 743747, 743748, 743755, 743758, 773170, 755696, 755712, 755710, 755711, 755743, 746252, 746286, 759083, 206281, 206282, 470789, 1248377, 746276, 356166);
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 743734, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 743747, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 743748, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 743755, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 743758, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 773170, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 755696, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 755712, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 755710, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 755711, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 755743, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 746252, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 746286, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 759083, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 206281, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 206282, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 470789, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 1248377, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 746276, 1, -1),
(@HOTFIX_ID, @HOTFIX_ID, @TABLE_HASH, 356166, 1, -1);
