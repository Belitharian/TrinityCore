-- Ce qui devait etre fait (500000)
--   * classee sous Dalaran (zone 7502, celle du scenario 5002) au lieu de -397 « Campagne pandarene »
--   * point de rendu (?) sur la carte de Hurlevent (UiMap 84) : roi Varian Wrynn au donjon (guid 50000562)
UPDATE `quest_template` SET `QuestSortID`=7502 WHERE `ID`=500000;

DELETE FROM `quest_poi` WHERE `QuestID`=500000;
INSERT INTO `quest_poi` (`QuestID`,`BlobIndex`,`Idx1`,`ObjectiveIndex`,`QuestObjectiveID`,`QuestObjectID`,`MapID`,`UiMapID`,`Priority`,`Flags`,`WorldEffectID`,`PlayerConditionID`,`NavigationPlayerConditionID`,`SpawnTrackingID`,`AlwaysAllowMergingBlobs`,`VerifiedBuild`) VALUES
(500000,0,0,-1,0,0,0,84,0,3,0,0,0,0,0,0);

DELETE FROM `quest_poi_points` WHERE `QuestID`=500000;
INSERT INTO `quest_poi_points` (`QuestID`,`Idx1`,`Idx2`,`X`,`Y`,`Z`,`VerifiedBuild`) VALUES
(500000,0,0,-8394,313,147,0);
