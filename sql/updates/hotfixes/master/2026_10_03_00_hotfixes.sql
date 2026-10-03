-- Ruins of Theramore - "Jaina protected" : l'evenement scripte (92, 65816) devient
-- le critere natif "Killed all units in spawn region" (64) sur l'assaut final.
UPDATE `criteria` SET `Type`=64, `Asset`=500101 WHERE `ID`=100026 AND `VerifiedBuild`=-1;

SET @TABLE_HASH := 4012104832;   -- Criteria.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
SET @UNIQUE_ID  := TO_DAYS('2026-10-03');   -- Un UniqueId par jour : meme valeur pour tous les hotfixes du jour, +1 le lendemain
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=100026;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 100026, 1, -1);

-- Portal to Stormwind (332489) : le sort declenche (Force Cast) 332487 "Oribos Teleport to Stormwind",
-- dont la ligne SpellXSpellVisual Blizzard 306698 (visuel 97317) est remplacee par le visuel de
-- teleportation de Teleport: Dalaran - Northrend (53140, SpellVisualID 182728).
DELETE FROM `spell_x_spell_visual` WHERE `ID` IN (306698, 600000) AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(306698, 0, 182728, 1, 0, 0, 0, 0, 0, 0, 0, 0, 332487, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId` IN (306698, 600000);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 306698, 1, -1);

-- Oribos Teleport to Stormwind (332487) : l'effet d'ecran du teleport (effet 15, MiscValue = duree, MiscValueB = SpellVisualKit)
-- reprend celui de Teleport: Dalaran - Northrend (53140, effet 45775 : 1500 ms, kit 208631) au lieu de 1000 ms / kit 128466.
DELETE FROM `spell_effect` WHERE `ID`=827161 AND `VerifiedBuild`=-1;
INSERT INTO `spell_effect` (`ID`,`EffectAura`,`DifficultyID`,`EffectIndex`,`Effect`,`EffectAmplitude`,`EffectAttributes`,`EffectAuraPeriod`,`EffectBonusCoefficient`,`EffectChainAmplitude`,`EffectChainTargets`,`EffectItemType`,`EffectMechanic`,`EffectPointsPerResource`,`EffectPosFacing`,`EffectRealPointsPerLevel`,`EffectTriggerSpell`,`BonusCoefficientFromAP`,`PvpMultiplier`,`Coefficient`,`Variance`,`ResourceCoefficient`,`GroupSizeBasePointsCoefficient`,`EffectBasePoints`,`ScalingClass`,`TargetNodeGraph`,`EffectMiscValue1`,`EffectMiscValue2`,`EffectRadiusIndex1`,`EffectRadiusIndex2`,`EffectSpellClassMask1`,`EffectSpellClassMask2`,`EffectSpellClassMask3`,`EffectSpellClassMask4`,`ImplicitTarget1`,`ImplicitTarget2`,`SpellID`,`VerifiedBuild`) VALUES
(827161, 0, 0, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 5.33808946609, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1500, 208631, 0, 0, 0, 0, 0, 0, 1, 17, 332487, -1);

SET @TABLE_HASH := 4030871717;   -- SpellEffect.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=827161;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 827161, 1, -1);

-- Frozen Orb (84714) : le SpellVisual 38672 n'affiche l'orbe complete qu'au lanceur (CasterSpellVisualID 40291)
-- et aux hostiles (HostileSpellVisualID 44711) ; les allies voient la version allegee. La ligne Blizzard 39087
-- pointe directement sur le visuel du lanceur, visible par tout le monde (PNJ compris).
DELETE FROM `spell_x_spell_visual` WHERE `ID`=39087 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(39087, 0, 40291, 1, 0, 0, 0, 0, 0, 0, 0, 0, 84714, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=39087;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 39087, 1, -1);

-- Visuels identiques quand un PNJ lance le sort (detectes par _Tools/CheckSpellVisuals.ps1) :
--   Spell Reflection (23920) : la ligne guerrier (PlayerCondition 18184 = classe guerrier) devient inconditionnelle, priorite 2.
--   Blizzard (284968), Mass Polymorph (383121), Purge the Wicked (451740) : pointent sur le CasterSpellVisualID.
--   Ecartes (variantes de proc / cosmetiques) : Prayer of Healing, Healing Wave, Fire Blast, Flurry, Demonbolt, Arcane Familiar, Ultimate Penitence.
-- Spell Reflection (23920) : XSV 104255 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=104255 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(104255, 0, 39267, 1, 0, 2, 0, 0, 0, 0, 0, 0, 23920, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=104255;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 104255, 1, -1);

-- Blizzard (284968) : XSV 263645 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=263645 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(263645, 0, 70167, 1, 0, 0, 0, 0, 0, 0, 0, 0, 284968, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=263645;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 263645, 1, -1);

-- Mass Polymorph (383121) : XSV 368649 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=368649 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(368649, 0, 182855, 1, 0, 0, 0, 0, 0, 0, 0, 0, 383121, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=368649;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 368649, 1, -1);

-- Purge the Wicked (451740) : XSV 430018 visible a l'identique quand un PNJ le lance
DELETE FROM `spell_x_spell_visual` WHERE `ID`=430018 AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(430018, 0, 53429, 1, 0, 0, 0, 0, 0, 0, 0, 0, 451740, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=430018;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 430018, 1, -1);


-- Soul Shard (104756 / 104759 / 123171) : fragments flottants du Glyph of Floating Shards. Toutes les lignes exigent un glyphe
-- connu (PlayerCondition 50531 / 50532 / 50538), donc rien pour un PNJ. La variante verte Fel-Touched devient inconditionnelle
-- en Priority -1 : repli seulement quand aucun glyphe ne correspond (les joueurs gardent le leur). Utilise par npc_roknah_felcaster.
DELETE FROM `spell_x_spell_visual` WHERE `ID` IN (121506, 121507, 121508) AND `VerifiedBuild`=-1;
INSERT INTO `spell_x_spell_visual` (`ID`,`DifficultyID`,`SpellVisualID`,`Probability`,`Flags`,`Priority`,`SpellIconFileID`,`ActiveIconFileID`,`ViewerUnitConditionID`,`ViewerPlayerConditionID`,`CasterUnitConditionID`,`CasterPlayerConditionID`,`SpellID`,`VerifiedBuild`) VALUES
(121506, 0, 68118, 1, 0, -1, 0, 0, 0, 0, 0, 0, 104756, -1),
(121507, 0, 68119, 1, 0, -1, 0, 0, 0, 0, 0, 0, 104759, -1),
(121508, 0, 68120, 1, 0, -1, 0, 0, 0, 0, 0, 0, 123171, -1);

SET @TABLE_HASH := 666345498;   -- SpellXSpellVisual.db2
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId` IN (121506, 121507, 121508);
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID+0, @UNIQUE_ID, @TABLE_HASH, 121506, 1, -1),
(@HOTFIX_ID+1, @UNIQUE_ID, @TABLE_HASH, 121507, 1, -1),
(@HOTFIX_ID+2, @UNIQUE_ID, @TABLE_HASH, 121508, 1, -1);

