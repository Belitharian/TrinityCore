--[[
    Clan HUD — supervision live des PNJ de clan + etat du monde.

    Cote serveur (GM) :
      - .clan world  : ouvre/alimente la fenetre MONDE (etat global, live).
      - .clan hud    : ouvre une fenetre pour le PNJ CIBLE (une par PNJ).
      - .clan monitor: flux texte dans le chat (sans addon).

    La fenetre MONDE a deux boutons :
      - "+ HUD (cible)" : lance .clan hud sur ta cible actuelle.
      - "Live monde"    : (re)active le flux .clan world.

    Toutes les fenetres sont deplacables ; leurs positions sont sauvegardees.
]]

local ADDON_PREFIX = "CLANHUD"

ClanHUDDB = ClanHUDDB or {}
ClanHUDDB.pos = ClanHUDDB.pos or {}

local memberWindows = {}
local newCount = 0

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
    bg:SetColorTexture(0, 0, 0, 0.7)
    local border = frame:CreateTexture(nil, "BACKGROUND", nil, -1)
    border:SetPoint("TOPLEFT", -1, 1)
    border:SetPoint("BOTTOMRIGHT", 1, -1)
    border:SetColorTexture(r, g, b, 0.85)
end

-- ---------------------------------------------------------------------------
-- Tableau global (flux "tout suivre" : une ligne par membre)
-- ---------------------------------------------------------------------------
local TABLE_WIDTH = 600
local ROW_H       = 16
local HEADER_Y    = -34

local tableWindow
local tableRows = {}     -- id -> ligne
local tableRowList = {}  -- ordre d'affichage

-- Couleur de vie : vert (100%) -> rouge (0%), en hexa "rrggbb".
local function hpColorHex(pct)
    pct = tonumber(pct) or 0
    local r = math.min(1, 2 - pct / 50)
    local g = math.min(1, pct / 50)
    return string.format("%02x%02x%02x", math.floor(r * 255), math.floor(g * 255), 26)
end

local function tableCol(parent, x, w, justify)
    local fs = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    fs:SetPoint("LEFT", parent, "LEFT", x, 0)
    fs:SetWidth(w)
    fs:SetJustifyH(justify or "LEFT")
    return fs
end

local function createTableWindow()
    local f = CreateFrame("Frame", "ClanHUDTable", UIParent)
    f:SetSize(TABLE_WIDTH, 90)
    if not ClanHUDDB.pos["table"] then
        f:SetPoint("CENTER", UIParent, "CENTER", 0, 0)
    end
    makeDraggable("table", f)
    backdrop(f, 0.4, 0.6, 0.35)

    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -8)
    title:SetText("|cff99ff99Clan — Tableau|r")

    local function header(x, w, text, justify)
        local fs = f:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
        fs:SetPoint("TOPLEFT", x, HEADER_Y)
        fs:SetWidth(w)
        fs:SetJustifyH(justify or "LEFT")
        fs:SetText(text)
    end
    header(10, 120, "Nom")
    header(135, 40, "Vie",    "CENTER")
    header(180, 42, "Faim",   "CENTER")
    header(225, 42, "Soif",   "CENTER")
    header(270, 46, "Fatig.", "CENTER")
    header(320, 42, "Repro",  "CENTER")
    header(370, 120, "Action")

    f.emptyLine = f:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    f.emptyLine:SetPoint("TOPLEFT", 12, HEADER_Y - ROW_H)
    f.emptyLine:SetText("En attente... (bouton \"Tableau\" ou .clan hudall)")

    tableWindow = f
    return f
end

local function layoutTable()
    local y = HEADER_Y - ROW_H
    local shown = 0
    for _, row in ipairs(tableRowList) do
        if row:IsShown() then
            row:ClearAllPoints()
            row:SetPoint("TOPLEFT", tableWindow, "TOPLEFT", 8, y)
            y = y - ROW_H
            shown = shown + 1
        end
    end
    tableWindow.emptyLine:SetShown(shown == 0)
    tableWindow:SetHeight(math.max(90, -HEADER_Y + ROW_H + shown * ROW_H + 12))
