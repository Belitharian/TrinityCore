-- ============================================================================
-- Les Ruines de Theramore - cadavres de l'armurerie (Tides of War)
--   Un fantassin mort dans une flaque de sang rouge ; a trois pas de lui, un
--   chevalier qui flotte a hauteur des yeux de Jaina.
--   Le traitement est dans scenario_ruins_of_theramore::SetupCorpse (StringId).
-- ============================================================================

-- Fantassin de Theramore (58612) : sang, pas de trace arcanique
UPDATE `creature` SET `StringId` = 'ruins_corpse_blood' WHERE `guid` = 50000572 AND `id` = 58612;

-- Chevalier de Theramore (59654) : sans gravite, releve de 1,7 m (a hauteur des yeux)
UPDATE `creature` SET `StringId` = 'ruins_corpse_floating', `position_z` = 10.91 WHERE `guid` = 50000573 AND `id` = 59654;
