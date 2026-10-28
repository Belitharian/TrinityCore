--! scene 500009
--! name Emerald - BFT - Help The Wounded Kinndy
-- Apres la bataille, partie II : Jaina reconforte Kinndy pendant que les
-- joueurs soignent les blesses. Lancee a la validation de
-- CRITERIA_TREE_FOLLOW_JAINA, annulee par le serveur si l etape se termine
-- avant la fin.

Spawn("Jaina",  NPC(NPC_JAINA_PROUDMOORE),  PATHEND(JainaPath01, 3.13))
Spawn("Kinndy", NPC(NPC_KINNDY_SPARKSHINE), LOC(archmagesRelocation, DATA_KINNDY_SPARKSHINE))
Sheathe("Jaina", false)
Sheathe("Kinndy", false)
-- Aucune animation ne combine parole et pleurs (toutes les emotes de pleurs
-- jouent EmoteCry) : tant qu elle pleure, Kinndy parle sans animation de
-- parole, qui masquerait ses sanglots sur le haut du corps.
EmoteState("Kinndy", ANIM(EMOTE_STATE_CRY))
Quiet("Kinndy", true)
Ready()

LookAt("Jaina", "Kinndy")
LookAt("Kinndy", "Jaina")
Next(0.8)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_POST_BATTLE_04))
Next(4)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_05))
Next(7)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_POST_BATTLE_06))
EmoteState("Kinndy", 0)
Quiet("Kinndy", false)
Next(4)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_07))
Next(3.5)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_POST_BATTLE_08))
Next(3)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_09))
Next(13.5)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_POST_BATTLE_10))
Next(4)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_11))
Next(7.5)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_POST_BATTLE_11_BIS))
Next(5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_12))
Next(8.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_13))
Next(12)

Say("Kinndy", T(NPC_KINNDY_SPARKSHINE, SAY_POST_BATTLE_14))
Next(3)

-- Jaina s agenouille a hauteur de Kinndy pour lui dire qu elle est fiere
-- d elle, puis se releve et reprend son orientation.
Kneel("Jaina")
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_POST_BATTLE_15))
Next(6.5)

ClearLook()
Wait(StandUp("Jaina"))
Face("Jaina", 3.15)
Face("Kinndy", 2.73)
Finish()
