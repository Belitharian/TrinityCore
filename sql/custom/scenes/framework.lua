-- Emerald : boite a outils des scenes de dialogue.
--
-- Chargee par chaque package de dialogue apres le Cinematic Framework
-- (package 156 : SpawnActor, Wait...) et avant le script de la scene.
-- Elle reprend, cote client, ce que faisaient les chaines d events de
-- l InstanceScript : Talk, SetTarget (avec ou sans UNIT_FLAG2_CANNOT_TURN),
-- SetFacingTo, MovePath / MovePoint, emotes, sorts et auras.
--
-- Conventions :
--   - coordonnees du monde, orientations en radians comme cote serveur
--     (le client attend des degres, la conversion est faite ici) ;
--   - chaque acteur porte un nom (Cast.Jaina...), c est ce nom que
--     prennent toutes les fonctions ;
--   - aucune variable locale au niveau global : le script est decoupe en
--     morceaux chaines (SceneScript.NextSceneScriptID).
--
-- Signaux envoyes au serveur (scene:TriggerServerEvent) :
--   "Ready" : les acteurs sont apparus, le serveur masque les vrais PNJ ;
--   "Step<n>" : n Next() sont passes (reprise apres reconnexion) ;
--   "Done"  : la scene est finie, le serveur remet les vrais PNJ en place.
-- Les autres signaux sont propres a chaque scene (Cue).

ZERO   = Vector:New(0, 0, 0)
WALK   = 2.5                -- marche, speed_walk 1
RUN    = 8                  -- course, speed_run 1.14286 x 7
PAUSE  = 0.7                -- respiration apres une replique (DialogueClock)
LINGER = 1                  -- recouvrement acteur / vrai PNJ en fin de scene
PLAYER = "@player"          -- cible : le joueur qui regarde la scene

Cast   = {}
Speech = { done = true }

-- Reprise apres reconnexion ---------------------------------------------------

-- Le serveur retient le dernier "Step<n>" recu. Un joueur qui revient pendant
-- la scene la rejoue avec n dans le world state RESUME_WORLD_STATE
-- (CustomScenario::DialogueSceneDirector::ResumeWorldState) : jusqu au n-ieme
-- Next(), la scene avance en accelere. Pas d attente, ni de replique, ni
-- d animation ponctuelle ou de visuel de lancer ; les deplacements arrivent
-- d un coup ; les etats (emote en boucle, auras, armes, regards, acteurs
-- masques) sont appliques. La scene reprend ensuite normalement, a la
-- replique ou le joueur l avait quittee.
RESUME_WORLD_STATE = 900001
Step = 0
Resume = scene:GetWorldState(RESUME_WORLD_STATE) or 0
FastForward = Resume > 0

-- Wait du Cinematic Framework, sans attente en accelere.
EngineWait = EngineWait or Wait
function Wait(time)
	if FastForward then
		return
	end
	EngineWait(time)
end

-- scene:AddCoroutine, execute tout de suite en accelere : une coroutine
-- lancee pendant l acceleration ne doit pas jouer en temps reel apres.
function Async(fn)
	if FastForward then
		fn()
	else
		scene:AddCoroutine(fn)
	end
end

-- PlaySoundKitStereo, muet en accelere.
function PlaySound(kit)
	if not FastForward then
		scene:PlaySoundKitStereo(kit, true)
	end
end

-- Place un acteur sans deplacement (acceleration).
function Teleport(actor, x, y, z, yaw)
	actor:SetTransform(Transform:New(Vector:New(x, y, z), yaw or actor:GetTransform().yaw))
end

-- Acteurs -------------------------------------------------------------------

-- Apparition d un acteur a la place du vrai PNJ. ACTORS est rempli par le
-- generateur depuis la base : le client ne connait pas l apparence des PNJ
-- du serveur (Creature.db2 ignore creature_template_model, les PNJ maison
-- n y sont meme pas), sans elle l acteur serait un cube. Les armes viennent
-- de creature_equip_template.
function Spawn(name, creatureID, x, y, z, o)
	local info = ACTORS and ACTORS[creatureID] or {}
	local createData = ActorCreateData:New(creatureID, Transform:New(Vector:New(x, y, z), math.deg(o)), info.scale or 1)
	if info.display then
		createData.creatureDisplayID = info.display
	end
	if info.name then
		createData.name = info.name
	end
	createData.groundSnap = true
	createData.interactible = false
	local actor = scene:SpawnActor(createData)
	actor:SetInteractible(false)
	actor.cannotTurn = false
	actor.homeFacing = o
	actor.auras = {}
	actor.move = 0
	actor.moving = false
	if info.equip then
		actor:EquipWeapon(WeaponSlot.MainHand, info.equip[1])
		actor:EquipWeapon(WeaponSlot.OffHand, info.equip[2])
		actor:EquipWeapon(WeaponSlot.Ranged, info.equip[3])
	end
	Cast[name] = actor
	return actor
