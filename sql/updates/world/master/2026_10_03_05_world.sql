-- Rewind Time (101590) : retour du joueur a son clone fige (Purge de Dalaran, High Arcanist Savor)
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_rewind_time';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(101590, 'spell_rewind_time');

-- Arcane Deflection (1290802 face/dos, 1290806 cotes) : absorption directionnelle de High Arcanist Savor en vitesse reduite
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_savor_arcane_deflection';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1290802, 'spell_savor_arcane_deflection'),
(1290806, 'spell_savor_arcane_deflection');

-- Mass Teleport (446636) : la teleportation de Vereesa ne prend que les PNJ autour d'elle
-- (effets 0 et 1 en TARGET_UNIT_DEST_AREA_ENTRY : les joueurs sont exclus, un joueur etant aussi une unite)
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_purge_mass_teleport';
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=13 AND `SourceEntry`=446636;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(13, 3, 446636, 0, 0, 52, 0, 64, 0, 0, '', 1, 0, 0, '', 'Mass Teleport - pas de joueurs (TYPEMASK_PLAYER)');

-- Horde (195838) : pendant l'illusion, les PNJ jugent le joueur sur sa faction et plus sur sa reputation
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_purge_horde_illusion_reactions';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(195838, 'spell_purge_horde_illusion_reactions');

-- Temporal Burst (376679 effet 1, 384652 effet 0) de Narasi : cibles en TARGET_UNIT_SRC_AREA_ENTRY, qui prennent
-- toutes les unites autour d'elle sans controle de faction ni d'immunite (le joueur sous illusion compris).
-- Le filtre ne garde que ce que le lanceur peut attaquer : ses ennemis, jamais les joueurs (Narasi est ImmuneToPC).
DELETE FROM `spell_script_names` WHERE `spell_id` IN (376679, 384652) AND `ScriptName`='spell_purge_hostile_area_only';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(376679, 'spell_purge_hostile_area_only'),
(384652, 'spell_purge_hostile_area_only');
