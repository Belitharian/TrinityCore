--! scene 500006
--! name Emerald - BFT - Evacuation Janene
-- Auberge : Janene refuse de partir (Tides of War, chapitre 7). Lancee par
-- le gossip de Janene.

Spawn("Janene", NPC(NPC_INNKEEPER_JANENE), SPAWN(50000509))
Sheathe("Janene", false)
Ready()

Say("Janene", T(NPC_INNKEEPER_JANENE, SAY_EVAC_JANENE_STAYS))
Next(4)

Say("Janene", T(NPC_INNKEEPER_JANENE, SAY_EVAC_JANENE_CURFEW))
Next(5)

Say("Janene", T(NPC_INNKEEPER_JANENE, SAY_EVAC_JANENE_HOME))
Next(0)
Finish()