end

-- Disparition (SetVisible(false) ou despawn cote serveur).
-- Fade n efface pas les visuels de sort : sans RemoveAllAuras, menottes ou
-- canalisations resteraient affichees dans le vide. Ils partent une fois le
-- fondu termine (la dissolution de Kalecgos va ainsi jusqu au bout).
function Hide(name, time)
	local actor = Cast[name]
	if actor then
		if FastForward then
			time = 0
		end
		actor:Fade(0, time or 0)
		actor.hidden = true
		actor.fade = (actor.fade or 0) + 1
		local token = actor.fade
		Async(function()
			Wait(time or 0)
			if actor.fade == token and actor.hidden then
				RemoveAllAuras(name)
			end
		end)
	end
end

function Show(name, time)
	local actor = Cast[name]
	if actor then
		actor:Fade(1, FastForward and 0 or time or 0)
		actor.hidden = false
		actor.fade = (actor.fade or 0) + 1
	end
end

-- SetSheath : arme en main (SHEATH_STATE_MELEE) ou rangee.
function Sheathe(name, armed)
	Cast[name]:SetSheatheState(armed and SheatheState.ShowWeapon or SheatheState.Sheathed, true)
end

-- Arme a distance en main (fusil des nains) : rangee, le client la pose mal
-- sur les epaules pendant les animations de parole.
function SheatheRanged(name)
	Cast[name]:SetSheatheState(SheatheState.ShowRanged, true)
end

-- Regards -------------------------------------------------------------------

-- UNIT_FLAG2_CANNOT_TURN : un acteur qui ne peut pas tourner suit sa cible de
-- la tete seulement, les autres se tournent en entier.
function CannotTurn(name, value)
	Cast[name].cannotTurn = value
end

function TargetOf(target)
	if target == PLAYER then
		return scene:GetActivePlayerDisplay()
	end
	return Cast[target]
end

-- Position d une cible : un acteur (nom, PLAYER) ou un point { x, y, z }
-- (la macro POS en donne un, l orientation en 4e valeur est ignoree).
function PositionOf(target)
	if type(target) == "table" then
		return Vector:New(target[1], target[2], target[3])
	end
	local other = TargetOf(target)
	return other and other:GetPosition()
end

-- Creature::SetTarget sur un seul acteur. Un acteur en marche ne suit pas
-- sa cible, meme de la tete (le corps et la tete se disputaient l
-- orientation, l acteur tremblait au depart) : la cible est retenue et il
-- se tourne vers elle a l arrivee (TurnToLook), comme un PNJ cote serveur.
function LookAt(name, target)
	local actor = Cast[name]
	if actor then
		actor.lookToken = (actor.lookToken or 0) + 1
	end
	ApplyLook(name, target)
end

-- Regard applique tout de suite (LookAt, ou Look une fois le delai passe).
function ApplyLook(name, target)
	local actor = Cast[name]
	local other = TargetOf(target)
	if not actor or not other or actor.hidden then
		return
	end
	actor.lookTarget = target
	if actor.moving then
		return
	end
	if actor.cannotTurn then
		actor:SetHeadFacingToTarget(other, ZERO)
	else
		actor:ClearHeadFacing()
		actor:SetFacingToTarget(other, ZERO)
	end
end

-- Fin de deplacement : l acteur reprend sa cible (tete seule s il ne peut
-- pas tourner).
function TurnToLook(actor)
	local other = actor.lookTarget and TargetOf(actor.lookTarget)
	if not other or actor.hidden then
		return
	end
	if actor.cannotTurn then
		actor:SetHeadFacingToTarget(other, ZERO)
	else
		actor:ClearHeadFacing()
		actor:SetFacingToTarget(other, ZERO)
	end
end

-- Debut de deplacement : plus aucune orientation imposee, ni cible du corps
-- ou de la tete, ni SetFacing (il reste actif jusqu au ClearFacing suivant
-- et lutterait contre l orientation de la marche). Une orientation demandee
-- pendant la marche est remise a l arrivee (WhenStill).
function StartWalking(actor)
	actor:ClearFacing()
	actor:ClearHeadFacing()
	actor.afterMove = nil
end

-- Face, FaceTo ou GlanceAt sur un acteur en marche : remis a l arrivee.
function WhenStill(actor, action)
	if actor.moving then
		actor.afterMove = action
		return true
	end
	return false
