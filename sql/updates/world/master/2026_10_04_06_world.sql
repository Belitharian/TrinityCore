-- ============================================================================
-- Ce qui devait etre fait (500000) : scene de fin et visibilite de Varian
--   * scene 150 (SceneID officiel du sort 135894) -> package 313, sans fondu au noir :
--     Varian finit a sa place, le vrai Varian (68690) reprend le relais
--     (scene_what_had_to_be_done : visibilite recalculee a la fin ou a l'annulation)
--   * Varian visible avec la quete en cours ou terminee, ou rendue une fois la scene
--     terminee (condition_what_had_to_be_done_scene_over) : cache pendant la scene,
--     ou l'acteur SmoothPhaseSpawnActor le remplace.
-- ============================================================================

DELETE FROM `scene_template` WHERE `SceneId`=150;
INSERT INTO `scene_template` (`SceneId`,`Flags`,`ScriptPackageID`,`Encrypted`,`ScriptName`) VALUES
(150,16,313,0,'scene_what_had_to_be_done');

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=32 AND `SourceGroup`=5 AND `SourceEntry`=68690;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(32,5,68690,0,0,47,0,500000,10,0,'',0,0,0,'','Roi Varian Wrynn visible si la quete 500000 est en cours ou terminee'),
(32,5,68690,0,1,47,0,500000,64,0,'',0,0,0,'condition_what_had_to_be_done_scene_over','Roi Varian Wrynn visible si la quete 500000 est rendue et la scene 313 terminee');
