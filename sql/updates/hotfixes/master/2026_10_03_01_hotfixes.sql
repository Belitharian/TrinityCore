-- QuestFeedbackEffect.db2 : le core charge desormais ce store, les hotfixes officiels (build 69497)
-- livres en hotfix_blob sont donc rejetes ("fill related table instead of hotfix_blob").
-- On les convertit en lignes de quest_feedback_effect (blobs decodes, memes valeurs) ; les lignes
-- hotfix_data officielles existantes (Id 110298 / 110902) restent valables.
DELETE FROM `quest_feedback_effect` WHERE `ID` IN (940, 1379, 1440);
INSERT INTO `quest_feedback_effect` (`ID`,`InteractCursor`,`FileDataID`,`MinimapAtlasMemberID`,`AttachPoint`,`PassiveHighlightColorType`,`Priority`,`Flags`,`SpellID`,`VerifiedBuild`) VALUES
(940, '', 0, 4721, 18, 0, 0, 0, 0, 69497),
(1379, 'openhandglow', 0, 8594, 18, 14, 0, 0, 312056, 69497),
(1440, 'openhandglow', 0, 7259, 18, 14, 0, 0, 312056, 69497);
DELETE FROM `hotfix_blob` WHERE `TableHash`=2043095305 AND `RecordId` IN (940, 1379, 1440);

-- Purge de Dalaran : lignes frFR orphelines de BroadcastText 2 et 6 (aucun BroadcastText 2/6),
-- doublons des textes officiels 69613 / 69617 deja localises.
DELETE FROM `broadcast_text_locale` WHERE `ID` IN (2, 6) AND `locale`='frFR' AND `VerifiedBuild`=-1;

-- Gossip 65008 (PNJ 500030) "Please, take me to her." : OptionBroadcastTextID 850026 n'existait pas.
DELETE FROM `broadcast_text` WHERE `ID`=850026 AND `VerifiedBuild`=-1;
INSERT INTO `broadcast_text` (`Text`,`Text1`,`ID`,`LanguageID`,`ConditionID`,`EmotesID`,`Flags`,`ChatBubbleDurationMs`,`VoiceOverPriorityID`,`SoundKitID1`,`SoundKitID2`,`EmoteID1`,`EmoteID2`,`EmoteID3`,`EmoteDelay1`,`EmoteDelay2`,`EmoteDelay3`,`VerifiedBuild`) VALUES
('Please, take me to her.', 'Please, take me to her.', 850026, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1);

DELETE FROM `broadcast_text_locale` WHERE `ID`=850026 AND `locale`='frFR';
INSERT INTO `broadcast_text_locale` (`ID`,`locale`,`Text_lang`,`Text1_lang`,`VerifiedBuild`) VALUES
(850026, 'frFR', 'Emmenez-moi auprès d''elle.', 'Emmenez-moi auprès d''elle.', -1);

SET @TABLE_HASH := 35137211;   -- BroadcastText.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
SET @UNIQUE_ID  := TO_DAYS('2026-10-03');   -- Un UniqueId par jour : meme valeur pour tous les hotfixes du jour, +1 le lendemain
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId`=850026;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID, @UNIQUE_ID, @TABLE_HASH, 850026, 1, -1);