end

-- Arrivee : orientation finale, cible retenue, puis orientation demandee en
-- route.
function StopWalking(actor)
	actor.moving = false
	WalkPose(actor, false)
	TurnToLook(actor)
	local after = actor.afterMove
	actor.afterMove = nil
	if after then
		after()
	end
end

-- Regard par-dessus l epaule. La tete tourne de GLANCE_MAX degres au plus ;
-- une cible plus loin dans le dos (Hedric derriere Jaina) fait pivoter le
-- corps de ce qui manque seulement, comme quelqu un qui se retourne a
-- moitie. L orientation de l acteur vient de GetTransform().yaw (degres).
GLANCE_MAX = 80

function GlanceAt(name, target)
	local actor = Cast[name]
	local other = TargetOf(target)
	if not actor or not other or actor.hidden then
		return
	end
	if WhenStill(actor, function() GlanceAt(name, target) end) then
		return
	end
	actor.lookToken = (actor.lookToken or 0) + 1
	local from = actor:GetPosition()
	local at = other:GetPosition()
	local facing = actor:GetTransform().yaw
	local wanted = math.deg(math.atan2(at.y - from.y, at.x - from.x))
	local delta = (wanted - facing + 540) % 360 - 180
	if math.abs(delta) > GLANCE_MAX then
		local turn = delta > 0 and delta - GLANCE_MAX or delta + GLANCE_MAX
		actor:ClearFacing()
		actor:SetFacing(facing + turn)
	end
	actor:SetHeadFacingToTarget(other, ZERO)
end

-- Acteur qui parle sans animation de parole (Quiet(nom, true)).
function Quiet(name, value)
	Cast[name].quiet = value
end

-- SetTarget(ObjectGuid::Empty) sur un seul acteur.
function ClearLookOf(name)
	local actor = Cast[name]
	if actor then
		actor.lookToken = (actor.lookToken or 0) + 1
		actor.lookTarget = nil
		actor:ClearHeadFacing()
		actor:ClearFacing()
	end
end

-- Delai de reaction des auditeurs quand quelqu un prend la parole : tires
-- au sort pour chacun, sinon toutes les tetes tournent en meme temps.
LOOK_DELAY_MIN = 0.15
LOOK_DELAY_MAX = 0.9

-- InstanceScript::SetTarget : tous les autres regardent celui qui parle,
-- chacun a son rythme. Un Look ou un ClearLook suivant annule les regards
-- encore en attente.
function Look(target)
	for name, actor in pairs(Cast) do
		if name ~= target then
			actor.lookToken = (actor.lookToken or 0) + 1
			local token = actor.lookToken
			local delay = LOOK_DELAY_MIN + math.random() * (LOOK_DELAY_MAX - LOOK_DELAY_MIN)
			Async(function()
				Wait(delay)
				if actor.lookToken == token then
					ApplyLook(name, target)
				end
			end)
		end
	end
end

-- InstanceScript::ClearTarget.
function ClearLook()
	for name in pairs(Cast) do
		ClearLookOf(name)
	end
end

-- SetFacingTo(orientation).
function Face(name, o)
	local actor = Cast[name]
	if WhenStill(actor, function() Face(name, o) end) then
		return
	end
	actor:ClearFacing()
	actor:SetFacing(math.deg(o))
end

-- Retour a l orientation d apparition (Spawn).
function ResetFacing(name)
	local actor = Cast[name]
	ClearLookOf(name)
	Face(name, actor.homeFacing)
end

-- SetFacingToObject : un quart de tour unique, sans suivre la cible.
-- target : un acteur ou un point (PositionOf).
function FaceTo(name, target)
	local actor = Cast[name]
	local at = PositionOf(target)
	if actor and at then
		if WhenStill(actor, function() FaceTo(name, target) end) then
			return
		end
		actor:ClearFacing()
		actor:SetFacingToAbs(at)
	end
end

-- Repliques -----------------------------------------------------------------

-- Animations de parole : jouees sur le haut du corps tant que la replique
-- dure, comme dans les cinematiques officielles (l acteur peut marcher ou
-- garder son emote state pendant qu il parle). Elles s enchainent une par
-- une au lieu de tourner en boucle : la derniere va jusqu au bout, et une
-- nouvelle ne demarre que s il reste assez de replique pour l essentiel de
-- sa duree (TalkSubdued est longue, elle deborderait sinon).
TALK_ANIM  = 1492       -- TalkSubdued, pour toutes les repliques parlees
SHORT_TALK_ANIM = 60    -- EmoteTalk, pour une replique courte
SHORT_TALK = 1.6        -- en dessous (secondes), TalkSubdued est trop longue ; marge pour les
                        -- durees relevees sur les mp3 (1.52 s pour un mot de 1.5 s)
