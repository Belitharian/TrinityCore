-- Ce qui devait etre fait (500000) : quete de niveau 90, Midnight
-- ContentTuning 5381 (ExpansionID 11, MinLevel 90, MaxLevel 90) au lieu de 59 (Mists of Pandaria)
UPDATE `quest_template` SET `ContentTuningID`=5381, `Expansion`=11 WHERE `ID`=500000;
