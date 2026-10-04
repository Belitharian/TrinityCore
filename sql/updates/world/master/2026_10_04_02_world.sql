-- Hot Streak PNJ : retour au driver officiel 44448 (vraie aura, proc avec le crit fiable meme pour les projectiles)
-- spell_mage_hot_streak_npc (2026_10_04_01) appelait IsHitCrit sur des sorts a projectile -> assertion du core
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_mage_hot_streak_npc';

-- 44448 - Pyroblast Clearcasting Driver : filtre famille/masques deplace dans spell_mage_hot_streak (joueurs),
-- les sorts de feu des PNJ (famille 0) sont acceptes par ID pour les creatures
UPDATE `spell_proc` SET `SpellFamilyName`=0, `SpellFamilyMask0`=0, `SpellFamilyMask1`=0 WHERE `SpellId`=44448;

-- Sunreaver Pyromancer : 195283 (Dummy, pas d'aura) remplace par le driver 44448
UPDATE `creature_template_addon` SET `auras`='448601 44448 205026' WHERE `entry`=68757;
UPDATE `creature_addon` SET `auras`='448601 44448 205026' WHERE `guid`=50000273;
UPDATE `creature_addon` SET `auras`='406138 448601 44448 205026' WHERE `guid`=50000453;
