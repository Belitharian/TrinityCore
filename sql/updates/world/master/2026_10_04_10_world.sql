-- Penance (soin PNJ) : spell_pri_penance_npc_divine_aegis (284594) appelait IsHitCrit -> assertion du core.
-- Remplace par spell_pri_penance_npc_heal (284593) qui relance le tick avec les procs autorises : Divine Aegis proc normalement.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_pri_penance_npc_divine_aegis','spell_pri_penance_npc_heal');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(284593,'spell_pri_penance_npc_heal');
