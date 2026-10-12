-- ============================================================================
-- Les Ruines de Theramore - Jaina (64727) frappe plus fort contre les pillards
--   DamageModifier pondere sa SP "joueur" (voir Creature::GetPlayerLikePowerForLevel) :
--   1 -> 1.5 dans la difficulte du scenario (12). Les BasePoints des sorts ne bougent pas.
--   Les coups critiques sont autorises par l'AI (SetSpellCritChance).
-- ============================================================================

UPDATE `creature_template_difficulty` SET `DamageModifier` = 1.5 WHERE `Entry` = 64727 AND `DifficultyID` = 12;
