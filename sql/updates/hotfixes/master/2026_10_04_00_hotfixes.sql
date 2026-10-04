-- ============================================================================
-- Scene 313 « What Had To Be Done » : Jaina et Varian au donjon de Hurlevent
-- (rendu de la quete 500000, copie de 32423). Lignes Blizzard reecrites (memes ID) :
--   9419 + 9453 : init + liste principale (texte enchaine par SceneScript 9419 -> 9453)
--   9454 : Jaina - arrive par son portail, rejoint Varian, rouvre le portail et le traverse
--   9455 : Varian - deja present, fait quelques pas vers le portail apres le depart de Jaina,
--          puis retourne a sa place (le vrai Varian reprend le relais en fin de scene)
-- 9479 (attente de la musique) est inchange. Repliques, musiques et animations d'origine
-- conservees (BroadcastText 69619-69635). Modernise : Jaina 68677 (scenario de la Purge),
-- portail 8fx_portalroom_dalaran (2746855), visuels « Jaina Portal [DNT] » (146822) et
-- « Teleport: Dalaran - Northrend » (182728) joues par SpellVisualID, CastSpellAtActor
-- ne rendant plus rien.
-- ============================================================================

DELETE FROM `scene_script_text` WHERE `ID` IN (9419, 9453, 9454, 9455) AND `VerifiedBuild`=-1;
INSERT INTO `scene_script_text` (`ID`,`Name`,`Script`,`VerifiedBuild`) VALUES
(9419, 'Shrine of Seven Stars 5.1 - What Had To Be Done - INIT - ZTO', '----------
-- Init --
----------
-- Emerald : scene jouee au donjon de Hurlevent, au rendu de la quete 500000
-- (copie de 32423). Jaina arrive de Dalaran par son portail et repart par le meme.
-- CastSpellAtActor ne rend plus rien : visuels joues par SpellVisualID.

--Visuals
portalVisualID = 146822   -- Jaina Portal [DNT] (457953)
teleportVisualID = 182728 -- Teleport: Dalaran - Northrend (53140)

--Creatures and Models
varianCreatureID = 68690
jainaCreatureID = 68677   -- Jaina du scenario de la Purge de Dalaran
portalModelID = fid(2746855) -- 8fx_portalroom_dalaran

--Positions (facings en degres)
portalX = -8400.857422
portalY = 322.041290
portalZ = 147.014954
portalFacing = 262.74
jainaTalkX = -8399.884766
jainaTalkY = 314.449066
jainaTalkFacing = 346.75
varianTalkFacing = 167.44
jainaExitX = -8401.173
jainaExitY = 319.561
jainaExitFacing = 82.74

varian = SmoothPhaseSpawnActor (varianCreatureID, -8393.531250, 313.033752, 147.014954, 202.64, 1, true)
jaina = SpawnActor (jainaCreatureID, portalX, portalY, portalZ, portalFacing, 1, true)
jaina:SetSheatheState(SheatheState.Sheathed, true)
portal = SpawnModelActor ( portalModelID, portalX, portalY, portalZ, portalFacing, 1, true)

FadeOut(jaina, 0)
FadeOut(portal, 0)

------------------------
-- Master Action List --
------------------------
Wait(1)
SendTrigger("jaina - arrive")
WaitForTrigger("jaina - arrived")

jainaMusic = varian:PlayMusic(34893) --Jaina Music Start
varian:BroadcastTextStereo(BroadcastType.Say, 69619) -- VO Line 1 - "Has there been an attack?"
PlayOneShotSplitBodyAnim( varian, Animations.EmoteTalkExclamation )
Wait(2)
varian:PlayOneShotAnimKit(1521)
Wait(2)
WaitForTrigger("jaina - in place")
Wait(1)

jaina:BroadcastTextStereo(BroadcastType.Say, 69620) -- VO Line 2 - "Kirin Tor betrayed"
PlayOneShotSplitBodyAnim( jaina, Animations.EmoteTalk )
Wait(5)
Wait(.5)

varian:BroadcastTextStereo(BroadcastType.Say, 69621) -- VO Line 3 - "How?"
PlayOneShotSplitBodyAnim( varian, Animations.EmoteTalkQuestion )
Wait(1)
Wait(.5)

jaina:BroadcastTextStereo(BroadcastType.Say, 69622) -- VO Line 4 - "Purge"
PlayOneShotSplitBodyAnim( jaina, Animations.EmoteTalk )
Wait(7)
Wait(.5)

varian:BroadcastTextStereo(BroadcastType.Say, 69624) -- VO Line 5 - "But you said.."
PlayOneShotSplitBodyAnim( varian, Animations.EmoteTalk )
Wait(1)

jaina:BroadcastTextStereo(BroadcastType.Say, 69623) -- VO Line 6 - "I know what I said..."
PlayOneShotSplitBodyAnim( jaina, Animations.EmoteTalk )
Wait(4)
Wait(1)

jainaMusic:Stop()
jainaMusic = varian:PlayMusic(34894) --Jaina Music part 2 Start
SendTrigger("Music Wait")
varian:BroadcastTextStereo(BroadcastType.Say, 69625) -- VO Line 7 - "What of the Sin''dorei - the Sunreavers?"
PlayOneShotSplitBodyAnim( varian, Animations.EmoteTalkQuestion )
Wait(3)
Wait(.5)

jaina:BroadcastTextStereo(BroadcastType.Say, 69626) -- VO Line 8 - "Violet Hold"
PlayOneShotSplitBodyAnim( jaina, Animations.EmoteTalk )
Wait(6)
Wait(.5)', -1),
(9453, '[1] Shrine of Seven Stars 5.1 - What Had To Be Done - INIT - ZTO', '
varian:BroadcastTextStereo(BroadcastType.Say, 69627) -- VO Line 9 - "Jaina, you need to talk to me before you act-"
PlayOneShotSplitBodyAnim( varian, Animations.EmotePoint )
Wait(2)
PlayOneShotSplitBodyAnim( varian, Animations.EmoteTalk )
Wait(2)

jaina:BroadcastTextStereo(BroadcastType.Say, 69628) -- VO Line 10 - "How I run the Kirin Tor is my business."
PlayOneShotSplitBodyAnim( jaina, Animations.EmoteNo )
Wait(2)
PlayOneShotSplitBodyAnim( jaina, Animations.EmoteTalk )
Wait(2)
Wait(.5)

varian:BroadcastTextStereo(BroadcastType.Say, 69629) -- VO Line 11 - "I was trying to negotiate with the Sin''dorei"
PlayLoopingSplitBodyAnim( varian, Animations.EmoteTalk )
Wait(8)
StopLoopingSplitBodyAnim( varian, Animations.EmoteTalk )

jaina:BroadcastTextStereo(BroadcastType.Say, 69630) -- VO Line 12 - "They chose their own path."
PlayOneShotSplitBodyAnim( jaina, Animations.EmoteTalk )
Wait(3)
Wait(.5)

varian:BroadcastTextStereo(BroadcastType.Say, 69631) -- VO Line 13 - "You''ve driven them back to the Horde."
PlayOneShotSplitBodyAnim( varian, Animations.EmotePoint )
Wait(4)
Wait(.5)

jaina:BroadcastTextStereo(BroadcastType.Say, 69632) -- VO Line 14 - " Once Horde, always Horde"
PlayOneShotSplitBodyAnim( jaina, Animations.EmoteTalk )
Wait(5)
SendTrigger("jaina - prep exit")
Wait(2)
Wait(.5)

varian:BroadcastTextStereo(BroadcastType.Say, 69633) -- VO Line 15 - " The Alliance must act as one."
PlayOneShotSplitBodyAnim( varian, Animations.EmoteTalk )
Wait(6)
Wait(.5)

-- Portail ouvert : face au portail, Jaina tourne seulement la tete vers Varian avant de partir
WaitForTrigger("jaina - portal open")
jaina:SetFullHeadFacingToTarget(varian, Vector:New())
Wait(.5)
jaina:BroadcastTextStereo(BroadcastType.Say, 69634) -- VO Line 16 - "Don''t get soft on me"
jaina:PlayOneShotAnimKit(1530) -- Talking head
jaina:WaitBroadcastSoundComplete()
jaina:ClearHeadFacing()
Wait(.5)
SendTrigger("jaina - teleport")
SendTrigger("varian - cue 2")

WaitForTrigger("varian - line")
varian:BroadcastTextStereo(BroadcastType.Say, 69635) -- VO Line 17 - "The game has changed."
PlayOneShotSplitBodyAnim( varian, Animations.EmoteTalk )
Wait(1)
Wait(.5)

WaitForTrigger("varian - finished")
WaitForTrigger("Music Done")
scene:EndScene()', -1),
(9454, 'Shrine of Seven Stars 5.1 - What Had To Be Done - jaina - ZTO', '-- Arrivee : le portail s''ouvre, Jaina en sort et rejoint aussitot Varian,
-- le portail se referme derriere elle
WaitForTrigger("jaina - arrive")
FadeIn(portal, 1)
Wait(1.5)
jaina:PlaySpellImpactVisual( teleportVisualID )
FadeIn(jaina, .5)
varian:SetHeadFacingToTarget( jaina, Vector:New() )
SendTrigger("jaina - arrived")
RunToLocation( jaina, 2.5, jainaTalkX, jainaTalkY, portalZ, nil, true )
Wait(1)
FadeOut(portal, 1.5)

-- Ils se font face
jaina:WaitMovementComplete()
jaina:SetFacing(jainaTalkFacing)
varian:ClearHeadFacing()
varian:SetFacing(varianTalkFacing)
SendTrigger("jaina - in place")

-- Depart : elle rouvre son portail a son point d''arrivee
WaitForTrigger("jaina - prep exit")
varian:SetHeadFacingToTarget( jaina, Vector:New() )
RunToLocation( jaina, 2, jainaExitX, jainaExitY, portalZ, jainaExitFacing )
local portalPrecast = jaina:PlaySpellPreCastVisual( portalVisualID )
Wait(1.5)
jaina:ClearSpellVisual( portalPrecast )
jaina:PlaySpellCastVisual( portalVisualID )
FadeIn(portal, 2)
Wait(2)
SendTrigger("jaina - portal open")

-- Elle passe le portail avec l''effet de teleportation, le portail se referme
WaitForTrigger("jaina - teleport")
RunToLocation( jaina, 2, portalX, portalY, portalZ, jainaExitFacing )
jaina:PlaySpellCastVisual( teleportVisualID )
Wait(.5)
FadeOut(jaina, 1)
Wait(1)
FadeOut(portal, 1.5)', -1),
(9455, 'Shrine of Seven Stars 5.1 - What Had To Be Done - varian - ZTO', '-- Varian est deja la : il ne bouge qu''une fois Jaina partie,
-- de quelques pas vers le portail qui se referme, puis retourne a sa place :
-- le vrai Varian reprend le relais a la fin de la scene.
WaitForTrigger("varian - cue 2")
Wait(2)
varian:ClearHeadFacing() -- Jaina partie : il cesse de la suivre du regard
RunToLocation( varian, 2.5, -8396.462, 316.637, 147.014954, 129.12 )
Wait(1)
SendTrigger("varian - line")
Wait(3)
RunToLocation( varian, 2.5, -8393.531250, 313.033752, 147.014954, 202.64 )
SendTrigger("varian - finished")', -1);

SET @TABLE_HASH := 537670055;   -- SceneScriptText.db2
SET @HOTFIX_ID  := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);
SET @UNIQUE_ID  := TO_DAYS('2026-10-04');   -- Un UniqueId par jour : meme valeur pour tous les hotfixes du jour, +1 le lendemain
DELETE FROM `hotfix_data` WHERE `TableHash`=@TABLE_HASH AND `RecordId` IN (9419, 9453, 9454, 9455);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(@HOTFIX_ID,     @UNIQUE_ID, @TABLE_HASH, 9419, 1, -1),
(@HOTFIX_ID + 1, @UNIQUE_ID, @TABLE_HASH, 9453, 1, -1),
(@HOTFIX_ID + 2, @UNIQUE_ID, @TABLE_HASH, 9454, 1, -1),
(@HOTFIX_ID + 3, @UNIQUE_ID, @TABLE_HASH, 9455, 1, -1);
