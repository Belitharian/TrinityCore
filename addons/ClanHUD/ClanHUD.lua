--[[
    Clan HUD — fenetre unique de supervision du module Clans.

    Mise en page :
      - en haut : etat du monde (heure, jour/nuit, population, malades) ;
      - en dessous : UNE COLONNE PAR PNJ, toutes visibles d'un coup. La zone defile
        HORIZONTALEMENT (molette) pour que la fenetre reste de largeur raisonnable.

    Cote serveur (GM) :
      - .clan world  : alimente le bandeau "monde" (1x/s).
      - .clan hudall : alimente les colonnes de TOUS les PNJ (1x/s).
      - .clan hud    : flux d'un seul PNJ (la cible) ; alimente aussi cette fenetre.
      - .clan monitor: flux texte dans le chat (sans addon).

    Commandes addon :
      /clanhud        : affiche/masque la fenetre.
      /clanhud debug  : trace les clics du bouton "Cibler" (diagnostic du ciblage).
]]

local ADDON_PREFIX = "CLANHUD"

ClanHUDDB = ClanHUDDB or {}
ClanHUDDB.pos = ClanHUDDB.pos or {}

-- Geometrie des colonnes.
local COL_W        = 200  -- largeur d'une colonne (un PNJ)
local COL_H        = 278  -- hauteur d'une colonne
local VISIBLE_COLS = 4    -- colonnes visibles sans defiler
local SCROLL_W     = COL_W * VISIBLE_COLS

-- Cache des PNJ recus : id -> dernier paquet de donnees (avec _last = horodatage).
local members = {}
local main            -- la fenetre unique
local columnsChild    -- conteneur defilant des colonnes
local columns  = {}   -- index -> frame de colonne (recyclee)

-- ---------------------------------------------------------------------------
-- Utilitaires
-- ---------------------------------------------------------------------------
local function parse(msg)
    local t = {}
    for pair in string.gmatch(msg, "([^;]+)") do
        local k, v = string.match(pair, "([^=]+)=(.*)")
        if k then t[k] = v end
    end
    return t
end

local function yn(v)
    return (v == "1") and "|cff40ff40oui|r" or "|cffff6060non|r"
end

-- Role "metier" derive du genre + de l'etape (division du travail d'epoque).
--   st = "Enfant"/"Adulte"/"Ancien" ; ge = "M"/"F".
local function roleText(st, ge)
    if st == "Enfant" then return "|cff9adcffEnfant (jeu)|r" end
    if ge == "F" then return "|cffff9cc0Femme (foyer)|r" end
    return "|cff9ad19aHomme (chasse)|r"
end

-- Decode le masque d'affliction (bit0=maladie, bit1=poison, bit2=saignement).
local function afflictionText(mask)
    mask = tonumber(mask) or 0
    if mask == 0 then
        return "|cff40ff40sain|r"
    end
    local parts = {}
    if bit.band(mask, 1) ~= 0 then table.insert(parts, "|cffcc66ffMaladie|r") end
    if bit.band(mask, 2) ~= 0 then table.insert(parts, "|cff66ff66Poison|r") end
    if bit.band(mask, 4) ~= 0 then table.insert(parts, "|cffff4040Saignement|r") end
    return table.concat(parts, ", ")
end

-- Couleur de la barre de vie : vert (100%) -> rouge (0%).
local function hpColor(pct)
    pct = tonumber(pct) or 0
    return math.min(1, 2 - pct / 50), math.min(1, pct / 50), 0.1
end

-- Configure un bouton SECURISE pour cibler un PNJ par son nom exact.
-- Meme approche que RareScanner : "/cleartarget" puis "/targetexact <nom>". Le ciblage est
-- une action protegee : il faut un bouton securise + un clic materiel. Les attributs
-- securises ne peuvent pas etre modifies en combat.
local function setTargetMacro(btn, name)
    if not btn or not name or name == "" then return end
    if InCombatLockdown() then return end
    if btn.clanTargetName == name then return end
    btn:SetAttribute("macrotext", "/cleartarget\n/targetexact " .. name)
    btn.clanTargetName = name
end

