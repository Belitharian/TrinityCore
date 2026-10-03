-- Puissance des PNJ calquee sur celle d'un joueur equipe (SP et AP de base)
-- Creature::GetPlayerLikePowerForLevel : stat principale de base de la classe au niveau
-- + budget RandPropPoints(ItemLevel) d'un equipement epique de reference, x DamageModifier.
-- Sans ligne pour le ContentTuning du PNJ : stat principale attendue (ExpectedStat PlayerPrimaryStat).
DROP TABLE IF EXISTS `creature_template_ilvl`;
CREATE TABLE `creature_template_ilvl` (
  `ContentTuningID` INT UNSIGNED NOT NULL,
  `ItemLevel` INT UNSIGNED NOT NULL DEFAULT 0,
  `Comment` VARCHAR(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`ContentTuningID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 3025 : ContentTuning des PNJ de scenario (difficulte 12, niveau 90)
-- ilvl 344 = equipement actuel des personnages (ItemScalingConfig 453) -> ~3400 de stat principale
INSERT INTO `creature_template_ilvl` (`ContentTuningID`, `ItemLevel`, `Comment`) VALUES
(3025, 344, 'Scenarios custom niveau 90');
