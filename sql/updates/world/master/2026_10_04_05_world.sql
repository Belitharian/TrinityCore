-- Jaina de la Purge de Dalaran (68677) : le modele custom 6000002 de son spawn (guid 50000237)
-- devient celui de l'entree. Les acteurs de scene (SpawnActor par entry : scene 313 a Hurlevent)
-- et les invocations du scenario (Jaina du flashback) prenaient le modele Blizzard 80016.
-- Echelle 1 : celle appliquee au `modelid` d'un spawn (DEFAULT_PLAYER_DISPLAY_SCALE).
UPDATE `creature_template_model` SET `CreatureDisplayID`=6000002, `DisplayScale`=1, `VerifiedBuild`=0 WHERE `CreatureID`=68677 AND `Idx`=0;