-- Fel Firebolt (104318, diablotins sauvages) : n'apparait plus dans le journal de combat
-- (SPELL_ATTR0_DO_NOT_LOG 0x100, attribut client : 65536 -> 65792).
DELETE FROM `spell_misc` WHERE `ID`=80028 AND `VerifiedBuild`=-1;
INSERT INTO `spell_misc` (`ID`,`Attributes1`,`Attributes2`,`Attributes3`,`Attributes4`,`Attributes5`,`Attributes6`,`Attributes7`,`Attributes8`,`Attributes9`,`Attributes10`,`Attributes11`,`Attributes12`,`Attributes13`,`Attributes14`,`Attributes15`,`Attributes16`,`Attributes17`,`DifficultyID`,`CastingTimeIndex`,`DurationIndex`,`PvPDurationIndex`,`RangeIndex`,`SchoolMask`,`Speed`,`LaunchDelay`,`MinDuration`,`SpellIconFileDataID`,`ActiveIconFileDataID`,`ContentTuningID`,`ShowFutureSpellPlayerConditionID`,`SpellVisualScript`,`ActiveSpellVisualScript`,`SpellID`,`VerifiedBuild`) VALUES
(80028, 65792, 0, 0, 0, 0, 4194304, 256, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 5, 0, 0, 5, 4, 30, 0, 0, 841220, 0, 0, 0, 0, 0, 104318, -1);

