-- ============================================================================
-- Les Ruines de Theramore - animations des repliques de Jaina (64727), debout uniquement
--   Sans animation volontairement :
--     15, 17, 23 : dites a genoux (Kinndy, Peine)
--     27, 28     : dites pendant qu'elle canalise sur l'iris
--     16, 22, 29-35 : emotes en texte
--   0-6 et 14 en avaient deja une. Les lignes officielles 11 et 12 ne sont pas touchees.
-- ============================================================================

UPDATE `creature_text` SET `Emote` = 18  WHERE `CreatureID` = 64727 AND `GroupID` = 18 AND `ID` = 0; -- Les generaux, "Ils sont tous la"          : pleure (ONESHOT_CRY)
UPDATE `creature_text` SET `Emote` = 1   WHERE `CreatureID` = 64727 AND `GroupID` = 19 AND `ID` = 0; -- Le soldat qui flotte                        : parle (ONESHOT_TALK)
UPDATE `creature_text` SET `Emote` = 274 WHERE `CreatureID` = 64727 AND `GroupID` = 20 AND `ID` = 0; -- Le rat, "Rien n'a survecu"                  : secoue la tete (ONESHOT_NO)
UPDATE `creature_text` SET `Emote` = 5   WHERE `CreatureID` = 64727 AND `GroupID` = 21 AND `ID` = 0; -- "Ou etes-vous ?!"                           : s'exclame (ONESHOT_EXCLAMATION)
UPDATE `creature_text` SET `Emote` = 1   WHERE `CreatureID` = 64727 AND `GroupID` = 24 AND `ID` = 0; -- Les docks                                   : parle
UPDATE `creature_text` SET `Emote` = 274 WHERE `CreatureID` = 64727 AND `GroupID` = 25 AND `ID` = 0; -- "Non... Le cratere."                        : secoue la tete
UPDATE `creature_text` SET `Emote` = 1   WHERE `CreatureID` = 64727 AND `GroupID` = 26 AND `ID` = 0; -- Devant l'iris, avant de canaliser          : parle
