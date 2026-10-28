--! scene 500010
--! name Emerald - BFT - Wait For Amara
-- Kalecgos previent Jaina du danger que represente l iris, Rhonin et
-- Kalecgos s en vont, puis Amara Leeson revient par le portail (etape Wait
-- For Archmage Leeson). Lancee a la validation de CRITERIA_TREE_JOIN_JAINA.
-- A la fin, Jaina part vers le sommet de la tour.
-- Signaux : "RhoninGone" (Rhonin reapparait au sommet de la tour),
-- "AmaraBack" (EVENT_WAIT_ARCHMAGE_LESSON).
-- Tari, Thader et Tervosh restent de vrais PNJ : ils ne parlent pas.

Spawn("Jaina",  NPC(NPC_JAINA_PROUDMOORE),  PATHEND(JainaPath01, 3.15))
Spawn("Kalec",  NPC(NPC_KALECGOS),          PATHSTART(KalecPath02))
Spawn("Rhonin", NPC(NPC_RHONIN),            LOC(archmagesRelocation, DATA_RHONIN))
Spawn("Amara",  NPC(NPC_AMARA_LEESON),      LOC(archmagesRelocation, DATA_AMARA_LEESON))
Spawn("Kinndy", NPC(NPC_KINNDY_SPARKSHINE), LOC(archmagesRelocation, DATA_KINNDY_SPARKSHINE))
Spawn("Hedric", NPC(NPC_HEDRIC_EVENCANE),   POS(HedricPoint03))
Sheathe("Jaina", false)
Sheathe("Rhonin", false)
Sheathe("Amara", false)
Sheathe("Kinndy", false)
Sheathe("Hedric", false)
Face("Kinndy", 2.73)
EmoteState("Hedric", ANIM(EMOTE_STATE_WAGUARDSTAND01))
CannotTurn("Jaina", true)
CannotTurn("Kalec", true)
CannotTurn("Rhonin", true)
CannotTurn("Amara", true)
CannotTurn("Kinndy", true)
CannotTurn("Hedric", true)
Ready()

-- Rhonin rejoint la table de banquet, Kalecgos arrive de la place.
MoveTo("Rhonin", WALK, POS(RhoninPoint01))
Wait(1)
Follow("Kalec", PATH(KalecPath02))
Next(8)

Look("Kalec")
Say("Kalec", T(NPC_KALECGOS, SAY_IRIS_WARN_01))
Next(1.5)

Look("Jaina")
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_WARN_02))
Next(2)

Look("Kalec")
Say("Kalec", T(NPC_KALECGOS, SAY_IRIS_WARN_03))
Next(5.5)

Look("Jaina")
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_WARN_04))
Next(8.5)

Look("Kalec")
Say("Kalec", T(NPC_KALECGOS, SAY_IRIS_WARN_05))
Next(2)

Look("Jaina")
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_WARN_06))
Next(8.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_WARN_07))
Next(2)

Look("Kalec")
Say("Kalec", T(NPC_KALECGOS, SAY_IRIS_WARN_08))
Next(7)

Look("Rhonin")
Say("Rhonin", T(NPC_RHONIN, SAY_IRIS_WARN_09))
Next(6)

-- Kalecgos s en va (npc_kalecgos_theramore, path 3), Rhonin aussi : il
-- reapparait au sommet de la tour (npc_rhonin, path 1). Amara part en
-- mission (npc_amara_leeson, path 1).
ClearLook()
Follow("Kalec", PATH(KalecPath03), nil, function() Hide("Kalec") end)
Next(3)

Follow("Rhonin", PATH(RhoninPath01), nil, function()
	Hide("Rhonin")
	Cue("RhoninGone")
end)
Next(3)

Follow("Amara", PATH(AmaraPath01), nil, function() Hide("Amara") end)
Next(10)

-- Retour d Amara (npc_amara_leeson, path 2) : c est son arrivee qui valide
-- l attente cote scenario. Elle finit d abord son aller : reprendre le
-- chemin du retour en plein escalier lui faisait couper a travers les murs.
WaitArrival("Amara")
Show("Amara")
Follow("Amara", PATH(KalecPath02), nil, function() Cue("AmaraBack") end)
WaitArrival("Amara")
Next(1)

LookAt("Jaina", "Amara")
LookAt("Amara", "Jaina")
Next(1)

Say("Amara", T(NPC_AMARA_LEESON, SAY_IRIS_WARN_10))
Next(6)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_WARN_11))
Next(4)

ClearLook()
Follow("Amara", PATH(KalecPath03), nil, function() Hide("Amara") end)
Next(4)

-- Jaina part vers le sommet de la tour (npc_jaina_theramore, path 2).
Follow("Jaina", PATH(JainaPath02), nil, function() Hide("Jaina") end)
WaitArrival("Jaina")
WaitArrival("Amara")
WaitArrival("Kalec")
Finish()
