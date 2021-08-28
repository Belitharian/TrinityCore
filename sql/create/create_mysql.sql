GRANT USAGE ON * . * TO 'trinity'@'localhost' IDENTIFIED BY 'trinity' WITH MAX_QUERIES_PER_HOUR 0 MAX_CONNECTIONS_PER_HOUR 0 MAX_UPDATES_PER_HOUR 0 ;

CREATE DATABASE `world_emerald` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

CREATE DATABASE `characters_emerald` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

CREATE DATABASE `auth_emerald` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

CREATE DATABASE `hotfixes_emerald` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

GRANT ALL PRIVILEGES ON `world_emerald` . * TO 'trinity'@'localhost' WITH GRANT OPTION;

GRANT ALL PRIVILEGES ON `characters_emerald` . * TO 'trinity'@'localhost' WITH GRANT OPTION;

GRANT ALL PRIVILEGES ON `auth_emerald` . * TO 'trinity'@'localhost' WITH GRANT OPTION;

GRANT ALL PRIVILEGES ON `hotfixes_emerald` . * TO 'trinity'@'localhost' WITH GRANT OPTION;
