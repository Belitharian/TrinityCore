-- Comet Barrage (354938) : script de sort deplace dans spell_custom.cpp
DELETE FROM `spell_script_names` WHERE `spell_id` = 354938 AND `ScriptName` IN ('spell_ruins_comet_barrage', 'spell_custom_comet_barrage');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(354938, 'spell_custom_comet_barrage');
