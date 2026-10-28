--! scene 500005
--! name Emerald - BFT - Evacuation Docks
-- Quai de L Etoile du matin (Tides of War, chapitre 7) : le second fait
-- embarquer les enfants, cap sur Gadgetzan. Lancee par le gossip du second.

Spawn("Mate",   NPC(NPC_MORNING_STAR_FIRST_MATE), SPAWN(50000675))
Spawn("Shaman", NPC(NPC_DRAENEI_SHAMAN),          SPAWN(50000676))
Ready()

Say("Mate", T(NPC_MORNING_STAR_FIRST_MATE, SAY_EVAC_MATE_BOARDING))
Next(4)

Say("Mate", T(NPC_MORNING_STAR_FIRST_MATE, SAY_EVAC_MATE_RATCHET))
Next(5)

Say("Shaman", T(NPC_DRAENEI_SHAMAN, SAY_EVAC_SHAMAN_ELEMENTS))
Emote("Shaman", ANIM(EMOTE_ONESHOT_SPELL_CAST_OMNI))
Next(5)

Say("Mate", T(NPC_MORNING_STAR_FIRST_MATE, SAY_EVAC_MATE_FAREWELL))
Next(0)
Finish()