end

local function getTableRow(id)
    if tableRows[id] then return tableRows[id] end

    local row = CreateFrame("Frame", nil, tableWindow)
    row:SetSize(TABLE_WIDTH - 16, ROW_H)
    row.nameFS = tableCol(row, 10, 120)
    row.hpFS   = tableCol(row, 135, 40, "CENTER")
    row.huFS   = tableCol(row, 180, 42, "CENTER")
    row.thFS   = tableCol(row, 225, 42, "CENTER")
    row.enFS   = tableCol(row, 270, 46, "CENTER")
    row.reFS   = tableCol(row, 320, 42, "CENTER")
    row.actFS  = tableCol(row, 370, 120)

    row.id = id
    row.btn = CreateFrame("Button", nil, row, "UIPanelButtonTemplate")
    row.btn:SetSize(66, 14)
    row.btn:SetPoint("LEFT", row, "LEFT", 500, 0)
    row.btn:SetText("Cibler")
    row.btn:SetScript("OnClick", function()
        SendChatMessage(".clan target " .. row.id, "SAY")
    end)

    tableRows[id] = row
    table.insert(tableRowList, row)
    return row
end

local function updateTableRow(d)
    if not tableWindow then createTableWindow() end
    tableWindow:Show()

    local row = getTableRow(d.id)
    local wasShown = row:IsShown()
    row.nameFS:SetText(d.n or "?")
    row.hpFS:SetText(string.format("|cff%s%s%%|r", hpColorHex(d.hp), d.hp or "?"))
    row.huFS:SetText(d.hu or "?")
    row.thFS:SetText(d.th or "?")
    row.enFS:SetText(d.en or "?")
    row.reFS:SetText(d.re or "?")
    row.actFS:SetText(d.act or "-")
    row.lastUpdate = GetTime()
    row:Show()
    if not wasShown then layoutTable() end
end

-- Ouvre le tableau et (re)active le flux serveur "tout suivre".
local function openTable()
    if not tableWindow then createTableWindow() end
    tableWindow:Show()
    SendChatMessage(".clan hudall", "SAY")
end

-- ---------------------------------------------------------------------------
-- Fenetre MONDE (principale)
-- ---------------------------------------------------------------------------
local world
local function createWorldWindow()
    local f = CreateFrame("Frame", "ClanHUDWorld", UIParent)
    f:SetSize(270, 178)
    if not ClanHUDDB.pos["world"] then
        f:SetPoint("TOP", UIParent, "TOP", 0, -120)
    end
    makeDraggable("world", f)
    backdrop(f, 0.85, 0.7, 0.2)

    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -8)
    title:SetText("|cffffd100Clan — Monde|r")

    f.timeLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    f.timeLine:SetPoint("TOP", title, "BOTTOM", 0, -6)
    f.timeLine:SetText("En attente... (tape .clan world)")

    f.popLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.popLine:SetPoint("TOPLEFT", 14, -56)
    f.popLine:SetText("")

    f.sickLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.sickLine:SetPoint("TOPLEFT", 14, -74)
    f.sickLine:SetText("")

    -- Bouton : ajouter un HUD depuis la cible.
    local addBtn = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    addBtn:SetSize(120, 22)
    addBtn:SetPoint("BOTTOMLEFT", 12, 38)
    addBtn:SetText("+ HUD (cible)")
    addBtn:SetScript("OnClick", function()
        -- Lance la commande serveur sur la cible actuelle.
        SendChatMessage(".clan hud", "SAY")
    end)

    -- Bouton : (re)activer le flux monde.
    local liveBtn = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    liveBtn:SetSize(120, 22)
    liveBtn:SetPoint("BOTTOMRIGHT", -12, 38)
    liveBtn:SetText("Live monde")
    liveBtn:SetScript("OnClick", function()
        SendChatMessage(".clan world", "SAY")
    end)

    -- Bouton : ouvrir le tableau global et suivre TOUT le monde.
    local tableBtn = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    tableBtn:SetSize(246, 22)
    tableBtn:SetPoint("BOTTOM", 0, 10)
    tableBtn:SetText("Tableau (tout suivre)")
    tableBtn:SetScript("OnClick", openTable)

    world = f
    return f