-- Bouton "Cibler" : on n'herite QUE de SecureActionButtonTemplate et on l'habille a la main.
-- Combiner deux templates peut ecraser le OnClick securise, auquel cas la macro n'est jamais
-- executee et le ciblage ne fonctionne pas.
local function createTargetButton(parent, name)
    local btn = CreateFrame("Button", name, parent, "SecureActionButtonTemplate")
    btn:RegisterForClicks("AnyUp", "AnyDown")
    btn:SetAttribute("type", "macro")

    local bg = btn:CreateTexture(nil, "BACKGROUND")
    bg:SetAllPoints()
    bg:SetColorTexture(0.25, 0.25, 0.32, 0.95)
    local hl = btn:CreateTexture(nil, "HIGHLIGHT")
    hl:SetAllPoints()
    hl:SetColorTexture(0.45, 0.45, 0.60, 0.9)

    btn.label = btn:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    btn.label:SetPoint("CENTER")
    btn.label:SetText("Cibler")

    -- Diagnostic (/clanhud debug) : PreClick s'execute hors du code securise, il ne casse
    -- donc rien et permet de verifier que le clic arrive et quelle macro est posee.
    btn:SetScript("PreClick", function(self)
        if ClanHUDDB.debug and DEFAULT_CHAT_FRAME then
            DEFAULT_CHAT_FRAME:AddMessage(string.format(
                "|cff88ccff[Clan HUD]|r clic Cibler | nom=%s | macro=%s",
                tostring(self.clanTargetName), tostring(self:GetAttribute("macrotext"))))
        end
    end)

    return btn
end

-- ---------------------------------------------------------------------------
-- Stele : fenetre d'epitaphe (affichee au clic sur une tombe)
-- ---------------------------------------------------------------------------
-- Fenetre 100% custom, et non un habillage du GossipFrame : l'apparence du gossip
-- (fond, police, couleur du texte) vit entierement cote client et n'est pas pilotable.
-- Le serveur se contente d'envoyer le texte ("ep=..."), on fait le reste ici.
local epitaphWindow

