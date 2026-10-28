--! scene 500011
--! name Emerald - BFT - Retrieve Rhonin
-- Dernier acte au sommet de la tour : Jaina et Rhonin comprennent que la
-- bombe va exploser. Lancee a la validation de CRITERIA_TREE_RETRIEVE ; a la
-- fin, le serveur leve EVENT_REDUCE_IMPACT (scene de l explosion).

Spawn("Jaina",  NPC(NPC_JAINA_PROUDMOORE), POS(JainaPoint05))
Spawn("Rhonin", NPC(NPC_RHONIN),           POS(RhoninPoint02))
Sheathe("Jaina", false)
Sheathe("Rhonin", false)
Aura("Jaina", "channel", SV(SPELL_PORTAL_CHANNELING_01))
Aura("Rhonin", "channel", SV(SPELL_PORTAL_CHANNELING_03))
CannotTurn("Jaina", true)
CannotTurn("Rhonin", true)
-- Tant qu ils canalisent, ils parlent sans animation de parole (elle
-- couperait la pose de canalisation) et ne tournent que la tete.
Quiet("Jaina", true)
Quiet("Rhonin", true)
Ready()

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_XPLOSION_01))
GlanceAt("Jaina", "Rhonin")
Next(5)

-- Jaina lache le portail et se tourne vers Rhonin, qui canalise toujours.
FaceTo("Jaina", "Rhonin")
RemoveAllAuras("Jaina")
Quiet("Jaina", false)
LookAt("Jaina", "Rhonin")
GlanceAt("Rhonin", "Jaina")
Next(3)

Say("Rhonin", T(NPC_RHONIN, SAY_IRIS_XPLOSION_02))
Next(4)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_XPLOSION_03))
Next(5.5)

Say("Rhonin", T(NPC_RHONIN, SAY_IRIS_XPLOSION_04))
Next(9)

Say("Rhonin", T(NPC_RHONIN, SAY_IRIS_XPLOSION_05))
Next(11)

Say("Rhonin", T(NPC_RHONIN, SAY_IRIS_XPLOSION_06))
Next(8)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_XPLOSION_07))
Next(6)

Say("Rhonin", T(NPC_RHONIN, SAY_IRIS_XPLOSION_08))
Next(3.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_IRIS_XPLOSION_09))
Next(6)

-- Rhonin lache le portail sur sa derniere replique.
CannotTurn("Rhonin", false)
RemoveAllAuras("Rhonin")
Quiet("Rhonin", false)
Say("Rhonin", T(NPC_RHONIN, SAY_IRIS_XPLOSION_10))
Next(13)
Finish()