end

local function updateWorld(d)
    if not world then createWorldWindow() end
    local hour = tonumber(d.hour) or 0
    local night = (d.night == "1")
    world.timeLine:SetText(string.format("Heure : %02dh00  —  %s",
        hour, night and "|cff6699ffNuit|r" or "|cffffd100Jour|r"))
    world.popLine:SetText(string.format("Population : %s   (adultes %s | enfants %s | anciens %s)",
        d.pop or "?", d.ad or "?", d.ch or "?", d.el or "?"))
    world.sickLine:SetText(string.format("Malades : %s", d.sick or "0"))
    world.frameLastUpdate = GetTime()
end

-- ---------------------------------------------------------------------------
-- Fenetres PNJ (une par membre)
-- ---------------------------------------------------------------------------
local function CreateBar(parent, labelText, y, r, g, b)
    local label = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    label:SetPoint("TOPLEFT", 12, y)
    label:SetWidth(60)
    label:SetJustifyH("LEFT")
    label:SetText(labelText)

    local bar = CreateFrame("StatusBar", nil, parent)
    bar:SetSize(170, 14)
    bar:SetPoint("TOPLEFT", 78, y)
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

local function CreateMemberWindow(id)
    newCount = newCount + 1
    local f = CreateFrame("Frame", "ClanHUDMember_" .. id, UIParent)
    f:SetSize(260, 262)
    if not ClanHUDDB.pos[id] then
        f:SetPoint("CENTER", UIParent, "CENTER", (newCount - 1) * 24, -(newCount - 1) * 24)
    end
    makeDraggable(id, f)
    backdrop(f, 0.3, 0.5, 0.7)

    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -8)
    title:SetText("|cff88ccffClan HUD|r")

    local sub = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    sub:SetPoint("TOP", title, "BOTTOM", 0, -3)
    sub:SetText("...")

    local win = { frame = f, sub = sub, lastUpdate = 0 }
    win.barHunger, win.valHunger = CreateBar(f, "Faim",    -46, 0.85, 0.55, 0.20)
    win.barThirst, win.valThirst = CreateBar(f, "Soif",    -64, 0.25, 0.55, 0.90)
    win.barEnergy, win.valEnergy = CreateBar(f, "Fatigue", -82, 0.60, 0.40, 0.80)
    win.barRepro,  win.valRepro  = CreateBar(f, "Repro",  -100, 0.85, 0.45, 0.65)

    win.disLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    win.disLine:SetPoint("TOPLEFT", 12, -124)
    win.disLine:SetPoint("TOPRIGHT", -12, -124)
    win.disLine:SetJustifyH("LEFT")

    win.invLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    win.invLine:SetPoint("TOPLEFT", 12, -146)
    win.invLine:SetPoint("TOPRIGHT", -12, -146)
    win.invLine:SetJustifyH("LEFT")

    win.actLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    win.actLine:SetPoint("TOPLEFT", 12, -168)
    win.actLine:SetPoint("TOPRIGHT", -12, -168)
    win.actLine:SetJustifyH("LEFT")

    win.learnLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    win.learnLine:SetPoint("TOPLEFT", 12, -190)
    win.learnLine:SetPoint("TOPRIGHT", -12, -190)
    win.learnLine:SetJustifyH("LEFT")

    -- Bouton "Cibler" : demande au serveur de cibler CE PNJ precisement, par son id
    -- (compteur de GUID) via ".clan target <id>". Precis meme si les noms se repetent.
    local targetBtn = CreateFrame("Button", "ClanHUDTarget_" .. id, f, "UIPanelButtonTemplate")
    targetBtn:SetSize(120, 22)
    targetBtn:SetPoint("BOTTOM", 0, 10)
    targetBtn:SetText("Cibler")
    targetBtn:SetScript("OnClick", function()
        SendChatMessage(".clan target " .. id, "SAY")
    end)
    win.targetBtn = targetBtn

    memberWindows[id] = win
    return win