SET @TABLE_HASH := 3322146344;   -- SpellMisc.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=80028;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 80028, 1, -1);

-- Sands of Time (UnitPowerBar 90) : nouveau sablier a 2 charges (textures 533349 / 533350 remplacees cote client).
-- Les 2 sabliers (pas de 48 px) sont centres dans la texture 256x64 (cadre fixe de la barre) : px 80-176,
-- d'ou StartInset = EndInset = 80/256 pour que le remplissage ne couvre que les sabliers.
DELETE FROM `unit_power_bar` WHERE `ID`=90 AND `VerifiedBuild`=-1;
INSERT INTO `unit_power_bar` (`ID`,`Name`,`Cost`,`OutOfError`,`ToolTip`,`MinPower`,`MaxPower`,`StartPower`,`CenterPower`,`RegenerationPeace`,`RegenerationCombat`,`BarType`,`Flags`,`StartInset`,`EndInset`,`FileDataID1`,`FileDataID2`,`FileDataID3`,`FileDataID4`,`FileDataID5`,`FileDataID6`,`Color1`,`Color2`,`Color3`,`Color4`,`Color5`,`Color6`,`VerifiedBuild`) VALUES
(90, 'Sands of Time', 'Sands of Time', 'The hourglass has no sands remaining!', 'The number of times the Hourglass can turn back time.', 0, 2, 2, 0, 0, 0, 0, 4, 0.3125, 0.3125, 0, 533349, 533350, 0, 0, 0, -1, -1, -1, -1, -1, -16777216, -1);

DELETE FROM `unit_power_bar_locale` WHERE `ID`=90 AND `locale`='frFR' AND `VerifiedBuild`=-1;
INSERT INTO `unit_power_bar_locale` (`ID`,`locale`,`Name_lang`,`Cost_lang`,`OutOfError_lang`,`ToolTip_lang`,`VerifiedBuild`) VALUES
(90, 'frFR', 'Sables du temps', 'Sables du temps', 'Il ne reste plus de sable dans le sablier !', 'Nombre de fois que le sablier peut faire remonter le temps.', -1);

SET @TABLE_HASH := 1161940423;   -- UnitPowerBar.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=90;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 90, 1, -1);

-- Dalaran Purge - What happened? (etape 5028) : critere cache "groupe de la prison elimine".
-- Type 64 (KilledAllUnitsInSpawnRegion) sur la region 500201 (creature_summon_groups 5002/2/0),
-- arbre enfant de 1000068 en DoNotDisplay (0x2) : il ne s'affiche pas dans le suivi des objectifs.
DELETE FROM `criteria` WHERE `ID`=100056 AND `VerifiedBuild`=-1;
INSERT INTO `criteria` (`ID`,`Type`,`Asset`,`ModifierTreeId`,`StartEvent`,`StartAsset`,`StartTimer`,`FailEvent`,`FailAsset`,`Flags`,`EligibilityWorldStateID`,`EligibilityWorldStateValue`,`VerifiedBuild`) VALUES
(100056, 64, 500201, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1);

DELETE FROM `criteria_tree` WHERE `ID`=1000097 AND `VerifiedBuild`=-1;
INSERT INTO `criteria_tree` (`ID`,`Description`,`Parent`,`Amount`,`Operator`,`CriteriaID`,`OrderIndex`,`Flags`,`VerifiedBuild`) VALUES
(1000097, 'Kill the prison guards', 1000068, 1, 0, 100056, 3, 2, -1);

SET @TABLE_HASH := 4012104832;   -- Criteria.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=100056;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 100056, 1, -1);

SET @TABLE_HASH := 1255424668;   -- CriteriaTree.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=1000097;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 1000097, 1, -1);
