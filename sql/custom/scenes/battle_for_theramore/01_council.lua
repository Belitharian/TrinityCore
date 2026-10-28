--! scene 500001
--! name Emerald - BFT - Council
-- Conseil de guerre dans la tour (etape The Council). Lancee a la
-- validation de CRITERIA_TREE_FIND_JAINA ; a la fin, Jaina rejoint la table
-- et le serveur leve EVENT_THE_COUNCIL.

Spawn("Jaina",   NPC(NPC_JAINA_PROUDMOORE),  SPAWN(50000007))
Spawn("Kalec",   NPC(NPC_KALECGOS),          SPAWN(50000010))
Spawn("Tervosh", NPC(NPC_ARCHMAGE_TERVOSH),  SPAWN(50000008))
Spawn("Kinndy",  NPC(NPC_KINNDY_SPARKSHINE), SPAWN(50000009))
Sheathe("Jaina", false)
EmoteState("Tervosh", ANIM(EMOTE_STATE_READ_BOOK_AND_TALK))
CannotTurn("Jaina", true)
CannotTurn("Kalec", true)
CannotTurn("Tervosh", true)
CannotTurn("Kinndy", true)
Ready()

-- Tervosh referme son livre et Kinndy s approche : ils rejoignent la table
-- pendant que Jaina parle.
EmoteState("Tervosh", 0)
Follow("Tervosh", PATH(TervoshPath01), 0.70)
Follow("Kinndy", { speed = WALK, { POS(KinndyPoint01) } }, 1.09)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_REUNION_1))
Look("Jaina")
Next(2)
WaitArrival("Tervosh")
WaitArrival("Kinndy")

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_REUNION_2))
Look("Kinndy")
Next(10)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_REUNION_3))
Look("Jaina")
Next(9)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_REUNION_4))
Look("Kinndy")
Next(4)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_REUNION_5))
Look("Jaina")
Next(7)

Say("Tervosh", T(NPC_ARCHMAGE_TERVOSH, SAY_REUNION_6))
Look("Tervosh")
Next(6.5)

Say("Kalec", T(NPC_KALECGOS, SAY_REUNION_7))
Look("Kalec")
Next(6)

Say("Kalec", T(NPC_KALECGOS, SAY_REUNION_8))
Next(8)

Say("Tervosh", T(NPC_ARCHMAGE_TERVOSH, SAY_REUNION_9))
Next(2)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_REUNION_9_BIS))
Next(4)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_REUNION_10))
Look("Jaina")
Next(4.5)

Say("Kalec", T(NPC_KALECGOS, SAY_REUNION_11))
Look("Kalec")
Next(3)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_REUNION_12))
Look("Jaina")
Next(6)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_REUNION_13))
Look("Kinndy")
Next(4)

Say("Kalec", T(NPC_KALECGOS, SAY_REUNION_14))
Look("Kalec")
Next(5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_REUNION_15))
Look("Jaina")
Next(2)

Say("Kalec", T(NPC_KALECGOS, SAY_REUNION_16))
Look("Kalec")
Next(3)

Say("Kalec", T(NPC_KALECGOS, SAY_REUNION_17))
Next(5)

-- Dispersion : chacun repart vers son poste. Kalecgos sort de la tour, se
-- teleporte (npc_kalecgos_theramore, path 1) puis se dissout.
ClearLook()
Follow("Kalec", PATH(KalecPath01), nil, function()
	Wait(1)
	CastVisual("Kalec", SV(400542))
	Wait(4.8)
	Aura("Kalec", "dissolve", SV(255295))
	Hide("Kalec", 1)
end)
Next(2)

-- Tervosh monte a l etage et quitte la scene (npc_archmage_tervosh, path 2).
Follow("Tervosh", PATH(TervoshPath02), 2.14, function() Hide("Tervosh") end)
Next(5)

-- Kinndy sort de la salle (npc_kinndy_sparkshine, path 1).
Follow("Kinndy", PATH(KinndyPath01), 4.62, function() Hide("Kinndy") end)
Next(6)

-- Jaina rejoint la table du conseil : c est son arrivee qui valide l etape.
Follow("Jaina", { speed = WALK, { POS(JainaPoint01) } })
-- Kalecgos est deja hors de vue : on n attend pas la fin de sa sortie.
WaitArrival("Jaina")
WaitArrival("Tervosh")
WaitArrival("Kinndy")
Finish()