YELL_ANIM  = 64         -- EmoteTalkExclamation, pour les cris
TALK_CYCLE = 2          -- duree d une animation inconnue du client
TALK_FIT   = 0.9        -- part de l animation qui doit tenir dans le reste de la replique
FILL_FIT   = 0.5        -- idem pour EmoteTalk, qui comble la fin quand TalkSubdued est trop longue
TALK_ANIMS = { [60] = true, [64] = true, [65] = true }   -- emotes de parole de creature_text
LOOPING_SPLIT_BODY = 2130                                -- AnimKits.LoopingSplitBody

-- Duree d une animation pour cet acteur (GetAnimDuration, en secondes ou
-- en millisecondes selon le client).
function AnimLength(actor, anim)
	local length = actor:GetAnimDuration(anim, 0)
	if not length or length <= 0 then
		return TALK_CYCLE
	end
	if length > 30 then
		length = length / 1000
	end
	return length
end

-- Talk : bulle au-dessus de l acteur, avec la voix de la replique (la macro
-- T du generateur donne text, kind, anim et duration). Un geste (salut,
-- rire...) est joue en entier, puis l acteur parle en TalkSubdued jusqu a
-- la fin de la replique (cri : exclamation ; replique de moins de
-- SHORT_TALK : EmoteTalk, ou l emote de parole de creature_text). Les emotes narratives
-- (BroadcastType.Emote) n animent pas l acteur.
-- La replique est finie quand sa duree (creature_text.Duration) et son son
-- sont passes, plus PAUSE : c est ce que Next attend.
function Say(name, text, kind, anim, duration)
	local actor = Cast[name]
	if not actor then
		return
	end
	if FastForward then
		Speech = { done = true }
		return
	end
	local speech = { done = false }
	local line = { speaking = true }
	Speech = speech
	actor.talk = (actor.talk or 0) + 1
	local token = actor.talk
	-- Stereo : la voix est jouee a volume constant, sans spatialisation (sinon
	-- elle baisse a gauche, a droite ou derriere un mur), et la bulle reste
	-- au-dessus de l acteur.
	actor:BroadcastTextStereo(kind, text)
	if kind ~= BroadcastType.Emote and not actor.quiet then
		scene:AddCoroutine(function()
			local elapsed = 0
			if anim > 0 and not TALK_ANIMS[anim] then
				actor:PlayOneShotFullBodyAnim(anim)
				elapsed = AnimLength(actor, anim)
				Wait(elapsed)
			end
			local talk = TALK_ANIM
			if kind == BroadcastType.Yell then
				talk = YELL_ANIM
			elseif duration < SHORT_TALK then
				talk = TALK_ANIMS[anim] and anim or SHORT_TALK_ANIM
			end
			-- Une animation ne demarre que si l essentiel tient dans le reste de
			-- la replique (TalkSubdued deborderait de plusieurs secondes) ; la
			-- fin est comblee par EmoteTalk, plus courte. Toujours au moins une
			-- animation ; une replique suivante du meme acteur prend le relais.
			local first = true
			while line.speaking and actor.talk == token do
				local remaining = duration - elapsed
				local current = nil
				if remaining >= AnimLength(actor, talk) * TALK_FIT then
					current = talk
				elseif talk ~= SHORT_TALK_ANIM and remaining >= AnimLength(actor, SHORT_TALK_ANIM) * FILL_FIT then
					current = SHORT_TALK_ANIM
				elseif first then
					current = talk == TALK_ANIM and SHORT_TALK_ANIM or talk
				end
				if not current then
					break
				end
				local length = AnimLength(actor, current)
				actor:PlayOneShotSplitBodyAnim(current)
				Wait(length)
				elapsed = elapsed + length
				first = false
			end
			-- En marche, le haut du corps revient a l emote state (WalkPose).
			if actor.talk == token and actor.moving and actor.emoteState then
				actor:PlayLoopingSplitBodyAnim(actor.emoteState)
			end
		end)
	end
	scene:AddCoroutine(function()
		Wait(duration)
		actor:WaitBroadcastSoundComplete()
		line.speaking = false
		Wait(PAUSE)
		speech.done = true
	end)
end

