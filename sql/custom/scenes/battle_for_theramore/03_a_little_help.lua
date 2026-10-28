--! scene 500003
--! name Emerald - BFT - A Little Help
-- Renforts de Dalaran et repartition des generaux de la 7e Legion (etape A
-- Little Help). Lancee a la validation de CRITERIA_TREE_A_LITTLE_HELP ; elle
-- se termine sur le teleport de masse de Jaina, le serveur met alors en
-- place le champ de bataille.
-- Signaux : "Portal" (portail de Dalaran), "ClosePortal".

-- Archimage sortant du portail de Dalaran (archmagesLocation).
function Archmage(name, entry, x, y, z, o)
	Spawn(name, entry, POS(PortalPoint01))
	Sheathe(name, false)
	CannotTurn(name, true)
	ImpactVisual(name, SV(SPELL_TELEPORT_DUMMY))
	MoveTo(name, WALK, x, y, z, o)
	Wait(0.9)
end

-- General de la 7e Legion (generalsLocation) : au bout de son chemin il se
-- tourne vers Jaina et ne tourne plus (npc_theramore_general, path 1).
-- ranged : fusil en main (Thaddus, Horran), il est mal place range.
function General(name, entry, delay, path, x, y, z, o, ranged)
	Async(function()
		Wait(delay)
		Spawn(name, entry, x, y, z, o)
		if ranged then
			SheatheRanged(name)
		else
			Sheathe(name, false)
		end
		Follow(name, path, nil, function()
			FaceTo(name, "Jaina")
			CannotTurn(name, true)
		end)
	end)
end

Spawn("Jaina",   NPC(NPC_JAINA_PROUDMOORE),  POS(JainaPoint02))
Spawn("Tervosh", NPC(NPC_ARCHMAGE_TERVOSH),  POS(TervoshPoint01))
Spawn("Kinndy",  NPC(NPC_KINNDY_SPARKSHINE), POS(KinndyPoint02))
Spawn("Kalec",   NPC(NPC_KALECGOS),          POS(KalecgosPoint01))
Spawn("Hedric",  NPC(NPC_HEDRIC_EVENCANE),   POS(HedricPoint01))
Sheathe("Jaina", false)
Aura("Tervosh", "fire", SV(SPELL_COSMETIC_FIRE_LIGHT))
CannotTurn("Tervosh", true)
CannotTurn("Kinndy", true)
CannotTurn("Kalec", true)
CannotTurn("Hedric", true)
Ready()

-- Cris de la foule (SOUND_FEARFUL_CROWD) : joues par la scene, Hedric etant
-- encore masque cote serveur personne ne les entendrait.
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_2))
PlaySound(CONST(SOUND_FEARFUL_CROWD))
Next(4.5)

-- Hedric accourt jusqu au portail. Le 4e noeud du chemin le faisait passer
-- a droite du portail puis revenir dessus en crochet : la scene va droit.
Say("Hedric", T(NPC_HEDRIC_EVENCANE, SAY_PRE_BATTLE_1))
Follow("Hedric", PathWithout(PATH(HedricPath01), 4))
Next(2)

Say("Hedric", T(NPC_HEDRIC_EVENCANE, SAY_PRE_BATTLE_3))
Next(5.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_4))
Next(5)

Cue("Portal")
Next(0.5)

-- Hedric recule devant le portail (npc_hedric_evencane, point 1).
-- Il se tourne vite vers le portail, puis recule sans le quitter des yeux.
MoveBackward("Hedric", BACK_STEADY, POS(HedricPoint02), { POS(PortalPoint01) })
Next(0.5)