local function createEpitaphWindow()
    -- BackdropTemplate : donne le tuilage du fond ET la bordure en une seule declaration
    -- (bien plus propre que d'empiler des textures a la main).
    local f = CreateFrame("Frame", "ClanHUDEpitaph", UIParent, "BackdropTemplate")
    f:SetSize(380, 285)
    f:SetPoint("CENTER", UIParent, "CENTER", 0, 80)
    f:SetFrameStrata("DIALOG")
    f:SetBackdrop({
        bgFile   = "Interface\\FrameGeneral\\UI-Background-Marble",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 64, edgeSize = 16,
        insets = { left = 5, right = 5, top = 5, bottom = 5 },
    })
    f:SetBackdropBorderColor(0.55, 0.5, 0.4, 1)

    f:SetMovable(true)
    f:EnableMouse(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", f.StartMoving)
    f:SetScript("OnDragStop", f.StopMovingOrSizing)

    -- Fermeture par Echap, comme n'importe quelle fenetre du jeu.
    tinsert(UISpecialFrames, "ClanHUDEpitaph")

    local title = f:CreateFontString(nil, "OVERLAY")
    title:SetFont("Fonts\\MORPHEUS.TTF", 22, "")
    title:SetPoint("TOP", 0, -18)
    title:SetTextColor(0.85, 0.78, 0.6)
    title:SetText("Épitaphe")

    local rule = f:CreateTexture(nil, "ARTWORK")
    rule:SetColorTexture(0.55, 0.5, 0.4, 0.6)
    rule:SetHeight(1)
    rule:SetPoint("TOPLEFT", 40, -48)
    rule:SetPoint("TOPRIGHT", -40, -48)

    -- Nom du defunt, sous le titre : sa propre FontString (sinon l'epitaphe l'ecraserait).
    f.name = f:CreateFontString(nil, "OVERLAY")
    f.name:SetFont("Fonts\\MORPHEUS.TTF", 19, "")
    f.name:SetPoint("TOP", 0, -58)
    f.name:SetTextColor(1, 0.95, 0.8)
    f.name:SetJustifyH("CENTER")

    -- MORPHEUS : la police "livre/parchemin" du jeu, parfaite pour une inscription.
    -- Le texte porte ses propres codes couleur (|cff...) definis en base, par cause de mort.
    f.text = f:CreateFontString(nil, "OVERLAY")
    f.text:SetFont("Fonts\\MORPHEUS.TTF", 17, "")
    f.text:SetPoint("TOPLEFT", 28, -92)
    f.text:SetPoint("BOTTOMRIGHT", -28, 52)
    f.text:SetJustifyH("CENTER")
    f.text:SetJustifyV("MIDDLE")
    f.text:SetSpacing(4)

    local close = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    close:SetSize(110, 22)
    close:SetPoint("BOTTOM", 0, 16)
    close:SetText("Fermer")
    close:SetScript("OnClick", function() f:Hide() end)

    f:Hide()
    epitaphWindow = f
    return f
end

local function showEpitaph(text, name)
    if not epitaphWindow then createEpitaphWindow() end
    epitaphWindow.name:SetText(name or "")
    epitaphWindow.text:SetText(text or "")
    epitaphWindow:Show()
    epitaphWindow:Raise()
end

local function saveWindowPos(id, frame)
    local point, _, relPoint, x, y = frame:GetPoint()
    ClanHUDDB.pos[id] = { point = point, relPoint = relPoint, x = x, y = y }
end

local function makeDraggable(id, frame)
    frame:SetMovable(true)
    frame:EnableMouse(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetScript("OnDragStart", frame.StartMoving)
    frame:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        saveWindowPos(id, self)
    end)
    local saved = ClanHUDDB.pos[id]
    if saved then
        frame:ClearAllPoints()
        frame:SetPoint(saved.point, UIParent, saved.relPoint, saved.x, saved.y)
    end
end

local function backdrop(frame, r, g, b)
    local bg = frame:CreateTexture(nil, "BACKGROUND")
    bg:SetAllPoints()
    bg:SetColorTexture(0, 0, 0, 0.85)
    local border = frame:CreateTexture(nil, "BACKGROUND", nil, -1)
    border:SetPoint("TOPLEFT", -1, 1)
    border:SetPoint("BOTTOMRIGHT", 1, -1)
    border:SetColorTexture(r, g, b, 0.85)
end

-- Barre horizontale (vie / besoin) dimensionnee pour tenir dans une colonne.
local function createBar(parent, labelText, y, r, g, b)
    local label = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    label:SetPoint("TOPLEFT", 6, y)
    label:SetWidth(42)
    label:SetJustifyH("LEFT")
    label:SetText(labelText)

    local bar = CreateFrame("StatusBar", nil, parent)
    bar:SetSize(138, 13)
    bar:SetPoint("TOPLEFT", 50, y)
    bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    bar:SetStatusBarColor(r, g, b)
    bar:SetMinMaxValues(0, 100)
    bar:SetValue(0)

    local barbg = bar:CreateTexture(nil, "BACKGROUND")
    barbg:SetAllPoints()
    barbg:SetColorTexture(0.15, 0.15, 0.15, 0.9)

    local val = bar:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    val:SetPoint("CENTER")
    val:SetText("0")
    return bar, val
end

-- ---------------------------------------------------------------------------
-- Colonnes (une par PNJ)
-- ---------------------------------------------------------------------------
local function getColumn(i)
    local col = columns[i]
    if col then return col end

    col = CreateFrame("Frame", nil, columnsChild)
    col:SetSize(COL_W, COL_H)
    col:SetPoint("TOPLEFT", (i - 1) * COL_W, 0)

    -- Filet vertical de separation entre colonnes.
    local sep = col:CreateTexture(nil, "BACKGROUND")
    sep:SetPoint("TOPRIGHT", 0, 0)
    sep:SetPoint("BOTTOMRIGHT", 0, 0)
    sep:SetWidth(1)
    sep:SetColorTexture(0.35, 0.35, 0.4, 0.8)

    -- Nom : police standard (pas "Large").
    col.name = col:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    col.name:SetPoint("TOPLEFT", 6, 0)
    col.name:SetWidth(188)
    col.name:SetJustifyH("LEFT")

    col.sub = col:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    col.sub:SetPoint("TOPLEFT", 6, -17)
    col.sub:SetWidth(188)
    col.sub:SetJustifyH("LEFT")

    col.barHp,     col.valHp     = createBar(col, "Vie",     -38, 0.20, 0.80, 0.20)
    col.barHunger, col.valHunger = createBar(col, "Faim",    -60, 0.85, 0.55, 0.20)
    col.barThirst, col.valThirst = createBar(col, "Soif",    -77, 0.25, 0.55, 0.90)
    col.barEnergy, col.valEnergy = createBar(col, "Fatigue", -94, 0.60, 0.40, 0.80)
    col.barRepro,  col.valRepro  = createBar(col, "Repro",  -111, 0.85, 0.45, 0.65)

    local function line(y)
        local fs = col:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        fs:SetPoint("TOPLEFT", 6, y)
        fs:SetWidth(188)
        fs:SetJustifyH("LEFT")
        return fs
    end
    col.disLine    = line(-132)
    col.houseLine  = line(-150)  -- stock de la maison (viande/bois/pierre)
    col.mealLine   = line(-168)  -- repas prets + etat du foyer
    col.actLine    = line(-186)
    col.learnLine  = line(-204)
    col.spouseLine = line(-222)

    col.targetBtn = createTargetButton(col, "ClanHUDColTarget_" .. i)
    col.targetBtn:SetSize(110, 20)
    col.targetBtn:SetPoint("TOPLEFT", 6, -250)

    columns[i] = col
    return col
end

local function paintColumn(col, d)
    col.name:SetText(string.format("|cff88ccff%s|r", d.n or "?"))
    col.sub:SetText(string.format("clan %s | %sj | %s",
        d.cl or "?", d.ag or "?", roleText(d.st, d.ge)))

    local hp = tonumber(d.hp) or 0
    col.barHp:SetValue(hp)
    col.barHp:SetStatusBarColor(hpColor(hp))
    col.valHp:SetText(string.format("%d%%", hp))

    local hu = tonumber(d.hu) or 0
    local th = tonumber(d.th) or 0
    local en = tonumber(d.en) or 0
    local re = tonumber(d.re) or 0
    col.barHunger:SetValue(hu); col.valHunger:SetText(string.format("%d", hu))
    col.barThirst:SetValue(th); col.valThirst:SetText(string.format("%d", th))
    col.barEnergy:SetValue(en); col.valEnergy:SetText(string.format("%d", en))
    col.barRepro:SetValue(re);  col.valRepro:SetText(string.format("%d", re))

    col.disLine:SetText("Etat : " .. afflictionText(d.dis))
    -- Stock PARTAGE de la maison du clan (ce autour de quoi tourne toute la vie du clan).
    col.houseLine:SetText(string.format("Maison : V %s  B %s  P %s",
        d.hraw or "0", d.hwood or "0", d.hstn or "0"))
    -- Repas prets a manger : en rouge si 0 (personne ne peut se nourrir), vert sinon.
    local meals = tonumber(d.hmeal) or 0
    local mealsCol = (meals > 0) and "|cff40ff40" or "|cffff6060"
    -- Foyer : "eteint" en rouge, sinon "allume (42s)" avec le compte a rebours avant extinction
    -- (rouge s'il reste moins de 15s, jaune sinon).
    local fb = tonumber(d.fb) or 0
    local fireTxt
    if d.fi == "1" then
        local c = (fb < 15) and "ffff6060" or "ffffd100"
        fireTxt = string.format("|c%sallume (%ds)|r", c, fb)
    else
        fireTxt = "|cffff6060eteint|r"
    end
    col.mealLine:SetText(string.format("Repas : %s%s/%s|r   Foyer : %s",
        mealsCol, d.hmeal or "0", d.hmmax or "?", fireTxt))
    -- Sac plein : le membre doit rentrer livrer sa recolte avant de repartir en tournee. On le
    -- signale ici car c'est la cause la plus frequente d'un stock de maison qui ne monte pas.
    local bagTxt = (d.bag == "1") and "  |cffff6060[sac plein]|r" or ""
    col.actLine:SetText(string.format("Action : |cffffffff%s|r%s", d.act or "-", bagTxt))
    col.learnLine:SetText(string.format("Ideal : |cff88ff88%s|r (e%s)", d.best or "-", d.eps or "-"))
    -- "-" = celibataire ; "?" = marie mais le conjoint n'est pas apparu cote client.
    col.spouseLine:SetText(string.format("Conjoint : |cffff99cc%s|r", d.sp or "-"))

    setTargetMacro(col.targetBtn, d.n)
    col:Show()
end

-- Repeint TOUTES les colonnes depuis le cache (appele au throttle : les donnees
-- n'arrivent qu'1x/s, inutile de trier/repeindre a chaque paquet recu).
local function refreshColumns()
    if not main then return end

    local list = {}
    for id, d in pairs(members) do
        table.insert(list, { id = id, name = d.n or "?", data = d })
    end
    table.sort(list, function(x, y) return x.name < y.name end)

    for i, entry in ipairs(list) do
        paintColumn(getColumn(i), entry.data)
    end
    for i = #list + 1, #columns do
        columns[i]:Hide()
    end

    columnsChild:SetWidth(math.max(1, #list * COL_W))

    -- Le contenu a pu retrecir (PNJ disparus) : on re-borne le defilement courant, sinon
    -- on resterait bloque sur une zone vide, au-dela de la derniere colonne.
    local scroll = columnsChild:GetParent()
    local maxScroll = math.max(0, columnsChild:GetWidth() - scroll:GetWidth())
    if scroll:GetHorizontalScroll() > maxScroll then
        scroll:SetHorizontalScroll(maxScroll)
    end

    main.countLine:SetText(string.format("%d PNJ suivi%s   |cff808080(molette : defiler)|r",
        #list, #list > 1 and "s" or ""))
    main.emptyLine:SetShown(#list == 0)
end

-- ---------------------------------------------------------------------------
-- Fenetre
-- ---------------------------------------------------------------------------
local function createMainWindow()
    local f = CreateFrame("Frame", "ClanHUDMain", UIParent)
    -- Hauteur derivee de COL_H (88 d'en-tete + colonnes + 62 pour les boutons) : ainsi la
    -- fenetre suit automatiquement si l'on ajoute/retire une ligne dans les colonnes.
    f:SetSize(SCROLL_W + 28, COL_H + 150)
    if not ClanHUDDB.pos["main"] then
        f:SetPoint("CENTER", UIParent, "CENTER", 0, 0)
    end
    makeDraggable("main", f)
    backdrop(f, 0.85, 0.7, 0.2)

    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -10)
    title:SetText("|cffffd100Clan HUD|r")

    -- --- Bandeau "monde" (haut) ---
    f.timeLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    f.timeLine:SetPoint("TOPLEFT", 16, -34)
    f.timeLine:SetText("En attente du flux monde... (bouton \"Live monde\")")

    f.popLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.popLine:SetPoint("TOPLEFT", 16, -52)
    f.popLine:SetText("")

    f.countLine = f:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    f.countLine:SetPoint("TOPLEFT", 16, -70)
    f.countLine:SetText("0 PNJ suivi")

    -- --- Zone defilante HORIZONTALE contenant les colonnes ---
    -- ScrollFrame sans template : les templates standards sont verticaux. On pilote le
    -- defilement horizontal a la molette.
    local scroll = CreateFrame("ScrollFrame", "ClanHUDColumnsScroll", f)
    scroll:SetPoint("TOPLEFT", 14, -88)
    scroll:SetSize(SCROLL_W, COL_H)
    scroll:EnableMouseWheel(true)
    scroll:SetScript("OnMouseWheel", function(self, delta)
        local maxScroll = math.max(0, columnsChild:GetWidth() - self:GetWidth())
        local cur = self:GetHorizontalScroll()
        self:SetHorizontalScroll(math.min(maxScroll, math.max(0, cur - delta * COL_W)))
    end)

    columnsChild = CreateFrame("Frame", nil, scroll)
    columnsChild:SetSize(1, COL_H)
    scroll:SetScrollChild(columnsChild)

    f.emptyLine = f:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    f.emptyLine:SetPoint("TOPLEFT", 16, -100)
    f.emptyLine:SetText("Aucun PNJ suivi. Clique sur \"Tout suivre\" (ou .clan hudall).")

    -- --- Boutons (bas) ---
    local liveBtn = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    liveBtn:SetSize(150, 22)
    liveBtn:SetPoint("BOTTOMLEFT", 14, 12)
    liveBtn:SetText("Live monde")
    liveBtn:SetScript("OnClick", function() SendChatMessage(".clan world", "SAY") end)

    local allBtn = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    allBtn:SetSize(150, 22)
    allBtn:SetPoint("BOTTOMLEFT", 172, 12)
    allBtn:SetText("Tout suivre")
    allBtn:SetScript("OnClick", function() SendChatMessage(".clan hudall", "SAY") end)

    local hudBtn = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    hudBtn:SetSize(150, 22)
    hudBtn:SetPoint("BOTTOMLEFT", 330, 12)
    hudBtn:SetText("+ Suivre la cible")
    hudBtn:SetScript("OnClick", function() SendChatMessage(".clan hud", "SAY") end)

    local closeBtn = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    closeBtn:SetSize(100, 22)
    closeBtn:SetPoint("BOTTOMRIGHT", -14, 12)
    closeBtn:SetText("Fermer")
    closeBtn:SetScript("OnClick", function() f:Hide() end)

    main = f
    return f
end

local function updateWorld(d)
    if not main then return end

    local hour = tonumber(d.hour) or 0
    local night = (d.night == "1")
    main.timeLine:SetText(string.format("Heure : %02dh00  —  %s",
        hour, night and "|cff6699ffNuit|r" or "|cffffd100Jour|r"))
    -- Feux : allumes / total decouverts, colore selon qu'il reste des feux eteints a rallumer.
    local fl = tonumber(d.fl) or 0
    local ft = tonumber(d.ft) or 0
    local fireColor = (ft > 0 and fl < ft) and "ffff6060" or "ff40ff40"
    main.popLine:SetText(string.format(
        "Population : %s   (adultes %s | enfants %s | anciens %s)      Malades : %s      Feux : |c%s%d/%d|r",
        d.pop or "?", d.ad or "?", d.ch or "?", d.el or "?", d.sick or "0", fireColor, fl, ft))
end

-- ---------------------------------------------------------------------------
-- Driver (evenements + expiration)
-- ---------------------------------------------------------------------------
local driver = CreateFrame("Frame")
driver:RegisterEvent("PLAYER_LOGIN")
driver:RegisterEvent("CHAT_MSG_ADDON")
driver:SetScript("OnEvent", function(self, event, arg1, arg2)
    if event == "PLAYER_LOGIN" then
        if C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
            C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
        end
        createMainWindow()
        refreshColumns()

    elseif event == "CHAT_MSG_ADDON" and arg1 == ADDON_PREFIX and arg2 then
        local d = parse(arg2)
        if d.ep and d.epN then         -- epitaphe : clic sur une pierre tombale
            showEpitaph(d.ep, d.epN)
        elseif d.w then      -- resume monde
            updateWorld(d)
        elseif d.id then     -- donnees d'un PNJ (flux global "tbl=1" OU flux individuel)
            d._last = GetTime()
            members[d.id] = d
        end
    end
end)

-- Purge des PNJ qui ne recoivent plus de donnees (mort / flux coupe) puis repeinte.
local elapsed = 0
driver:SetScript("OnUpdate", function(self, dt)
    elapsed = elapsed + dt
    if elapsed < 0.5 then return end
    elapsed = 0

    local now = GetTime()
    for id, d in pairs(members) do
        if now - (d._last or 0) > 5 then
            members[id] = nil -- retrait pendant un pairs() : sans danger en Lua
        end
    end

    refreshColumns()
end)

-- ---------------------------------------------------------------------------
-- Commandes
-- ---------------------------------------------------------------------------
SLASH_CLANHUD1 = "/clanhud"
SlashCmdList["CLANHUD"] = function(msg)
    if msg and string.find(string.lower(msg), "debug") then
        ClanHUDDB.debug = not ClanHUDDB.debug
        if DEFAULT_CHAT_FRAME then
            DEFAULT_CHAT_FRAME:AddMessage("|cff88ccff[Clan HUD]|r debug : "
                .. (ClanHUDDB.debug and "|cff40ff40ON|r" or "|cffff6060OFF|r"))
        end
        return
    end

    if not main then createMainWindow() end
    if main:IsShown() then main:Hide() else main:Show() end
end
