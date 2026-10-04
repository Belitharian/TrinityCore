-- Hot Streak PNJ : 195283 n'a qu'un effet Dummy (pas d'aura), le script passe sur les sorts de feu des PNJ
DELETE FROM `spell_proc` WHERE `SpellId`=195283;
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_mage_hot_streak_npc';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(108853,'spell_mage_hot_streak_npc'),
(257542,'spell_mage_hot_streak_npc'),
(296457,'spell_mage_hot_streak_npc'),
(338914,'spell_mage_hot_streak_npc');

-- 284594 - Penance (soin PNJ) : Divine Aegis sur les ticks critiques (procs desactives sur le tick)
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_pri_penance_npc_divine_aegis';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(284594,'spell_pri_penance_npc_divine_aegis');
