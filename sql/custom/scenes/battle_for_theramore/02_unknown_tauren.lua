--! scene 500002
--! name Emerald - BFT - Unknown Tauren
-- Perith Stormhoove annonce l attaque de la Horde (etape The Unknown
-- Tauren). Lancee a la validation de CRITERIA_TREE_WAITING ; Jaina redige
-- le sauf-conduit de Perith a la plume magique. A la fin, Peine quitte la
-- salle et le serveur leve EVENT_THE_UNKNOWN_TAUREN.

Spawn("Jaina",  NPC(NPC_JAINA_PROUDMOORE), POS(JainaPoint01))
Sheathe("Jaina", false)
CannotTurn("Jaina", true)

-- Escorte de Perith : elle entre par la porte de la tour (perithLocation).
Spawn("Knight", NPC(NPC_KNIGHT_OF_THERAMORE), LOCFROM(perithLocation, NPC_KNIGHT_OF_THERAMORE))
Spawn("Pained", NPC(NPC_PAINED),              LOCFROM(perithLocation, NPC_PAINED))
Spawn("Perith", NPC(NPC_PERITH_STORMHOOVE),   LOCFROM(perithLocation, NPC_PERITH_STORMHOOVE))
Sheathe("Knight", false)
Sheathe("Pained", false)
EmoteState("Knight", ANIM(EMOTE_STATE_WAGUARDSTAND01))
CannotTurn("Knight", true)
CannotTurn("Pained", true)
CannotTurn("Perith", true)
Ready()

MoveTo("Knight", WALK, LOC(perithLocation, NPC_KNIGHT_OF_THERAMORE))
MoveTo("Pained", WALK, LOC(perithLocation, NPC_PAINED))
MoveTo("Perith", WALK, LOC(perithLocation, NPC_PERITH_STORMHOOVE))
Next(7)

LookAt("Jaina", "Perith")
Next(2)

Say("Pained", T(NPC_PAINED, SAY_WARN_1))
Look("Pained")
Next(1)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_2))
Look("Jaina")
Next(1)

Say("Pained", T(NPC_PAINED, SAY_WARN_3))
Look("Pained")
Next(6)

Say("Pained", T(NPC_PAINED, SAY_WARN_4))
Next(5.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_5))
Look("Jaina")
Next(4.5)

Say("Pained", T(NPC_PAINED, SAY_WARN_6))
Look("Pained")
Next(10)

-- Peine remet Brise-peur a Jaina.
ClearLook()
MoveCloser("Pained", "Jaina", 1.8)
Next(2)

-- Peine lui tend le paquet (Offer) et garde le bras tendu...
Look("Jaina")
EmoteThenState("Pained", ANIM(EMOTE_ONESHOT_OFFER_START), ANIM(EMOTE_STATE_OFFER_LOOP))
Next(1)

-- ... Jaina tend la main et le prend, Peine ramene son bras.
Emote("Jaina", ANIM(EMOTE_ONESHOT_OFFER_START))
Wait(0.6)
EndState("Pained", ANIM(EMOTE_ONESHOT_OFFER_END))
Say("Pained", T(NPC_PAINED, SAY_WARN_7))
Next(3)

-- Jaina observe l objet en main.
EmoteThenState("Jaina", ANIM(EMOTE_ONESHOT_OFFER_END), ANIM(EMOTE_STATE_HOLD))
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_8))
Next(4)

EmoteState("Jaina", 0)
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_9))
Look("Jaina")
Next(2)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_10))
ClearLook()
MoveTo("Pained", WALK, POS(PainedPoint01))
Next(2)

MoveCloser("Knight", "Jaina", 3)
Next(2)

Say("Knight", T(NPC_KNIGHT_OF_THERAMORE, SAY_WARN_11))
Look("Knight")
Next(5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_12))
Look("Jaina")
Next(5)

-- Le chevalier se retire, Perith s avance.
ClearLook()
Say("Pained", T(NPC_PAINED, SAY_WARN_13))
Follow("Knight", PATH(OfficerPath01), nil, function() Hide("Knight", 1) end)
MoveCloser("Perith", "Jaina", 3)
Next(3)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_14))
LookAt("Perith", "Jaina")
Look("Perith")
Next(9)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_15))
Next(4)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_16))
Next(12)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_17))
Next(10.5)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_18))
Next(11.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_19))
Next(4.2)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_20))
Next(5.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_21))
Next(1.5)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_22))
Next(14.5)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_23))
Next(10)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_24))
Next(11.5)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_25))
Next(15.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_26))
Next(5)

-- Jaina se tourne vers son bureau.
ClearLookOf("Jaina")
Face("Jaina", 3.33)
Next(2)

-- Elle redige le sauf-conduit : la plume magique est un visuel d aura,
-- retire a l etape suivante.
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_27))
Aura("Jaina", "quill", SV(SPELL_MAGIC_QUILL))
Next(10)

RemoveAura("Jaina", "quill")
CannotTurn("Jaina", false)
FaceTo("Jaina", "Perith")
Next(2)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_28))
Next(5)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_29))
Next(5)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_30))
Next(8)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_31))
Next(4)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_32))
Next(3.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_33))
Next(4)

Say("Perith", T(NPC_PERITH_STORMHOOVE, SAY_WARN_34))
Next(3)

-- Perith s en va par le meme chemin que le chevalier.
ClearLook()
CannotTurn("Pained", false)
Follow("Perith", PATH(OfficerPath01), nil, function() Hide("Perith", 1) end)
Next(5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_35))
FaceTo("Jaina", "Pained")
FaceTo("Pained", "Jaina")
Next(7)

Say("Pained", T(NPC_PAINED, SAY_WARN_36))
Next(2.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_WARN_37))
Next(2)

-- Peine quitte la salle (npc_pained, path 1) : c est sa sortie qui valide
-- l etape.
Face("Jaina", 0.39)
Follow("Pained", PATH(KinndyPath01), nil, function() Hide("Pained") end)
WaitArrival("Pained")
WaitArrival("Knight")
WaitArrival("Perith")
Finish()
