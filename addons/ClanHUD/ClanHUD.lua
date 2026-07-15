--[[
    Clan HUD — affiche en temps reel les besoins des PNJ de clan.

    Fonctionnement :
      - Cote serveur, selectionne un membre de clan et tape ".clan hud".
      - Le serveur envoie 1x/s un message addon (prefixe "CLANHUD") avec les donnees.
      - Cet addon ouvre UNE FENETRE PAR PNJ suivi et la met a jour.
      - Re-tape ".clan hud" sur un PNJ pour arreter : sa fenetre se ferme toute seule
        (apres ~3 s sans donnees). Tu peux suivre plusieurs PNJ en meme temps.

    Chaque fenetre est deplacable (clic gauche) ; sa position est sauvegardee.
    /clanhud masque/affiche toutes les fenetres.
]]

local ADDON_PREFIX = "CLANHUD"

ClanHUDDB = ClanHUDDB or {}
ClanHUDDB.pos = ClanHUDDB.pos or {}

local windows = {}       -- id -> fenetre
local newCount = 0       -- pour decaler les nouvelles fenetres
local masterHidden = false

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

-- ---------------------------------------------------------------------------
-- Fabrique de fenetre (une par PNJ)
-- ---------------------------------------------------------------------------
local function CreateWindow(id)
    newCount = newCount + 1

    local f = CreateFrame("Frame", "ClanHUDFrame_" .. id, UIParent)
    f:SetSize(260, 210)
    f:SetFrameStrata("MEDIUM")

    local saved = ClanHUDDB.pos[id]
    if saved then
        f:SetPoint(saved.point, UIParent, saved.relPoint, saved.x, saved.y)
    else
        f:SetPoint("CENTER", UIParent, "CENTER", (newCount - 1) * 24, -(newCount - 1) * 24)
    end

    f:SetMovable(true)
    f:EnableMouse(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", f.StartMoving)
    f:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        local point, _, relPoint, x, y = self:GetPoint()
        ClanHUDDB.pos[id] = { point = point, relPoint = relPoint, x = x, y = y }
    end)

    local bg = f:CreateTexture(nil, "BACKGROUND")
    bg:SetAllPoints()
    bg:SetColorTexture(0, 0, 0, 0.65)

    local border = f:CreateTexture(nil, "BACKGROUND", nil, -1)
    border:SetPoint("TOPLEFT", -1, 1)
    border:SetPoint("BOTTOMRIGHT", 1, -1)
    border:SetColorTexture(0.3, 0.5, 0.7, 0.8)

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

    win.invLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    win.invLine:SetPoint("TOPLEFT", 12, -124)
    win.invLine:SetPoint("TOPRIGHT", -12, -124)
    win.invLine:SetJustifyH("LEFT")

    win.actLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    win.actLine:SetPoint("TOPLEFT", 12, -142)
    win.actLine:SetPoint("TOPRIGHT", -12, -142)
    win.actLine:SetJustifyH("LEFT")

    win.learnLine = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    win.learnLine:SetPoint("TOPLEFT", 12, -160)
    win.learnLine:SetPoint("TOPRIGHT", -12, -160)
    win.learnLine:SetJustifyH("LEFT")

    windows[id] = win
    return win
end

local function GetWindow(id)
    return windows[id] or CreateWindow(id)
end

local function UpdateWindow(win, d)
    local hu = tonumber(d.hu) or 0
    local th = tonumber(d.th) or 0
    local en = tonumber(d.en) or 0
    local re = tonumber(d.re) or 0

    win.barHunger:SetValue(hu); win.valHunger:SetText(string.format("%d", hu))
    win.barThirst:SetValue(th); win.valThirst:SetText(string.format("%d", th))
    win.barEnergy:SetValue(en); win.valEnergy:SetText(string.format("%d", en))
    win.barRepro:SetValue(re);  win.valRepro:SetText(string.format("%d", re))

    win.sub:SetText(string.format("|cffffd100%s|r  —  clan %s | %s | %s | %sj",
        d.n or "?", d.cl or "?", d.ge or "?", d.st or "?", d.ag or "?"))
    win.invLine:SetText(string.format("Viande %s  Bois %s  Pierre %s  Feu %s",
        yn(d.raw), yn(d.wo), yn(d.sto), yn(d.fi)))
    win.actLine:SetText(string.format("Action : |cffffffff%s|r", d.act or "-"))
    win.learnLine:SetText(string.format("Ideal : |cff88ff88%s|r   (epsilon %s)",
        d.best or "-", d.eps or "-"))

    win.lastUpdate = GetTime()
end

-- ---------------------------------------------------------------------------
-- Driver (evenements + expiration des fenetres)
-- ---------------------------------------------------------------------------
local driver = CreateFrame("Frame")
driver:RegisterEvent("PLAYER_LOGIN")
driver:RegisterEvent("CHAT_MSG_ADDON")
driver:SetScript("OnEvent", function(self, event, arg1, arg2)
    if event == "PLAYER_LOGIN" then
        if C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
            C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
        end
    elseif event == "CHAT_MSG_ADDON" and arg1 == ADDON_PREFIX and arg2 then
        local d = parse(arg2)
        if d.id then
            local win = GetWindow(d.id)
            UpdateWindow(win, d)
            if not masterHidden then
                win.frame:Show()
            end
        end
    end
end)

-- Ferme les fenetres qui ne recoivent plus de donnees (.clan hud coupe cote serveur).
driver:SetScript("OnUpdate", function()
    local now = GetTime()
    for _, win in pairs(windows) do
        if win.frame:IsShown() and win.lastUpdate > 0 and (now - win.lastUpdate) > 3 then
            win.frame:Hide()
        end
    end
end)

-- /clanhud : masque/affiche toutes les fenetres actives.
SLASH_CLANHUD1 = "/clanhud"
SlashCmdList["CLANHUD"] = function()
    masterHidden = not masterHidden
    for _, win in pairs(windows) do
        if masterHidden then
            win.frame:Hide()
        elseif win.lastUpdate > 0 and (GetTime() - win.lastUpdate) <= 3 then
            win.frame:Show()
        end
    end
    print("ClanHUD : " .. (masterHidden and "masque" or "affiche"))
end
