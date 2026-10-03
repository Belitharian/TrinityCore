-- Cle primaire sur creature_summon_groups, pour pouvoir editer les lignes dans les outils
-- (la table n'avait aucune cle). Les lignes existantes sont numerotees automatiquement.
ALTER TABLE `creature_summon_groups` ADD COLUMN `ID` int unsigned NOT NULL AUTO_INCREMENT FIRST, ADD PRIMARY KEY (`ID`);