-- Next(delai) : le delai ecrit est un minimum, la replique en cours va
-- toujours jusqu au bout (CustomScenario::DialogueClock).
-- Chaque Next() passe est signale au serveur ("Step<n>"), qui sait ainsi ou
-- reprendre la scene ; en accelere, il compte jusqu au point de reprise.
function Next(delay)
	Step = Step + 1
	if FastForward then
		if Step >= Resume then
			FastForward = false
		end
		return
	end
	Wait(delay)
	local speech = Speech
	scene:WaitCondition(function() return speech.done end)
	Cue("Step" .. Step)
end

-- Emotes --------------------------------------------------------------------

-- HandleEmoteCommand : animation jouee une fois.
function Emote(name, anim)
	if not FastForward then
		Cast[name]:PlayOneShotFullBodyAnim(anim)
	end
end

-- SetEmoteState : animation en boucle, 0 pour revenir a la normale.
-- Elle occupe tout le corps a l arret ; en marche, elle passe sur le haut du
-- corps (WalkPose) pour que les jambes gardent l animation de marche.
function EmoteState(name, anim)
	local actor = Cast[name]
	if anim and anim > 0 then
		actor.emoteState = anim
		if actor.moving then
			actor:PlayLoopingSplitBodyAnim(anim)
		else
			actor:PlayLoopingFullBodyAnim(anim)
		end
	else
		if actor.emoteState and actor.moving then
			actor:StopAnimKit(AnimKits and AnimKits.LoopingSplitBody or LOOPING_SPLIT_BODY)
		end
		actor.emoteState = nil
		actor:StopLoopingFullBodyAnim()
	end
end

-- Un emote state (main sur l epee du chevalier...) bloque les jambes : en
-- marche, il passe sur le haut du corps, et reprend tout le corps a
-- l arrivee.
function WalkPose(actor, moving)
	local state = actor.emoteState
	if not state then
		return
	end
	if moving then
		actor:StopLoopingFullBodyAnim()
		actor:PlayLoopingSplitBodyAnim(state)
	else
		actor:StopAnimKit(AnimKits and AnimKits.LoopingSplitBody or LOOPING_SPLIT_BODY)
		actor:PlayLoopingFullBodyAnim(state)
	end
end

-- Animation d entree puis animation en boucle (tendre un objet, puis
-- garder le bras tendu...).
function EmoteThenState(name, start, loop)
	local actor = Cast[name]
	actor.emoteChain = (actor.emoteChain or 0) + 1
	local token = actor.emoteChain
	if FastForward then
		EmoteState(name, loop)
		return
	end
	actor:PlayOneShotFullBodyAnim(start)
	scene:AddCoroutine(function()
		Wait(AnimLength(actor, start))
		if actor.emoteChain == token then
			EmoteState(name, loop)
		end
	end)
end

-- Fin d une animation en boucle par son animation de sortie.
function EndState(name, finish)
	local actor = Cast[name]
	actor.emoteChain = (actor.emoteChain or 0) + 1
	EmoteState(name, 0)
	if not FastForward then
		actor:PlayOneShotFullBodyAnim(finish)
	end
end

-- A genoux : KneelStart puis KneelLoop sur tout le corps. Les animations de
-- parole passant par le haut du corps, l acteur peut parler a genoux.
function Kneel(name)
	local actor = Cast[name]
	actor.kneel = (actor.kneel or 0) + 1
	local token = actor.kneel
	if FastForward then
		EmoteState(name, Animations.KneelLoop)
		return
	end
	actor:PlayOneShotFullBodyAnim(Animations.KneelStart)
	scene:AddCoroutine(function()
		Wait(AnimLength(actor, Animations.KneelStart))
		if actor.kneel == token then
			EmoteState(name, Animations.KneelLoop)
		end
	end)
end

-- Se relever (KneelEnd). Renvoie la duree de l animation.
function StandUp(name)
	local actor = Cast[name]
	actor.kneel = (actor.kneel or 0) + 1
	EmoteState(name, 0)
	if FastForward then
		return 0
	end
	actor:PlayOneShotFullBodyAnim(Animations.KneelEnd)
	return AnimLength(actor, Animations.KneelEnd)
end

-- PlayOneShotAnimKitId.
function AnimKit(name, kit)
	if not FastForward then
		Cast[name]:PlayOneShotAnimKit(kit)
	end
end

-- Sorts et auras --------------------------------------------------------------

-- Aura posee (CastSpell / AddAura d un sort a etat) : la macro SV donne le
-- SpellVisualID, dont le StateKit tourne jusqu au RemoveAura.
function Aura(name, key, visual)
	local actor = Cast[name]
	RemoveAura(name, key)
	actor.auras[key] = actor:PlaySpellStateVisual(visual)
end

