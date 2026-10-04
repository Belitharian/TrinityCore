-- ============================================================================
-- Ce qui devait etre fait (500000) : copie de la quete 32423
--   * donnee par Jaina (68677) a tout le groupe a la fin de la Purge de Dalaran
--   * rendue au roi Varian Wrynn (68690) au donjon de Hurlevent (guid 50000562)
--   * au rendu : scene 313, Jaina arrive de Dalaran par son portail
--     (quest_what_had_to_be_done_stormwind, hotfix SceneScriptText du 2026-10-04)
-- Pas d'objectif : la quete est terminee des qu'elle est prise.
-- RewardSpell 135894 (scene 150 au territoire du Lion) est retire : la scene est
-- lancee par le script de quete.
-- ============================================================================

SET @QUEST := 500000;

DELETE FROM `quest_template` WHERE `ID`=@QUEST;
DROP TEMPORARY TABLE IF EXISTS `tmp_quest_32423`;
CREATE TEMPORARY TABLE `tmp_quest_32423` SELECT * FROM `quest_template` WHERE `ID`=32423;
UPDATE `tmp_quest_32423` SET
    `ID`=@QUEST,
    `RewardSpell`=0,
    `LogDescription`='Take the portal to Stormwind, then speak with King Varian Wrynn in Stormwind Keep.',
    `QuestDescription`='Aethas has escaped, but Dalaran is now closed to the Horde. King Varian must hear it from the Kirin Tor before he hears it from anyone else.$b$bReturn to Stormwind and wait for me in the Keep. I will join you there shortly.',
    `QuestCompletionLog`='Speak with King Varian Wrynn in Stormwind Keep.',
    `VerifiedBuild`=0;
INSERT INTO `quest_template` SELECT * FROM `tmp_quest_32423`;
DROP TEMPORARY TABLE `tmp_quest_32423`;

DELETE FROM `quest_template_locale` WHERE `ID`=@QUEST AND `locale`='frFR';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES
(@QUEST,'frFR','Ce qui devait être fait','Prenez le portail vers Hurlevent, puis parlez au roi Varian Wrynn dans le donjon de Hurlevent.','Aethas nous a échappé, mais Dalaran est désormais fermée à la Horde. Le roi Varian doit l''apprendre du Kirin Tor avant de l''entendre de qui que ce soit d''autre.$b$bRetournez à Hurlevent et attendez-moi au donjon. Je vous y rejoindrai sans tarder.','','','','','','Parlez au roi Varian Wrynn dans le donjon de Hurlevent.',0);

DELETE FROM `quest_template_addon` WHERE `ID`=@QUEST;
INSERT INTO `quest_template_addon` (`ID`,`ScriptName`) VALUES
(@QUEST,'quest_what_had_to_be_done_stormwind');

-- Texte de Varian au rendu : il ne sait encore rien, Jaina arrive juste apres (scene).
DELETE FROM `quest_offer_reward` WHERE `ID`=@QUEST;
INSERT INTO `quest_offer_reward` (`ID`,`Emote1`,`Emote2`,`Emote3`,`Emote4`,`EmoteDelay1`,`EmoteDelay2`,`EmoteDelay3`,`EmoteDelay4`,`RewardText`,`VerifiedBuild`) VALUES
(@QUEST,6,0,0,0,0,0,0,0,'$n? You have come from Dalaran? We have lost all contact with the city since this morning. What is going on over there?',0);
DELETE FROM `quest_offer_reward_locale` WHERE `ID`=@QUEST AND `locale`='frFR';
INSERT INTO `quest_offer_reward_locale` (`ID`,`locale`,`RewardText`,`VerifiedBuild`) VALUES
(@QUEST,'frFR','$n ? Vous arrivez de Dalaran ? Nous n''avons plus aucune nouvelle de la cité depuis ce matin. Que se passe-t-il là-bas ?',0);

DELETE FROM `creature_queststarter` WHERE `quest`=@QUEST;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(68677,@QUEST,0);   -- Dame Jaina Portvaillant (Purge de Dalaran)

DELETE FROM `creature_questender` WHERE `quest`=@QUEST;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(68690,@QUEST,0);   -- Roi Varian Wrynn

-- Varian (68690, seul spawn : donjon de Hurlevent) n'est visible qu'avec la quete en cours
-- ou terminee. Au rendu, il disparait et l'acteur de la scene prend sa place.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=32 AND `SourceGroup`=5 AND `SourceEntry`=68690;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(32,5,68690,0,0,47,0,@QUEST,10,0,'',0,0,0,'','Roi Varian Wrynn visible si la quete 500000 est en cours ou terminee');
