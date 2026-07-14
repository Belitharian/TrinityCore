-- Unseen Blade (Trickster hero talent) - proc data
-- Procs when the rogue deals melee ability damage. Works for players and NPCs alike
-- (spell_proc has no player-only gating). The triggering abilities are filtered in the script.
DELETE FROM `spell_proc` WHERE `SpellId` IN (441146);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(441146, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x10, 0x0, 0x0, 0x2, 0x403, 0x0, 0x0, 0, 0, 0, 0); -- Unseen Blade
