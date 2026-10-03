-- Creatures of each spawn region, for criteria type 64 (KilledAllUnitsInSpawnRegion)
DROP TABLE IF EXISTS `creature_spawn_region`;
CREATE TABLE `creature_spawn_region` (
  `SpawnRegionId` int unsigned NOT NULL,
  `SpawnId` bigint unsigned NOT NULL,
  PRIMARY KEY (`SpawnRegionId`,`SpawnId`),
  KEY `idx_spawn` (`SpawnId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