function RemoveAura(name, key)
	local actor = Cast[name]
	local handle = actor.auras[key]
	if handle then
		actor:ClearSpellVisual(handle)
		actor.auras[key] = nil
	end
end

-- RemoveAllAuras.
function RemoveAllAuras(name)
	for key in pairs(Cast[name].auras) do
		RemoveAura(name, key)
	end
end

-- Sort instantane : seulement son visuel de lancer.
function CastVisual(name, visual)
	if not FastForward then
		Cast[name]:PlaySpellCastVisual(visual)
	end
end

-- Visuel d impact sur l acteur : pour les sorts dont le SpellVisual n a que
-- des evenements d impact (TELEPORT_DUMMY), que CastVisual ne joue pas.
function ImpactVisual(name, visual)
	if not FastForward then
		Cast[name]:PlaySpellImpactVisual(visual)
	end
end

-- Sort lance sur une cible (trait, charge...) : le client joue le sort
-- complet, missile compris.
function CastAt(name, spell, target)
	if not FastForward then
		Cast[name]:CastSpell(spell, TargetOf(target))
	end
end

-- Deplacements ----------------------------------------------------------------

-- Points intermediaires calcules dans chaque virage arrondi.
SMOOTH_STEPS = 8
-- Distance au noeud ou commence et finit l arrondi d un virage (yd).
CORNER_RADIUS = 2.5
-- Un noeud de tete plus proche que ca de l acteur est saute (Follow).
SKIP_NODE_DISTANCE = 2.5
-- Ecart maximal entre deux points des lignes droites (yd) : avec des points
-- aussi rapproches que dans les virages, la vitesse reste constante.
STRAIGHT_STEP = 0.5

