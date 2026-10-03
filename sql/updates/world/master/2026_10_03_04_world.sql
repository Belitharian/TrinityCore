-- Scénarios custom (maps 5000, 5001, 5002) : retrait des flags PvP des créatures
-- (un PNJ flaggé PvP en combat passe le joueur PvP dès qu'il le cible avec un sort)

-- PvPFlags des entries présentes uniquement sur les maps custom
UPDATE `creature_template_addon` SET `PvPFlags` = 0 WHERE `entry` IN (64560, 68695, 68716);

-- Entries partagées avec des zones officielles : on neutralise uniquement les spawns custom via creature_addon
INSERT IGNORE INTO `creature_addon` (`guid`, `PathId`, `mount`, `MountCreatureID`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`)
SELECT c.`guid`, t.`PathId`, t.`mount`, t.`MountCreatureID`, t.`StandState`, t.`AnimTier`, t.`VisFlags`, t.`SheathState`, 0, t.`emote`, t.`aiAnimKit`, t.`movementAnimKit`, t.`meleeAnimKit`, t.`visibilityDistanceType`, t.`auras`
FROM `creature` c JOIN `creature_template_addon` t ON t.`entry` = c.`id`
WHERE c.`map` IN (5000, 5001, 5002) AND t.`PvPFlags` <> 0;

UPDATE `creature_addon` a JOIN `creature` c ON c.`guid` = a.`guid` SET a.`PvPFlags` = 0 WHERE c.`map` IN (5000, 5001, 5002) AND a.`PvPFlags` <> 0;

-- UNIT_FLAG_PVP_ENABLING (0x1000, obsolète) sur les templates propres aux scénarios
UPDATE `creature_template` SET `unit_flags` = `unit_flags` & ~4096 WHERE `entry` IN (67997, 68043, 68045, 68046, 68049, 68050, 68051, 68587, 68589, 68609, 68616, 68617, 68632, 68677, 68678, 68679, 68680, 68687, 68692, 68695, 68708, 68710, 68714, 68715, 68716, 68733, 68751, 68756, 68757, 68760, 68761, 68956, 114995, 500016, 500017, 500029, 550003);

-- ... et sur les spawns des maps custom
UPDATE `creature` SET `unit_flags` = `unit_flags` & ~4096 WHERE `map` IN (5000, 5001, 5002) AND `unit_flags` & 4096;

-- Factions 150 (Theramore) et 2129 : FACTION_TEMPLATE_FLAG_PVP (0x800) force le flag PvP au chargement
-- (Creature::UpdateEntry), sauf si un addon existe : LoadCreaturesAddon remplace alors les PvPFlags
INSERT IGNORE INTO `creature_template_addon` (`entry`, `PvPFlags`) VALUES
(58777, 0), (58788, 0), (58840, 0), (59654, 0), (64564, 0), (64565, 0), (64727, 0), (65680, 0), (68760, 0), (68761, 0), (121953, 0),
(500000, 0), (500001, 0), (500002, 0), (500006, 0), (500007, 0), (500008, 0), (500009, 0), (500010, 0), (500014, 0);
UPDATE `creature_template_addon` SET `PvPFlags` = 0 WHERE `entry` IN (58612, 58913, 59317, 59595, 59596, 64560, 68756, 68757);
