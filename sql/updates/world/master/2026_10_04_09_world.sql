-- Ce qui devait etre fait (500000)
--   * scene 150 sans flag : le flag 0x10 (non nomme par le core) faisait un fondu en fin de scene
--   * Varian, quete rendue : visible apres la scene tant que le joueur reste a moins de 30 m,
--     plus jamais une fois qu'il s'est eloigne (condition_what_had_to_be_done_scene_over)
UPDATE `scene_template` SET `Flags`=0 WHERE `SceneId`=150;

UPDATE `conditions` SET `Comment`='Roi Varian Wrynn visible apres la scene 313 (quete 500000 rendue) tant que le joueur reste pres de lui'
WHERE `SourceTypeOrReferenceId`=32 AND `SourceGroup`=5 AND `SourceEntry`=68690 AND `ElseGroup`=1;
