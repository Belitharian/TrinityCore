-- Talents des PNJ (creature_template_addon / creature_addon) : corrections pour le modele de degats "joueur"

-- 29725 - Sudden Death : la ligne custom donnait 2 charges au passif, retire apres 2 procs (DB2 : ProcCharges = 0)
UPDATE `spell_proc` SET `Charges`=0 WHERE `SpellId`=29725;

-- 47515 - Divine Aegis : les soins de PNJ sont de famille 0, le filtre Pretre passe dans spell_pri_divine_aegis
UPDATE `spell_proc` SET `SpellFamilyName`=0 WHERE `SpellId`=47515;

-- 195283 - Hot Streak : version PNJ (Heating Up / Hot Streak! sur les crits des sorts de feu de PNJ)
DELETE FROM `spell_proc` WHERE `SpellId`=195283;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(195283,0x04,0,0x00000000,0x00000000,0x00000000,0x00000000,0x10000,0x0,0x1,0x2,0x0,0x0,0x0,0,100,0,0);

DELETE FROM `spell_script_names` WHERE `spell_id`=195283 AND `ScriptName`='spell_mage_hot_streak_npc';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(195283,'spell_mage_hot_streak_npc');

-- Theramore Footman (guid 50000063) : aura "0" invalide en tete de liste
UPDATE `creature_addon` SET `auras`='29725 184361 386196 382549' WHERE `guid`=50000063;