-- Les archimages sortent du portail un a un, puis la 7e Legion entre dans
-- la citadelle.
Archmage("Rhonin",  NPC(NPC_RHONIN),             LOC(archmagesLocation, NPC_RHONIN))
Archmage("Vereesa", NPC(NPC_VEREESA_WINDRUNNER), LOC(archmagesLocation, NPC_VEREESA_WINDRUNNER))
Archmage("Thalen",  NPC(NPC_THALEN_SONGWEAVER),  LOC(archmagesLocation, NPC_THALEN_SONGWEAVER))
Archmage("Tari",    NPC(NPC_TARI_COGG),          LOC(archmagesLocation, NPC_TARI_COGG))
Archmage("Amara",   NPC(NPC_AMARA_LEESON),       LOC(archmagesLocation, NPC_AMARA_LEESON))
Archmage("Thader",  NPC(NPC_THADER_WINDERMERE),  LOC(archmagesLocation, NPC_THADER_WINDERMERE))
General("Marcus",   NPC(NPC_MARCUS_JONATHAN),      0,    PATH(MarcusPath01),    LOCFROM(generalsLocation, NPC_MARCUS_JONATHAN))
General("Thaddus",  NPC(NPC_THADDUS_STOUTBLOW),    0.38, PATH(ThaddusPath01),   LOCFROM(generalsLocation, NPC_THADDUS_STOUTBLOW), true)
General("Horran",   NPC(NPC_HORRAN_REDMANE),       0,    PATH(HorranPath01),    LOCFROM(generalsLocation, NPC_HORRAN_REDMANE), true)
General("Aubrey",   NPC(NPC_ADMIRAL_AUBREY),       0.65, PATH(AubreyPath01),    LOCFROM(generalsLocation, NPC_ADMIRAL_AUBREY))
General("Shandris", NPC(NPC_SHANDRIS_FEATHERMOON), 0.8,  PATH(ShandrisPath01),  LOCFROM(generalsLocation, NPC_SHANDRIS_FEATHERMOON))
General("Tiras",    NPC(NPC_GENERAL_TIRAS_ALAN),   1,    PATH(TirasAlanPath01), LOCFROM(generalsLocation, NPC_GENERAL_TIRAS_ALAN))
Next(2)

Say("Rhonin", T(NPC_RHONIN, SAY_PRE_BATTLE_5))
Look("Rhonin")
Cue("ClosePortal")
Next(4.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_6))
Look("Jaina")
Next(8)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_7))
Next(9)

Say("Thalen", T(NPC_THALEN_SONGWEAVER, SAY_PRE_BATTLE_8))
Look("Thalen")
Next(6)

-- Le plus haut grade de la 7e Legion se presente, Jaina remercie les
-- generaux avant de repartir les archimages.
-- Marcus s adresse a Jaina : tout le monde regarde Jaina, elle le regarde.
Say("Marcus", T(NPC_MARCUS_JONATHAN, SAY_GENERALS_MARCUS_INTRO))
Look("Jaina")
LookAt("Jaina", "Marcus")
Next(8.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_GENERALS_JAINA_THANKS))
Look("Jaina")
Next(7.5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_9))
Look("Jaina")
Next(7)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_10))
Next(5.5)

Say("Rhonin", T(NPC_RHONIN, SAY_PRE_BATTLE_11))
Look("Rhonin")
Next(2)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_12))
Look("Tervosh")
Next(4)

-- Repartition des generaux : Marcus et les deux nains aux portes
-- principales, Aubrey aux docks, Shandris a la porte Ouest, Tiras alan a la
-- citadelle.
Say("Marcus", T(NPC_MARCUS_JONATHAN, SAY_GENERALS_MARCUS_01))
Look("Marcus")
Next(6)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_GENERALS_JAINA_GATES))
Look("Jaina")
Next(6)

Say("Thaddus", T(NPC_THADDUS_STOUTBLOW, SAY_GENERALS_STOUTBLOW_01))
Look("Thaddus")
Next(8)

Say("Horran", T(NPC_HORRAN_REDMANE, SAY_GENERALS_REDMANE_01))
Look("Horran")
Next(5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_GENERALS_JAINA_DOCKS))
Look("Jaina")
Next(6.5)

Say("Aubrey", T(NPC_ADMIRAL_AUBREY, SAY_GENERALS_AUBREY_01))
Look("Aubrey")
Next(5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_GENERALS_JAINA_WEST))
Look("Jaina")
Next(7)

Say("Shandris", T(NPC_SHANDRIS_FEATHERMOON, SAY_GENERALS_SHANDRIS_01))
Look("Shandris")
Next(6)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_GENERALS_JAINA_CITADEL))
Look("Jaina")
Next(6)

Say("Tiras", T(NPC_GENERAL_TIRAS_ALAN, SAY_GENERALS_TIRAS_ALAN_01))
Look("Tiras")
Next(4.5)

-- Vereesa s adresse a Jaina : tout le monde regarde Jaina, elle la regarde.
Say("Vereesa", T(NPC_VEREESA_WINDRUNNER, SAY_PRE_BATTLE_13))
Look("Jaina")
LookAt("Jaina", "Vereesa")
Next(8)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_14))
Look("Jaina")
ClearLookOf("Jaina")
CannotTurn("Jaina", false)
Next(10)

-- Jaina s adresse au joueur qui regarde la scene.
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_PRE_BATTLE_15))
CannotTurn("Jaina", true)
Look(PLAYER)
Next(5)

-- Teleport de masse : le serveur prend le relais au moment ou il aboutit.
ClearLook()
CastAt("Jaina", SPELL(SPELL_MASS_TELEPORT), "Jaina")
Next(4.6)
Finish()
