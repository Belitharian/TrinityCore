--! scene 500008
--! name Emerald - BFT - Help The Wounded
-- Apres la bataille, partie I : Jaina et Hedric constatent les degats, puis
-- Jaina gagne la place de la citadelle (etape Help The Wounded). Lancee a la
-- validation de CRITERIA_TREE_SURVIVE_THE_BATTLE ; l arrivee de Jaina leve
-- EVENT_FIND_JAINA_04 cote serveur.

Spawn("Jaina",  NPC(NPC_JAINA_PROUDMOORE), POS(JainaPoint03))
Spawn("Hedric", NPC(NPC_HEDRIC_EVENCANE),  LOC(actorsRelocation, DATA_HEDRIC_EVENCANE))
Sheathe("Jaina", false)
Sheathe("Hedric", true)
Ready()

LookAt("Jaina", "Hedric")
EmoteState("Jaina", 0)
LookAt("Hedric", "Jaina")
EmoteState("Hedric", 0)
Next(0.8)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_01))
Next(2)

Say("Hedric", T(NPC_HEDRIC_EVENCANE, SAY_POST_BATTLE_02))
Next(5)

-- Thalen a ete libere pendant l assaut, Vereesa le traque.
Say("Hedric", T(NPC_HEDRIC_EVENCANE, SAY_POST_BATTLE_02_BIS))
Next(11)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_02_TER))
Next(6)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_03))
Next(5.5)

-- Jaina gagne la place (npc_jaina_theramore, path 1), Hedric la suit puis
-- rejoint son poste de garde (npc_hedric_evencane, path 2 et point 2).
ClearLook()
Follow("Jaina", PATH(JainaPath01), 3.13)
Next(1.5)

Follow("Hedric", PATH(HedricPath02), nil, function()
	MoveTo("Hedric", RUN, POS(HedricPoint03), function()
		Sheathe("Hedric", false)
		EmoteState("Hedric", ANIM(EMOTE_STATE_WAGUARDSTAND01))
	end)
end)
WaitArrival("Jaina")
Wait(1)
WaitArrival("Hedric")
Finish()
