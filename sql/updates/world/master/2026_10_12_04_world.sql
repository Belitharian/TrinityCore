-- ============================================================================
-- Les Ruines de Theramore - Jaina devant les morts qu'elle croise (emotes)
--   30 : debut du parcours          (scenario_ruins_of_theramore::OnJainaArrived)
--   31, 32 : pauses du chemin vers l'armurerie, 34, 35 : vers Kinndy (npc_jaina_ruins::WaypointReached)
--   33 : arret de l'armurerie, avant le soldat qui flotte
-- ============================================================================

DELETE FROM `creature_text` WHERE `CreatureID` = 64727 AND `GroupID` BETWEEN 30 AND 35;
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(64727,30,0,'Jaina contemple un soigneur étendu, un blessé encore dans les bras.',16,0,100,0,0,0,0,0,0,'Jaina - Traces : le soigneur et son blesse (emote)'),
(64727,31,0,'Jaina s''arrête devant un cavalier, tombé avec sa monture.',16,0,100,0,0,0,0,0,0,'Jaina - Traces : le cavalier et son cheval (emote)'),
(64727,32,0,'Jaina remarque que ces soldats sont tombés l''arme encore au fourreau.',16,0,100,0,0,0,0,0,0,'Jaina - Traces : les soldats, l''arme au fourreau (emote)'),
(64727,33,0,'Près de l''armurerie, un soldat gît dans une flaque de sang.',16,0,100,0,0,0,0,0,0,'Jaina - Traces : le soldat dans son sang (emote)'),
(64727,34,0,'Jaina reconnaît Thaddus Coup-Robuste parmi les corps.',16,0,100,0,0,0,0,0,0,'Jaina - Traces : Thaddus et l''ingenieur (emote)'),
(64727,35,0,'Sur un toit effondré, l''armure du lieutenant Aden a viré au violet sombre.',16,0,100,0,0,0,0,0,0,'Jaina - Traces : Aden sur son toit (emote)');