-- Virages arrondis : chaque noeud intermediaire est remplace par un arc
-- (Bezier quadratique, le noeud en point de controle) qui commence et finit
-- a CORNER_RADIUS du noeud au plus, et jamais au-dela de la moitie des
-- segments voisins. Contrairement a Catmull-Rom, l arc ne deborde jamais :
-- pas de boucle ni de crochet quand les noeuds sont rapproches ou le virage
-- serre. La hauteur suit la ligne droite (sinon la courbe plonge sous le sol
-- la ou une rampe commence). Les lignes droites entre deux virages sont
-- decoupees tous les STRAIGHT_STEP : un seul long segment entre deux arcs
-- faisait accelerer puis freiner l acteur. Les points trop proches sont
-- fusionnes.
function SmoothPath(points)
	local clean = {}
	for _, point in ipairs(points) do
		if #clean == 0 or (point - clean[#clean]):Magnitude() > 0.1 then
			table.insert(clean, point)
		end
	end
	if #clean < 3 then
		return clean
	end

	local curve = { clean[1] }
	local function addStraight(from, to)
		local steps = math.max(1, math.ceil((to - from):Magnitude() / STRAIGHT_STEP))
		for s = 1, steps do
			local t = s / steps
			table.insert(curve, Vector:New(from.x + (to.x - from.x) * t,
				from.y + (to.y - from.y) * t,
				from.z + (to.z - from.z) * t))
		end
	end

	local lastCorner = clean[1]
	for i = 2, #clean - 1 do
		local a, p, b = clean[i - 1], clean[i], clean[i + 1]
		local toA, toB = a - p, b - p
		local lenA, lenB = toA:Magnitude(), toB:Magnitude()
		local dA = math.min(CORNER_RADIUS, lenA / 2)
		local dB = math.min(CORNER_RADIUS, lenB / 2)
		local q1 = Vector:New(p.x + toA.x * dA / lenA, p.y + toA.y * dA / lenA, p.z + toA.z * dA / lenA)
		local q2 = Vector:New(p.x + toB.x * dB / lenB, p.y + toB.y * dB / lenB, p.z + toB.z * dB / lenB)
		-- Ligne droite depuis la fin du virage precedent
		if (q1 - lastCorner):Magnitude() > 0.05 then
			addStraight(lastCorner, q1)
		end
		for step = 1, SMOOTH_STEPS do
			local t = step / SMOOTH_STEPS
			local u = 1 - t
			table.insert(curve, Vector:New(u * u * q1.x + 2 * u * t * p.x + t * t * q2.x,
				u * u * q1.y + 2 * u * t * p.y + t * t * q2.y,
				q1.z + (q2.z - q1.z) * t))
		end
		lastCorner = q2
	end
	if (clean[#clean] - lastCorner):Magnitude() > 0.05 then
		addStraight(lastCorner, clean[#clean])
	end

	-- Deux virages qui se touchent partagent un point : pas de segment nul.
	local result = { curve[1] }
	for i = 2, #curve do
		if (curve[i] - result[#result]):Magnitude() > 0.05 then
			table.insert(result, curve[i])
		end
	end
	return result
end

-- MovePath / MovePoint : l acteur suit la courbe passant par chaque noeud
-- de path, tourne dans le sens de la marche (MoveRotControl.Tangent), puis
-- prend l orientation finale. path.speed vaut WALK ou RUN ; la macro PATH
-- d un script de scene donne la table toute faite.
-- Les noeuds sont releves au sol : l acteur suit leur hauteur exacte, sans
-- accroche au sol (le client le collait sinon au terrain sous les rampes et
-- les escaliers de la tour).
-- Un nouveau deplacement remplace celui en cours (MotionMaster).
function Follow(name, path, finalFacing, onArrive)
	local actor = Cast[name]
	actor.move = actor.move + 1
	local token = actor.move
	if not actor.moving then
		actor.moving = true
		WalkPose(actor, true)
	end
	actor:SetSnapToGround(false)
	local nodes = {}
	for _, node in ipairs(path) do
		table.insert(nodes, Vector:New(node[1], node[2], node[3]))
	end
	-- Noeuds de tete trop proches de l acteur, ou situes derriere lui par
	-- rapport a la suite du chemin : y aller d abord le fait hesiter sur
	-- place ou faire demi-tour. Le premier noeud, la ou le PNJ se trouve
	-- deja, tombe le plus souvent ici.
	while #nodes >= 2 do
		local toFirst = nodes[1] - actor:GetPosition()
		local onward = nodes[2] - nodes[1]
		local behind = toFirst.x * onward.x + toFirst.y * onward.y < 0
		if toFirst:Magnitude() > SKIP_NODE_DISTANCE and not behind then
			break
		end
		table.remove(nodes, 1)
	end
	StartWalking(actor)
	-- En accelere, l acteur est deja au bout du chemin.
	local function arrive()
		local o = finalFacing or path[#path][4]
		if o then
			actor:SetFacing(math.deg(o))
		end
		StopWalking(actor)
		if onArrive then
			onArrive()
		end
	end
	if FastForward then
		local last = path[#path]
		local o = finalFacing or last[4]
		Teleport(actor, last[1], last[2], last[3], o and math.deg(o))
		arrive()
		return
	end
	scene:AddCoroutine(function()
		-- Seulement les noeuds du chemin : le moteur part deja de la ou se
		-- trouve l acteur. Lisser aussi sa position de depart creait un
		-- premier arc qui le tirait en arriere ou de travers (micro-pause,
		-- demi-tour).
		local curve = SmoothPath(nodes)
		if #curve > 0 then
			local moveData = MoveData:Default()
			moveData.posControl = MovePosControl.ConstantSpeed
			moveData.yawControl = MoveRotControl.Tangent
			moveData.isGroundSnapping = false
			moveData.teleportToFirstPoint = false
			moveData.speed = path.speed
			moveData.points = {}
			for i, pos in ipairs(curve) do
				moveData.points[i] = MovePoint:Default()
				moveData.points[i].pos = pos
			end
			actor:WaitMove(moveData)
		end
		if actor.move ~= token then
			return
		end
		arrive()
	end)
end

-- MovePoint vers une seule destination.
function MoveTo(name, speed, x, y, z, o, onArrive)
	Follow(name, { speed = speed, { x, y, z, o } }, o, onArrive)
end

-- Copie d un chemin (macro PATH) sans les noeuds d indices donnes (1 pour
-- le premier) : un crochet du chemin serveur que la scene n a pas a suivre.
function PathWithout(path, ...)
	local skip = {}
	for _, index in ipairs({ ... }) do
		skip[index] = true
	end
	local copy = { speed = path.speed }
	for index, node in ipairs(path) do
		if not skip[index] then
			table.insert(copy, node)
		end
	end
	return copy
end

-- MoveBackward : l acteur recule a BACK_WALK en gardant une orientation fixe :
-- vers faceTarget s il y en a un (Peine qui surveille Thalen, Hedric le
-- portail), sinon dos a la direction du recul, comme un spline Backward cote
-- serveur. A l arrivee, il prend l orientation o, ou fait face a faceTarget.
-- L orientation suit MoveRotControl.Angle, interpolee depuis l orientation
-- de depart jusqu a celle du point. AngleAbs (les toupies des scenes
-- officielles, yaw 1080 ou 1440) ne ramene pas l angle entre 0 et 360 :
-- Hedric partait de 206 degres vers 2 et tournait pendant tout le recul.
-- L acteur se tourne donc d abord sur place, puis recule sans tourner.
-- Le demi-tour se fait BACK_TURN_RATE fois plus vite que les rotations
-- ordinaires de l acteur (SetFacingTurnRate), puis la vitesse normale
-- revient.
BACK_WALK = 1.4         -- recul lent
BACK_STEADY = 2.2       -- recul assure (Hedric devant le portail)
BACK_HURRY = 3          -- recul vif (Peine qui s ecarte de Thalen)
BACK_TURN_RATE = 3
BACK_TURN_TOLERANCE = 5 -- degres : l acteur est tourne
BACK_TURN_TIMEOUT = 1.5 -- secondes : au-dela, il recule quand meme
BACK_TURN_STEP = 0.05

function MoveBackward(name, speed, x, y, z, o, faceTarget)
	local actor = Cast[name]
	actor.move = actor.move + 1
	local token = actor.move
	if not actor.moving then
		actor.moving = true
		WalkPose(actor, true)
	end
	StartWalking(actor)
	actor:SetSnapToGround(false)
	local start = actor:GetPosition()
	local dest = Vector:New(x, y, z)
	local at = faceTarget and PositionOf(faceTarget)
	local yaw
	if at then
		-- Vers la cible depuis l arrivee : le recul se fait dans l axe de la
		-- cible. Depuis le depart, l angle ne vaut rien quand l acteur est
		-- presque sur elle (Hedric part du portail).
		yaw = math.deg(math.atan2(at.y - dest.y, at.x - dest.x))
	else
		yaw = math.deg(math.atan2(start.y - dest.y, start.x - dest.x))
	end
	if FastForward then
		Teleport(actor, x, y, z, yaw)
		StopWalking(actor)
		if faceTarget then
			FaceTo(name, faceTarget)
		elseif o then
			Face(name, o)
		end
		return
	end
	scene:AddCoroutine(function()
		local turnRate = actor:GetFacingTurnRate()
		actor:SetFacingTurnRate(turnRate * BACK_TURN_RATE)
		actor:SetFacing(yaw)
		local waited = 0
		while math.abs((yaw - actor:GetTransform().yaw + 540) % 360 - 180) > BACK_TURN_TOLERANCE and waited < BACK_TURN_TIMEOUT do
			Wait(BACK_TURN_STEP)
			waited = waited + BACK_TURN_STEP
		end
		actor:SetFacingTurnRate(turnRate)
		if actor.move ~= token then
			return
		end
		local moveData = MoveData:Default()
		moveData.posControl = MovePosControl.ConstantSpeed
		moveData.yawControl = MoveRotControl.Angle
		moveData.isGroundSnapping = false
		moveData.speed = speed
		-- Seulement la destination : le moteur part de la position actuelle.
		moveData.teleportToFirstPoint = false
		moveData.points = { MovePoint:Default() }
		moveData.points[1].pos = dest
		moveData.points[1].yaw = yaw
		actor:WaitMove(moveData)
		if actor.move ~= token then
			return
		end
		-- L acteur est arrete d abord : l orientation finale s applique
		-- directement, sans passer par la file des orientations en route.
		StopWalking(actor)
		if faceTarget then
			FaceTo(name, faceTarget)
		elseif o then
			Face(name, o)
		end
	end)
end

-- MoveCloserAndStop : s arrete a distance de la cible, face a elle.
function MoveCloser(name, target, distance, speed)
	local actor = Cast[name]
	local goal = TargetOf(target):GetPosition()
	local from = actor:GetPosition()
	local length = (from - goal):Magnitude()
	if length <= distance then
		return
	end
	local ratio = distance / length
	local x = goal.x + (from.x - goal.x) * ratio
	local y = goal.y + (from.y - goal.y) * ratio
	local z = goal.z + (from.z - goal.z) * ratio
	MoveTo(name, speed or WALK, x, y, z, math.atan2(goal.y - y, goal.x - x))
end

-- Attend la fin du deplacement d un acteur.
function WaitArrival(name)
	local actor = Cast[name]
	scene:WaitCondition(function() return not actor.moving end)
end

-- Serveur -------------------------------------------------------------------

-- Signal propre a la scene (portail, barriere...). Le serveur ne le traite
-- qu une fois, quel que soit le nombre de joueurs qui regardent.
function Cue(name)
	scene:TriggerServerEvent(name)
end

-- Debut de scene : une fois tous les acteurs apparus.
function Ready()
	Cue("Ready")
end

-- Fin de scene : le serveur remet les vrais PNJ en place pendant que les
-- acteurs sont encore la, puis la scene se ferme.
function Finish()
	Cue("Done")
	Wait(LINGER)
	scene:EndScene()
end
