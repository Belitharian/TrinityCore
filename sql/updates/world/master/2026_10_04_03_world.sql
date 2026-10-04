-- ============================================================================
-- Purge de Dalaran - fin du scenario
--   * L'evasion (flashback) : Jaina arrive trop tard, Aethas lui repond
--   * Entre les mains du chef : Jaina conclut, Kalecgos s'inquiete pour elle
--   * Rommath : second « Restez pres de moi » remplace
-- Sound = 0 : repliques a doubler. Duration estimee d'apres le texte (14 car/s),
-- comme _Tools/UpdateCreatureTextDurations.ps1 pour les lignes sans son.
-- ============================================================================

DELETE FROM `creature_text` WHERE `CreatureID`=68589 AND `GroupID`=8;
DELETE FROM `creature_text` WHERE `CreatureID`=68677 AND `GroupID` IN (9,10,11,12,13);
DELETE FROM `creature_text` WHERE `CreatureID`=68679 AND `GroupID`=2;
DELETE FROM `creature_text` WHERE `CreatureID`=64565 AND `GroupID` IN (17,18);
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
-- Grand Magistere Rommath
(68589,8,0,'Laissons Surdiel s''occuper d''elle. Avançons, Aethas ne doit pas être loin !',12,0,100,1,0,0,0,0,1,'Rommath - Prison, Surdiel retient Narasi'),
-- Jaina du souvenir
(68677,9,0,'Rommath ! Vous ne quitterez pas Dalaran avec ce traître !',14,0,100,5,0,0,0,0,3,'Jaina - Evasion, arrive trop tard'),
-- Aethas Saccage-Soleil
(68679,2,0,'Nous avons servi le Kirin Tor pendant des siècles, Portvaillant. Souvenez-vous de ce jour : c''est vous qui l''avez trahi.',12,0,100,1,0,0,0,0,1,'Aethas - Evasion, reponse a Jaina'),
-- Jaina, retour au present
(68677,10,0,'Voilà comment Aethas nous a échappé. À l''heure qu''il est, il se trouve à Lune-d''Argent, sous la protection de Rommath.',12,0,100,1,0,0,0,0,1,'Jaina - Conclusion, fuite d''Aethas'),
(68677,11,0,'Les Saccage-Soleil qui n''ont pas pu fuir sont enfermés au Fort-Pourpre. Désormais, Dalaran est fermée à la Horde, et le Kirin Tor se tiendra aux côtés de l''Alliance.',12,0,100,1,0,0,0,0,1,'Jaina - Conclusion, Dalaran fermee a la Horde'),
(68677,12,0,'Ils ont ouvert nos portails à la Horde, Kalec. Theramore n''est plus qu''un cratère. Je ne laisserai pas Garrosh nous frapper une seconde fois.',12,0,100,274,0,0,0,0,1,'Jaina - Conclusion, reponse a Kalecgos'),
(68677,13,0,'Héros, retournez à Hurlevent et faites votre rapport au roi Varian. Dites-lui que le Kirin Tor répondra à son appel.',12,0,100,1,0,0,0,0,1,'Jaina - Conclusion, portail vers Hurlevent'),
-- Kalecgos
(64565,17,0,'Jaina... Ces mages ont étudié et combattu à vos côtés pendant des années. Vous les avez enchaînés comme des criminels.',12,0,100,1,0,0,0,0,1,'Kalecgos - Purge, s''inquiete pour Jaina'),
(64565,18,0,'Ce n''est pas votre colère que je crains, Jaina. C''est ce qu''elle est en train de faire de vous.',12,0,100,1,0,0,0,0,1,'Kalecgos - Purge, depart');

UPDATE `creature_text` SET `Duration`=ROUND(CHAR_LENGTH(`Text`) / 14 * 1000)
WHERE (`CreatureID`=68589 AND `GroupID`=8)
   OR (`CreatureID`=68677 AND `GroupID` IN (9,10,11,12,13))
   OR (`CreatureID`=68679 AND `GroupID`=2)
   OR (`CreatureID`=64565 AND `GroupID` IN (17,18));
