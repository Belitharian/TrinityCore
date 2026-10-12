-- ============================================================================
-- Les Ruines de Theramore - Jaina regarde les morts qu'elle croise
--   La page perdue est rapprochee du chemin : Jaina s'agenouille pour la ramasser
--   (pause du noeud 9 de JainaPathArmory).
-- ============================================================================

-- Emote : la page tombe en poussiere (comme dans le roman, au bord du cratere)
DELETE FROM `creature_text` WHERE `CreatureID` = 64727 AND `GroupID` = 29;
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(64727,29,0,'Jaina ramasse une page de sa bibliothèque. Elle tombe en poussière entre ses doigts.',16,0,100,0,0,0,0,0,0,'Jaina - Traces : la page (emote)');

-- Page perdue (620533) : 1,5 m devant le noeud 9, dans la direction de marche
UPDATE `gameobject` SET `position_x` = -3731.54, `position_y` = -4325.81, `position_z` = 9.89 WHERE `guid` = 60000542 AND `id` = 620533;
