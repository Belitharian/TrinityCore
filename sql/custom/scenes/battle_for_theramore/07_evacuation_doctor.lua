--! scene 500007
--! name Emerald - BFT - Evacuation Doctor
-- Auberge : le docteur garde pres de lui ceux qui savent soigner (Tides of
-- War, chapitre 7). Lancee par le gossip du docteur.

Spawn("Doctor", NPC(NPC_DOCTOR_VANHOWZEN), SPAWN(50000544))
Ready()

Say("Doctor", T(NPC_DOCTOR_VANHOWZEN, SAY_EVAC_DOCTOR_VOLUNTEERS))
Next(4)

Say("Doctor", T(NPC_DOCTOR_VANHOWZEN, SAY_EVAC_DOCTOR_HARBOR))
Next(0)
Finish()
