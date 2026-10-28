--! scene 500004
--! name Emerald - BFT - The Battle
-- Trahison et capture de Thalen Songweaver (Tides of War, chapitre 17).
-- Lancee apres CRITERIA_TREE_RETRIEVE_JAINA, aux portes principales. Les
-- deux fantassins qui ligotent Thalen restent de vrais PNJ : le serveur les
-- choisit (les plus proches de Jaina) et les fait agir sur les signaux.
-- Signaux : "Bind" (les fantassins accourent), "Kneel" (ils ligotent
-- Thalen), "Escort" (le vrai Thalen, ligote, part vers la citadelle avec
-- eux), "BarrierBreaks", "ThaderWounded".

Spawn("Jaina",  NPC(NPC_JAINA_PROUDMOORE),  LOC(actorsRelocation, DATA_JAINA_PROUDMOORE))
Spawn("Hedric", NPC(NPC_HEDRIC_EVENCANE),   LOC(actorsRelocation, DATA_HEDRIC_EVENCANE))
Spawn("Thalen", NPC(NPC_THALEN_SONGWEAVER), LOC(actorsRelocation, DATA_THALEN_SONGWEAVER))
Spawn("Amara",  NPC(NPC_AMARA_LEESON),      LOC(actorsRelocation, DATA_AMARA_LEESON))
Spawn("Pained", NPC(NPC_PAINED),            POS(PainedPoint02))
Sheathe("Jaina", false)
Sheathe("Hedric", false)
Sheathe("Thalen", false)
Sheathe("Amara", false)
Sheathe("Pained", false)
Aura("Thalen", "channel", SV(SPELL_PORTAL_CHANNELING_02))
Aura("Amara", "channel", SV(SPELL_PORTAL_CHANNELING_03))
-- Peine garde Thalen a l oeil : elle parle sans geste.
Quiet("Pained", true)
Ready()

-- Jaina sent la barriere faiblir alors que Thalen l entretient.
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_THALEN_NOTICE_JAINA))
FaceTo("Jaina", "Thalen")
EmoteState("Jaina", 0)
Next(6.5)

-- Le trait arcanique n est qu un visuel.
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_THALEN_TRAITOR_JAINA))
Emote("Jaina", ANIM(EMOTE_ONESHOT_SPELL_CAST_OMNI))
RemoveAllAuras("Thalen")
Next(2)

-- Il ricane et leve les mains pour riposter, sans geste de parole : les
-- mains restent levees.
FaceTo("Thalen", "Jaina")
EmoteState("Thalen", ANIM(EMOTE_STATE_ARTDUALLOOP))
Quiet("Thalen", true)
Say("Thalen", T(NPC_THALEN_SONGWEAVER, SAY_THALEN_SNEER))
Next(4.5)

FaceTo("Pained", "Thalen")
Next(0.35)

-- Peine le charge et l assomme ; les deux fantassins accourent. Thalen perd
-- ses mains levees : sans emote, l animation d etourdissement se voit.
CastAt("Pained", SPELL(SPELL_CHARGE), "Thalen")
MoveCloser("Pained", "Thalen", 1.5, 25)
EmoteState("Pained", ANIM(EMOTE_STATE_READY1H))
EmoteState("Thalen", 0)
Aura("Thalen", "stunned", SV(SPELL_STUNNED))
Cue("Bind")
Next(1)

Aura("Thalen", "tied", SV(SPELL_TIED_UP))
MoveBackward("Pained", BACK_HURRY, POS(PainedPoint03), "Thalen")
Cue("Kneel")
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_THALEN_SPARED_JAINA))
Next(4)

Say("Pained", T(NPC_PAINED, SAY_THALEN_USEFUL_PAINED))
Next(5)

Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_THALEN_ONSLAUGHT_JAINA))
Next(3)

Say("Pained", T(NPC_PAINED, SAY_THALEN_TEA_PAINED))
Next(4)

-- Hedric est dans son dos : elle le regarde par-dessus l epaule.
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_THALEN_EVENCANE_JAINA))
CannotTurn("Jaina", true)
GlanceAt("Jaina", "Hedric")
AnimKit("Jaina", 1530)
Next(6)

-- Hedric confie Thalen a ses deux soldats : le vrai Thalen reprend sa place.
Say("Hedric", T(NPC_HEDRIC_EVENCANE, SAY_THALEN_ESCORT_HEDRIC))
GlanceAt("Amara", "Hedric")
Cue("Escort")
Hide("Thalen")
Next(4)

-- Jaina est dans son dos : elle la regarde par-dessus l epaule.
-- Jaina se tourne vers Amara, qui lui repond.
Say("Amara", T(NPC_AMARA_LEESON, SAY_THALEN_VOUCHED_AMARA))
GlanceAt("Amara", "Jaina")
RemoveAura("Amara", "channel")
ClearLookOf("Jaina")
CannotTurn("Jaina", false)
GlanceAt("Jaina", "Amara")
Next(6)

-- A la fin de sa question, Jaina revient face a la porte.
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_THALEN_ALONE_JAINA))
Next(2.5)
ResetFacing("Jaina")

Say("Amara", T(NPC_AMARA_LEESON, SAY_THALEN_ALONE_AMARA))
Next(2)

-- La barriere cede, la Horde entre.
Say("Jaina", T(NPC_JAINA_PROUDMOORE, SAY_BATTLE_GATE_BREAKS))
-- Elle cede a la fin de l avertissement de Jaina.
Next(0)
Cue("BarrierBreaks")
Next(3.5)

-- Amara releve la barriere : le lancer (l essentiel du visuel), puis
-- l aura. Le vrai PNJ la recoit a la fin de la scene, sans relancer le sort.
CastVisual("Amara", SV(SPELL_PRISMATIC_BARRIER))
Aura("Amara", "barrier", SV(SPELL_PRISMATIC_BARRIER))
Next(3)

-- A la porte Ouest, Thader et Kinndy sont pris d assaut.
Cue("ThaderWounded")
Next(3)
Finish()
