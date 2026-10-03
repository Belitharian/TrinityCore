-- 79684 - Clearcasting : un seul tirage par incantation (phase CAST) au lieu d'un par cible touchée
UPDATE `spell_proc` SET `SpellPhaseMask`=0x1 WHERE `SpellId`=79684;