end

local function getMemberWindow(id)
    return memberWindows[id] or CreateMemberWindow(id)
end

local function updateMember(d)
    -- Premier contact avec cet id : on ouvrira sa fenetre au premier plan (voir plus bas).
    local isNew = (memberWindows[d.id] == nil)
    local win = getMemberWindow(d.id)
    local hu = tonumber(d.hu) or 0
    local th = tonumber(d.th) or 0
    local en = tonumber(d.en) or 0
    local re = tonumber(d.re) or 0

    win.barHunger:SetValue(hu); win.valHunger:SetText(string.format("%d", hu))
    win.barThirst:SetValue(th); win.valThirst:SetText(string.format("%d", th))
    win.barEnergy:SetValue(en); win.valEnergy:SetText(string.format("%d", en))
    win.barRepro:SetValue(re);  win.valRepro:SetText(string.format("%d", re))

    win.sub:SetText(string.format("|cffffd100%s|r  —  clan %s | %s | %s | %sj | Vie %s%%",
        d.n or "?", d.cl or "?", d.ge or "?", d.st or "?", d.ag or "?", d.hp or "?"))
    win.disLine:SetText("Etat : " .. afflictionText(d.dis))
    win.invLine:SetText(string.format("Viande %s  Bois %s  Pierre %s  Feu %s",
        yn(d.raw), yn(d.wo), yn(d.sto), yn(d.fi)))
    win.actLine:SetText(string.format("Action : |cffffffff%s|r", d.act or "-"))
    win.learnLine:SetText(string.format("Ideal : |cff88ff88%s|r   (epsilon %s)",
        d.best or "-", d.eps or "-"))

    win.lastUpdate = GetTime()
    win.frame:Show()

    -- Nouveau PNJ repere : on met sa fenetre au premier plan et on le signale.
    if isNew then
        win.frame:Raise()
        if DEFAULT_CHAT_FRAME then
            DEFAULT_CHAT_FRAME:AddMessage(string.format(
                "|cff88ccff[Clan HUD]|r Nouveau PNJ repere : |cffffd100%s|r", d.n or "?"))
        end
    end
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
        createWorldWindow() -- la fenetre monde est toujours presente
    elseif event == "CHAT_MSG_ADDON" and arg1 == ADDON_PREFIX and arg2 then
        local d = parse(arg2)
        if d.tbl then          -- flux "tout suivre" -> ligne de tableau (contient aussi d.id)
            updateTableRow(d)
        elseif d.w then        -- resume monde
            updateWorld(d)
        elseif d.id then       -- fenetre PNJ individuelle (.clan hud)
            updateMember(d)
        end
    end
end)

-- Ferme les fenetres PNJ / lignes de tableau qui ne recoivent plus de donnees.
driver:SetScript("OnUpdate", function()
    local now = GetTime()
    for _, win in pairs(memberWindows) do
        if win.frame:IsShown() and win.lastUpdate > 0 and (now - win.lastUpdate) > 3 then
            win.frame:Hide()
        end
    end

    -- Tableau : masque les lignes obsoletes (membre mort / flux coupe) et re-agence.
    if tableWindow then
        local changed = false
        for _, row in ipairs(tableRowList) do
            if row:IsShown() and row.lastUpdate and (now - row.lastUpdate) > 4 then
                row:Hide()
                changed = true
            end
        end
        if changed then layoutTable() end
    end
end)

-- /clanhud : affiche/masque la fenetre monde.
SLASH_CLANHUD1 = "/clanhud"
SlashCmdList["CLANHUD"] = function()
    if not world then createWorldWindow() end
    if world:IsShown() then world:Hide() else world:Show() end
end
