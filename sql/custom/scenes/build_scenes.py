"""Genere le SQL des scenes de dialogue de La Chute de Theramore.

Les scenes sont ecrites en Lua (framework.lua + battle_for_theramore/*.lua)
avec des macros resolues ici, pour que positions, chemins, repliques et
visuels n'aient qu'une source :

    NPC(NPC_X)            entry          (battle_for_theramore.h)
    SPELL(SPELL_X)        SpellID        (header et InstanceScript)
    CONST(NOM)            toute autre constante du header (SOUND_X...)
    SV(SPELL_X | 12345)   SpellVisualID  (SpellXSpellVisual, cache EmeraldTools)
    ANIM(EMOTE_X)         AnimID         (SharedDefines.h -> Emotes.db2)
    POS(Nom)              x, y, z, o     (Position const Nom)
    PATH(Nom)             { speed = WALK|RUN, { x, y, z, o }, ... }
    PATHSTART(Nom)        x, y, z, o du premier noeud
    PATHEND(Nom[, o])     x, y, z, o du dernier noeud (orientation forcee)
    LOC(table, CLE)       destination de la ligne CLE d'une table Location
    LOCFROM(table, CLE)   position de depart de cette ligne
    SPAWN(guid)           x, y, z, o du spawn en base (world.creature)
    T(NPC_X, SAY_Y)       text, kind, anim, duration : un BroadcastText cree
                          a partir de creature_text (texte, son, emote, duree)

Sorties :
    hotfixes/<date>_hotfixes_theramore_dialogue_scenes.sql
    world/<date>_world_theramore_dialogue_scenes.sql

Usage : python -I build_scenes.py [--date 2026_10_28] [--force]
Relit la base a chaque passage : a relancer apres une modification des
repliques (texte, son, duree) ou des positions.

Les Lua sont la seule source : le SQL genere remplace toutes les scenes, et
le worldserver le rejoue au demarrage des que son contenu change
(Updates.Redundancy). Une scene retouchee directement en base serait donc
perdue. Avant d'ecrire, le generateur compare scene_script_text en base au
dernier SQL qu'il a produit (last_build.json) et s'arrete si un script a
ete modifie a la main : reporter la modification dans le Lua, ou relancer
avec --force pour l'abandonner.
"""

import argparse
import csv
import datetime
import difflib
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wdc5  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..'))   # C:\Projets\Emerald
SOURCES = os.path.join(ROOT, '_Sources')
SCRIPTS = os.path.join(SOURCES, 'src', 'server', 'scripts', 'Custom', 'TheramoreFall')
HEADER = os.path.join(SCRIPTS, 'battle_for_theramore.h')
INSTANCE = os.path.join(SCRIPTS, 'scenario_battle_for_theramore.cpp')
SHARED_DEFINES = os.path.join(SOURCES, 'src', 'server', 'game', 'Miscellaneous', 'SharedDefines.h')
EMOTES_DB2 = os.path.join(ROOT, '_Core', 'dbc', 'frFR', 'Emotes.db2')
VISUAL_CACHE = os.path.join(ROOT, '_Tools', 'EmeraldTools', 'resources', 'SpellVisualCache')
WORLDSERVER_CONF = os.path.join(ROOT, '_Core', 'worldserver.conf')
MYSQL = r'C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe'

SCENE_DIR = os.path.join(HERE, 'battle_for_theramore')
LAST_BUILD = os.path.join(HERE, 'last_build.json')  # scene_script_text du dernier SQL genere
FRAMEWORK = os.path.join(HERE, 'framework.lua')

# Plages d'identifiants reservees aux scenes de dialogue. L'introduction
# (package 50000, scripts et membres 500000-500001, BroadcastText 850082 a
# 850086) reste en dehors.
PACKAGE_FIRST = 50001
MEMBER_FIRST, MEMBER_LAST = 500010, 500099
SCRIPT_FIRST, SCRIPT_LAST = 500100, 500999
BROADCAST_FIRST, BROADCAST_LAST = 851000, 851999
CINEMATIC_FRAMEWORK_PACKAGE = 156
SCRIPT_MAX_CHARS = 3800             # texte d'un morceau de SceneScriptText
SCENE_SCRIPT_NAME = 'scene_bft_dialogue'
SCENE_FLAGS = 0                     # dans le monde : ni camera ni phase

HASH_BROADCAST_TEXT = 35137211
HASH_SCENE_SCRIPT = 3568395212
HASH_SCENE_SCRIPT_TEXT = 537670055
HASH_SCENE_SCRIPT_PACKAGE = 3905641993
HASH_SCENE_SCRIPT_MEMBER = 3830298396

# creature_text.Type -> BroadcastType du client
CHAT_KIND = {12: 'Say', 14: 'Yell', 15: 'Whisper', 16: 'Emote', 41: 'Emote', 42: 'Whisper'}


class BuildError(Exception):
    pass


# --------------------------------------------------------------------------
# Lecture des sources C++
# --------------------------------------------------------------------------

def read(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return f.read()


def strip_comments(code):
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)
    return re.sub(r'//[^\n]*', '', code)


NUM = r'(-?\d+(?:\.\d*)?)f?'           # 3.5f, 0.f, -12


def parse_constants(*paths):
    constants = {}
    for path in paths:
        for name, value in re.findall(r'\b([A-Z][A-Z0-9_]+)\s*=\s*(-?\d+)\b', strip_comments(read(path))):
            constants.setdefault(name, int(value))
    return constants


def parse_positions(code):
    positions = {}
    pattern = r'Position const (\w+)\s*=\s*\{\s*%s\s*,\s*%s\s*,\s*%s\s*,\s*%s\s*\}' % (NUM, NUM, NUM, NUM)
    for name, *values in re.findall(pattern, code):
        positions[name] = [float(v) for v in values]
    return positions


def parse_paths(code):
    paths = {}
    node = r'\{\s*\d+\s*,\s*%s\s*,\s*%s\s*,\s*%s\s*(?:,\s*%s\s*)?\}' % (NUM, NUM, NUM, NUM)
    for name, body, move in re.findall(
            r'WaypointPath const (\w+)\s*=?\s*\{\s*\d+\s*,\s*\{(.*?)\}\s*,\s*WaypointMoveType::(\w+)', code, re.S):
        nodes = []
        for x, y, z, o in re.findall(node, body):
            nodes.append([float(x), float(y), float(z), float(o) if o else None])
        paths[name] = {'speed': 'RUN' if move == 'Run' else 'WALK', 'nodes': nodes}
    return paths


def parse_locations(code):
    tables = {}
    vec = r'\{\s*%s\s*,\s*%s\s*,\s*%s\s*,\s*%s\s*\}' % (NUM, NUM, NUM, NUM)
    for name, body in re.findall(r'Location const (\w+)\[\w+\]\s*=\s*\{(.*?)\n\};', code, re.S):
        rows = []
        for key, *values in re.findall(r'\{\s*(\w+)\s*,\s*%s\s*,\s*%s' % (vec, vec), body):
            values = [float(v) for v in values]
            rows.append({'key': key, 'position': values[:4], 'destination': values[4:]})
        tables[name] = rows
    return tables


# --------------------------------------------------------------------------
# Donnees du client et de la base
# --------------------------------------------------------------------------

def load_visuals():
    builds = sorted(d for d in os.listdir(VISUAL_CACHE) if os.path.isdir(os.path.join(VISUAL_CACHE, d)))
    if not builds:
        raise BuildError('cache SpellXSpellVisual absent : lancer spell_visuals_check dans EmeraldTools')
    path = os.path.join(VISUAL_CACHE, builds[-1], 'SpellXSpellVisual.csv')
    visuals = {}
    with open(path, encoding='utf-8') as f:
        for row in csv.DictReader(f):
            if row['DifficultyID'] != '0':
                continue
            spell = int(row['SpellID'])
            conditioned = any(row[c] != '0' for c in (
                'ViewerUnitConditionID', 'ViewerPlayerConditionID', 'CasterUnitConditionID', 'CasterPlayerConditionID'))
            rank = (conditioned, -int(row['Priority']), int(row['ID']))
            current = visuals.get(spell)
            if current is None or rank < current[0]:
                visuals[spell] = (rank, int(row['SpellVisualID']))
    return {spell: v[1] for spell, v in visuals.items()}


def load_emote_anims():
    rows = wdc5.read_wdc5(EMOTES_DB2)
    return {emote: values[1] for emote, values in rows.items()}   # EmotesEntry::AnimID


class Database:
    def __init__(self):
        conf = read(WORLDSERVER_CONF)
        line = re.search(r'^WorldDatabaseInfo\s*=\s*"([^"]*)"', conf, re.M).group(1)
        host, port, user, password, self.world = line.split(';')
        self.hotfixes = re.search(r'^HotfixDatabaseInfo\s*=\s*"([^"]*)"', conf, re.M).group(1).split(';')[4]
        fd, self.defaults = tempfile.mkstemp(suffix='.cnf')
        with os.fdopen(fd, 'w') as f:
            f.write('[client]\nhost=%s\nport=%s\nuser=%s\npassword=%s\ndefault-character-set=utf8mb4\n'
                    % (host, port, user, password))

    def close(self):
        os.remove(self.defaults)

    def query(self, sql):
        out = subprocess.run([MYSQL, '--defaults-extra-file=' + self.defaults, '--batch', '--skip-column-names',
                              self.world, '-e', sql], capture_output=True, check=True).stdout.decode('utf-8')
        rows = []
        for line in out.splitlines():
            rows.append([unescape(v) for v in line.split('\t')])
        return rows


def unescape(value):
    if value == 'NULL':
        return None
    return re.sub(r'\\(.)', lambda m: {'n': '\n', 't': '\t', '0': '\0'}.get(m.group(1), m.group(1)), value)


# --------------------------------------------------------------------------
# Macros
# --------------------------------------------------------------------------

def fmt(value):
    if value is None:
        return 'nil'
    text = ('%.6f' % value).rstrip('0').rstrip('.')
    return text if text not in ('-0', '') else '0'


class Resolver:
    def __init__(self, db):
        header = strip_comments(read(HEADER))
        self.constants = parse_constants(HEADER, INSTANCE)
        self.emotes = parse_constants(SHARED_DEFINES)
        self.positions = parse_positions(header)
        self.paths = parse_paths(header)
        self.locations = parse_locations(header)
        self.visuals = load_visuals()
        self.anims = load_emote_anims()
        self.db = db
        self.texts = {}            # (entry, group) -> BroadcastText
        self.next_broadcast = BROADCAST_FIRST
        self.spawns = {}
        self.equipment = {}

    def const(self, name):
        name = name.strip()
        if re.fullmatch(r'-?\d+', name):
            return int(name)
        if name not in self.constants:
            raise BuildError('constante inconnue : %s' % name)
        return self.constants[name]

    def position(self, name):
        if name not in self.positions:
            raise BuildError('Position inconnue : %s' % name)
        return self.positions[name]

    def path(self, name):
        if name not in self.paths:
            raise BuildError('WaypointPath inconnu : %s' % name)
        return self.paths[name]

    def location(self, table, key):
        if table not in self.locations:
            raise BuildError('table Location inconnue : %s' % table)
        rows = self.locations[table]
        if re.fullmatch(r'\d+', key):
            return rows[int(key)]
        for row in rows:
            if row['key'] == key:
                return row
        raise BuildError('%s ne contient pas %s' % (table, key))

    def spawn(self, guid):
        if guid not in self.spawns:
            rows = self.db.query('SELECT position_x, position_y, position_z, orientation FROM creature WHERE guid = %d' % guid)
            if not rows:
                raise BuildError('spawn %d absent de world.creature' % guid)
            self.spawns[guid] = [float(v) for v in rows[0]]
        return self.spawns[guid]

    def visual(self, spell):
        spell = self.const(spell)
        if spell not in self.visuals:
            raise BuildError('aucun SpellXSpellVisual pour le sort %d' % spell)
        return self.visuals[spell]

    def anim(self, emote):
        emote = emote.strip()
        if emote not in self.emotes:
            raise BuildError('emote inconnue : %s' % emote)
        value = self.emotes[emote]
        if value not in self.anims:
            raise BuildError('emote %s (%d) absente de Emotes.db2' % (emote, value))
        return self.anims[value]

    def text(self, entry, group):
        key = (self.const(entry), self.const(group))
        if key not in self.texts:
            rows = self.db.query('SELECT Type, Emote, Sound, Duration, Text FROM creature_text '
                                 'WHERE CreatureID = %d AND GroupID = %d ORDER BY ID LIMIT 1' % key)
            if not rows:
                raise BuildError('creature_text %d groupe %d absent' % key)
            chat, emote, sound, duration, text = rows[0]
            chat = int(chat)
            if chat not in CHAT_KIND:
                raise BuildError('creature_text %d groupe %d : type %d non gere' % (key + (chat,)))
            if self.next_broadcast > BROADCAST_LAST:
                raise BuildError('plage BroadcastText epuisee')
            self.texts[key] = {
                'id': self.next_broadcast, 'kind': CHAT_KIND[chat], 'sound': int(sound or 0),
                'anim': self.anims.get(int(emote or 0), 0) if int(emote or 0) else 0,
                'duration': int(duration or 0) / 1000.0, 'text': text, 'entry': key[0], 'group': key[1]}
            self.next_broadcast += 1
        return self.texts[key]

    def actor(self, entry):
        """Apparence, echelle, nom (frFR) et armes du PNJ, que le client ne
        connait pas pour une creature du serveur."""
        if entry not in self.equipment:
            model = self.db.query('SELECT CreatureDisplayID, DisplayScale FROM creature_template_model '
                                  'WHERE CreatureID = %d ORDER BY Idx LIMIT 1' % entry)
            name = self.db.query("SELECT IFNULL(l.Name, t.name) FROM creature_template t "
                                 "LEFT JOIN creature_template_locale l ON l.entry = t.entry AND l.locale = 'frFR' "
                                 "WHERE t.entry = %d" % entry)
            equip = self.db.query('SELECT ItemID1, ItemID2, ItemID3 FROM creature_equip_template '
                                  'WHERE CreatureID = %d ORDER BY ID LIMIT 1' % entry)
            if not model:
                raise BuildError('creature %d sans creature_template_model' % entry)
            self.equipment[entry] = {
                'display': int(model[0][0]), 'scale': float(model[0][1] or 1),
                'name': name[0][0] if name else None,
                'equip': [int(v) for v in equip[0]] if equip else None}
        return self.equipment[entry]

    # -- expansion --------------------------------------------------------

    def expand(self, code, where):
        def args(raw):
            return [a.strip() for a in raw.split(',')] if raw.strip() else []

        def xyzo(values):
            return ', '.join(fmt(v) for v in values)

        def path_table(path):
            nodes = ', '.join('{ %s }' % xyzo(n) for n in path['nodes'])
            return '{ speed = %s, %s }' % (path['speed'], nodes)

        handlers = {
            'NPC': lambda a: str(self.const(a[0])),
            'SPELL': lambda a: str(self.const(a[0])),
            'CONST': lambda a: str(self.const(a[0])),
            'SV': lambda a: '%d --[[ %s ]]' % (self.visual(a[0]), a[0]),
            'ANIM': lambda a: '%d --[[ %s ]]' % (self.anim(a[0]), a[0]),
            'POS': lambda a: xyzo(self.position(a[0])),
            'PATH': lambda a: path_table(self.path(a[0])),
            'PATHSTART': lambda a: xyzo(self.path(a[0])['nodes'][0][:3] + [self.path(a[0])['nodes'][0][3] or 0]),
            'PATHEND': lambda a: xyzo(self.path(a[0])['nodes'][-1][:3]
                                      + [float(a[1]) if len(a) > 1 else (self.path(a[0])['nodes'][-1][3] or 0)]),
            'LOC': lambda a: xyzo(self.location(a[0], a[1])['destination']),
            'LOCFROM': lambda a: xyzo(self.location(a[0], a[1])['position']),
            'SPAWN': lambda a: xyzo(self.spawn(int(a[0]))),
            'T': lambda a: self.expand_text(a),
        }
        pattern = re.compile(r'\b(%s)\(([^()]*)\)' % '|'.join(handlers))

        def replace(match):
            try:
                return handlers[match.group(1)](args(match.group(2)))
            except BuildError as error:
                raise BuildError('%s : %s' % (where, error))

        return pattern.sub(replace, code)

    # AnimationData : EmoteTalk, EmoteTalkExclamation, EmoteTalkQuestion
    ANIM_TALK, ANIM_TALK_EXCLAMATION, ANIM_TALK_QUESTION = 60, 64, 65

    def expand_text(self, a):
        text = self.text(a[0], a[1])
        anim = text['anim']
        # Une question ou une exclamation sans emote propre prend
        # EmoteTalkQuestion / EmoteTalkExclamation : Say la joue pour une
        # replique courte (les longues restent en TalkSubdued).
        ending = text['text'].rstrip()[-1:]
        if anim in (0, self.ANIM_TALK):
            if ending == '?':
                anim = self.ANIM_TALK_QUESTION
            elif ending == '!':
                anim = self.ANIM_TALK_EXCLAMATION
        return '%d, BroadcastType.%s, %d, %s --[[ %s %s ]]' % (
            text['id'], text['kind'], anim, fmt(text['duration']), a[0], a[1])


# --------------------------------------------------------------------------
# Decoupage en morceaux de SceneScriptText
# --------------------------------------------------------------------------

OPENERS = re.compile(r'\b(function|if|do|repeat)\b|[({\[]')
CLOSERS = re.compile(r'\b(end|until)\b|[)}\]]')


def lua_depth_delta(line):
    line = re.sub(r'"(?:\\.|[^"\\])*"', '""', line)
    line = re.sub(r'--.*$', '', re.sub(r'--\[\[.*?\]\]', '', line))
    # "while x do" et "for x do" n'ouvrent qu'un bloc : seul le "do" compte.
    return len(OPENERS.findall(line)) - len(CLOSERS.findall(line))


def split_chunks(code, where):
    statements, current, depth = [], [], 0
    for line in code.split('\n'):
        current.append(line)
        depth += lua_depth_delta(line)
        if depth < 0:
            raise BuildError('%s : bloc ferme en trop pres de "%s"' % (where, line.strip()))
        if depth == 0 and (not line.strip() or not line.startswith(('\t', ' '))):
            statements.append('\n'.join(current))
            current = []
    if depth != 0:
        raise BuildError('%s : bloc non ferme' % where)
    if current:
        statements.append('\n'.join(current))

    chunks, chunk = [], ''
    for statement in statements:
        piece = statement + '\n'
        if len(piece) > SCRIPT_MAX_CHARS:
            raise BuildError('%s : instruction de %d caracteres, trop longue pour un morceau' % (where, len(piece)))
        if len(chunk) + len(piece) > SCRIPT_MAX_CHARS:
            chunks.append(chunk)
            chunk = ''
        chunk += piece
    if chunk.strip():
        chunks.append(chunk)
    return [c.strip('\n') + '\n' for c in chunks]


# --------------------------------------------------------------------------
# Scenes
# --------------------------------------------------------------------------

def load_scene(path):
    code = read(path)
    meta = dict(re.findall(r'^--! (\w+) (.+)$', code, re.M))
    for field in ('scene', 'name'):
        if field not in meta:
            raise BuildError('%s : en-tete --! %s manquant' % (path, field))
    return {'file': os.path.basename(path), 'scene': int(meta['scene']), 'name': meta['name'].strip(), 'code': code}


def lua_string(text):
    return '"%s"' % text.replace('\\', '\\\\').replace('"', '\\"')


def actor_table(resolver, code):
    """Table ACTORS lue par Spawn (framework.lua) : une entree par PNJ de la scene."""
    entries = sorted({int(e) for e in re.findall(r'\bSpawn\(\s*"[^"]+"\s*,\s*(\d+)', code)}
                     | {int(e) for e in re.findall(r'\b(?:Archmage|General)\(\s*"[^"]+"\s*,\s*(\d+)', code)})
    rows = []
    for entry in entries:
        info = resolver.actor(entry)
        fields = ['display = %d' % info['display'], 'scale = %s' % fmt(info['scale'])]
        if info['name']:
            fields.append('name = %s' % lua_string(info['name']))
        if info['equip'] and any(info['equip']):
            fields.append('equip = { %d, %d, %d }' % tuple(info['equip']))
        rows.append('\t[%d] = { %s },' % (entry, ', '.join(fields)))
    if not rows:
        return ''
    return ('-- Apparence, nom et armes des acteurs (creature_template_model,\n'
            '-- creature_template_locale, creature_equip_template).\nACTORS =\n{\n%s\n}\n\n' % '\n'.join(rows))


def sql_string(text):
    return "'" + text.replace('\\', '\\\\').replace("'", "''") + "'"


def blob_member(package, script, child, order):
    return '0x' + b''.join(v.to_bytes(4, 'little') for v in (package, script, child, order)).hex().upper()


def check_manual_edits(db, force):
    """Scripts modifies en base depuis le dernier SQL genere."""
    if not os.path.exists(LAST_BUILD):
        return
    with open(LAST_BUILD, encoding='utf-8') as f:
        last = {int(k): v for k, v in json.load(f).items()}
    rows = db.query('SELECT `ID`, `Name`, `Script` FROM `%s`.`scene_script_text` WHERE `ID` BETWEEN %d AND %d'
                    % (db.hotfixes, SCRIPT_FIRST, SCRIPT_LAST))
    report = []
    for script_id, name, script in rows:
        before = last.get(int(script_id))
        if before is None or before == script:
            continue
        diff = difflib.unified_diff(before.split('\n'), script.split('\n'), lineterm='', n=0)
        lines = [d for d in diff if not d.startswith(('---', '+++'))]
        report.append('%s (%s) :\n    %s' % (name, script_id, '\n    '.join(lines)))
    if report and not force:
        raise BuildError('scripts modifies en base depuis la derniere generation, a reporter dans les Lua '
                         '(ou --force pour les abandonner) :\n' + '\n'.join(report))


def save_last_build(scripts):
    with open(LAST_BUILD, 'w', encoding='utf-8') as f:
        json.dump({str(s['id']): '\n' + s['code'] for s in scripts}, f, ensure_ascii=False, indent=0)


def build(date, force=False):
    db = Database()
    try:
        check_manual_edits(db, force)
        resolver = Resolver(db)
        framework = resolver.expand(read(FRAMEWORK), 'framework.lua')
        scenes = [load_scene(os.path.join(SCENE_DIR, f)) for f in sorted(os.listdir(SCENE_DIR)) if f.endswith('.lua')]
        for scene in scenes:
            body = resolver.expand(scene['code'], scene['file'])
            scene['lua'] = actor_table(resolver, body) + body
            left = re.findall(r'\b(NPC|SPELL|CONST|SV|ANIM|POS|PATH|PATHSTART|PATHEND|LOC|LOCFROM|SPAWN|T)\(', scene['lua'])
            if left:
                raise BuildError('%s : macro non resolue (%s)' % (scene['file'], ', '.join(sorted(set(left)))))
    finally:
        db.close()

    # Identifiants des morceaux de script
    next_script = SCRIPT_FIRST
    scripts = []

    def add_chain(name, code, where):
        nonlocal next_script
        chunks = split_chunks(code, where)
        head = next_script
        for index, chunk in enumerate(chunks):
            script_id = next_script
            next_script += 1
            if next_script > SCRIPT_LAST + 1:
                raise BuildError('plage SceneScript epuisee')
            scripts.append({
                'id': script_id, 'first': 0 if index == 0 else head,
                'next': script_id + 1 if index + 1 < len(chunks) else 0,
                'name': name if index == 0 else '[%d] %s' % (index, name), 'code': chunk})
        return head

    framework_head = add_chain('Emerald - Dialogue Framework', framework, 'framework.lua')
    members = []
    next_member = MEMBER_FIRST
    for offset, scene in enumerate(scenes):
        scene['package'] = PACKAGE_FIRST + offset
        scene['head'] = add_chain(scene['name'], scene['lua'], scene['file'])
        for script, child, order in ((0, CINEMATIC_FRAMEWORK_PACKAGE, 1), (framework_head, 0, 2), (scene['head'], 0, 3)):
            members.append({'id': next_member, 'package': scene['package'], 'script': script, 'child': child, 'order': order})
            next_member += 1
    if next_member > MEMBER_LAST + 1:
        raise BuildError('plage SceneScriptPackageMember epuisee')

    write_hotfixes(date, resolver, scenes, scripts, members)
    write_world(date, scenes)
    save_last_build(scripts)
    return scenes, scripts, resolver


def write_hotfixes(date, resolver, scenes, scripts, members):
    texts = sorted(resolver.texts.values(), key=lambda t: t['id'])
    package_last = PACKAGE_FIRST + len(scenes) - 1
    out = []
    w = out.append
    w('-- ============================================================================')
    w('-- La Chute de Theramore (map 5000) : scenes de dialogue')
    w('--')
    w('-- FICHIER GENERE par sql/custom/scenes/build_scenes.py : ne pas le modifier a')
    w('-- la main, modifier les .lua (ou creature_text) et relancer le generateur.')
    w('--')
    w('-- Chaque dialogue scripte est une scene client jouee dans le monde : la')
    w('-- scene fait apparaitre ses acteurs a la place des vrais PNJ (masques par le')
    w('-- serveur), les repliques sont des bulles portees par les acteurs.')
    w('--   SceneScriptPackage %d a %d, un par scene :' % (PACKAGE_FIRST, package_last))
    w('--     membre ordre 1 : package enfant 156 (Cinematic Framework)')
    w('--     membre ordre 2 : script %d, outils communs (framework.lua)' % scripts[0]['id'])
    w('--     membre ordre 3 : script de la scene')
    for scene in scenes:
        w('--     %d %-40s scene_template %d, script %d' % (scene['package'], scene['name'], scene['scene'], scene['head']))
    w('--   BroadcastText %d a %d : repliques reprises de creature_text' % (texts[0]['id'], texts[-1]['id']))
    w('--')
    w('-- SceneScriptPackageMember.db2 n est pas charge par le core : hotfix_blob.')
    w('--   SceneScriptPackageID u32, SceneScriptID u32, ChildSceneScriptPackageID u32, OrderIndex u32')
    w('-- ============================================================================')
    w('')
    w('SET @HASH_BROADCAST_TEXT        := %d;     -- BroadcastText.db2' % HASH_BROADCAST_TEXT)
    w('SET @HASH_SCENE_SCRIPT          := %d;   -- SceneScript.db2' % HASH_SCENE_SCRIPT)
    w('SET @HASH_SCENE_SCRIPT_TEXT     := %d;    -- SceneScriptText.db2' % HASH_SCENE_SCRIPT_TEXT)
    w('SET @HASH_SCENE_SCRIPT_PACKAGE  := %d;   -- SceneScriptPackage.db2' % HASH_SCENE_SCRIPT_PACKAGE)
    w('SET @HASH_SCENE_SCRIPT_MEMBER   := %d;   -- SceneScriptPackageMember.db2' % HASH_SCENE_SCRIPT_MEMBER)
    w("SET @UNIQUE_ID := TO_DAYS('%s');         -- Un UniqueId par jour" % date.replace('_', '-'))
    w('')
    w('-- ----------------------------------------------------------------------------')
    w('-- BroadcastText.db2 : Flags 0, bulle et canal de discussion comme un Talk')
    w('-- ----------------------------------------------------------------------------')
    w('DELETE FROM `broadcast_text` WHERE `ID` BETWEEN %d AND %d;' % (BROADCAST_FIRST, BROADCAST_LAST))
    w('INSERT INTO `broadcast_text` (`Text`,`Text1`,`ID`,`LanguageID`,`ConditionID`,`EmotesID`,`Flags`,`ChatBubbleDurationMs`,'
      '`VoiceOverPriorityID`,`SoundKitID1`,`SoundKitID2`,`EmoteID1`,`EmoteID2`,`EmoteID3`,`EmoteDelay1`,`EmoteDelay2`,'
      '`EmoteDelay3`,`VerifiedBuild`) VALUES')
    w(join_rows([('(%s, %s, %d, 0, 0, 0, 0, 0, 0, %d, %d, 0, 0, 0, 0, 0, 0, -1)'
                  % (sql_string(t['text']), sql_string(t['text']), t['id'], t['sound'], t['sound']),
                  '%d groupe %d' % (t['entry'], t['group'])) for t in texts]))
    w('')
    w("DELETE FROM `broadcast_text_locale` WHERE `ID` BETWEEN %d AND %d AND `locale` = 'frFR';" % (BROADCAST_FIRST, BROADCAST_LAST))
    w('INSERT INTO `broadcast_text_locale` (`ID`,`locale`,`Text_lang`,`Text1_lang`,`VerifiedBuild`)')
    w("SELECT `ID`, 'frFR', `Text`, `Text1`, -1 FROM `broadcast_text` WHERE `ID` BETWEEN %d AND %d;" % (BROADCAST_FIRST, BROADCAST_LAST))
    w('')
    w('-- ----------------------------------------------------------------------------')
    w('-- SceneScriptPackage.db2')
    w('-- ----------------------------------------------------------------------------')
    w('DELETE FROM `scene_script_package` WHERE `ID` BETWEEN %d AND %d;' % (PACKAGE_FIRST, PACKAGE_FIRST + 98))
    w('INSERT INTO `scene_script_package` (`ID`,`Name`,`Flags`,`Unknown915`,`VerifiedBuild`) VALUES')
    w(join_rows(['(%d, %s, 0, 1, -1)' % (s['package'], sql_string(s['name'])) for s in scenes]))
    w('')
    w('-- ----------------------------------------------------------------------------')
    w('-- SceneScriptPackageMember.db2 (hotfix_blob)')
    w('-- ----------------------------------------------------------------------------')
    w('DELETE FROM `hotfix_blob` WHERE `TableHash` = @HASH_SCENE_SCRIPT_MEMBER AND `RecordId` BETWEEN %d AND %d;' % (MEMBER_FIRST, MEMBER_LAST))
    w('INSERT INTO `hotfix_blob` (`TableHash`,`RecordId`,`locale`,`Blob`,`VerifiedBuild`) VALUES')
    w(join_rows([("(@HASH_SCENE_SCRIPT_MEMBER, %d, 'frFR', %s, -1)"
                  % (m['id'], blob_member(m['package'], m['script'], m['child'], m['order'])),
                  'package %d, %s, ordre %d' % (m['package'], 'enfant %d' % m['child'] if m['child'] else 'script %d' % m['script'],
                                               m['order'])) for m in members]))
    w('')
    w('-- ----------------------------------------------------------------------------')
    w('-- SceneScript.db2 + SceneScriptText.db2')
    w('-- ----------------------------------------------------------------------------')
    w('DELETE FROM `scene_script` WHERE `ID` BETWEEN %d AND %d;' % (SCRIPT_FIRST, SCRIPT_LAST))
    w('INSERT INTO `scene_script` (`ID`,`FirstSceneScriptID`,`NextSceneScriptID`,`Unknown915`,`VerifiedBuild`) VALUES')
    w(join_rows(['(%d, %d, %d, 1, -1)' % (s['id'], s['first'], s['next']) for s in scripts]))
    w('')
    w('DELETE FROM `scene_script_text` WHERE `ID` BETWEEN %d AND %d;' % (SCRIPT_FIRST, SCRIPT_LAST))
    w('INSERT INTO `scene_script_text` (`ID`,`Name`,`Script`,`VerifiedBuild`) VALUES')
    w(join_rows(['(%d, %s, %s, -1)' % (s['id'], sql_string(s['name']), sql_string('\n' + s['code'])) for s in scripts]))
    w('')
    w('-- ----------------------------------------------------------------------------')
    w('-- hotfix_data')
    w('-- ----------------------------------------------------------------------------')
    w('-- Id calcule avant le DELETE : un Id neuf a chaque passage, sinon le client garde son cache')
    w('SET @HOTFIX_ID := (SELECT IFNULL(MAX(`Id`), 0) + 1 FROM `hotfix_data`);')
    w('DELETE FROM `hotfix_data` WHERE (`TableHash` = @HASH_BROADCAST_TEXT AND `RecordId` BETWEEN %d AND %d)' % (BROADCAST_FIRST, BROADCAST_LAST))
    w('    OR (`TableHash` = @HASH_SCENE_SCRIPT_PACKAGE AND `RecordId` BETWEEN %d AND %d)' % (PACKAGE_FIRST, PACKAGE_FIRST + 98))
    w('    OR (`TableHash` = @HASH_SCENE_SCRIPT_MEMBER AND `RecordId` BETWEEN %d AND %d)' % (MEMBER_FIRST, MEMBER_LAST))
    w('    OR (`TableHash` = @HASH_SCENE_SCRIPT AND `RecordId` BETWEEN %d AND %d)' % (SCRIPT_FIRST, SCRIPT_LAST))
    w('    OR (`TableHash` = @HASH_SCENE_SCRIPT_TEXT AND `RecordId` BETWEEN %d AND %d);' % (SCRIPT_FIRST, SCRIPT_LAST))
    w('INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES')
    entries = [('@HASH_BROADCAST_TEXT', t['id']) for t in texts]
    entries += [('@HASH_SCENE_SCRIPT_PACKAGE', s['package']) for s in scenes]
    entries += [('@HASH_SCENE_SCRIPT_MEMBER', m['id']) for m in members]
    for s in scripts:
        entries += [('@HASH_SCENE_SCRIPT', s['id']), ('@HASH_SCENE_SCRIPT_TEXT', s['id'])]
    w(join_rows(['(@HOTFIX_ID+%d, @UNIQUE_ID, %s, %d, 1, -1)' % (i, h, r) for i, (h, r) in enumerate(entries)]))
    write_file(os.path.join(SOURCES, 'sql', 'custom', 'hotfixes', '%s_00_hotfixes_theramore_dialogue_scenes.sql' % date), out)


def join_rows(rows):
    """Lignes d'un INSERT : virgule apres chaque ligne, point-virgule apres la
    derniere. Une ligne est une chaine ou un couple (ligne, commentaire)."""
    result = []
    for i, row in enumerate(rows):
        body, comment = row if isinstance(row, tuple) else (row, None)
        line = body + (',' if i + 1 < len(rows) else ';')
        result.append(line + ' -- ' + comment if comment else line)
    return '\n'.join(result)


def write_world(date, scenes):
    out = []
    w = out.append
    w('-- ============================================================================')
    w('-- La Chute de Theramore (map 5000) : scenes de dialogue')
    w('--')
    w('-- FICHIER GENERE par sql/custom/scenes/build_scenes.py.')
    w('-- Packages et scripts : %s_00_hotfixes_theramore_dialogue_scenes.sql' % date)
    w('-- Flags 0 : scene jouee dans le monde, sans camera ni phase. Toutes passent')
    w('-- par %s (CustomScenario::DialogueSceneScript).' % SCENE_SCRIPT_NAME)
    w('-- ============================================================================')
    w('')
    first, last = scenes[0]['scene'], scenes[-1]['scene']
    w('DELETE FROM `scene_template` WHERE `SceneId` BETWEEN %d AND %d;' % (first, last))
    w('INSERT INTO `scene_template` (`SceneId`,`Flags`,`ScriptPackageID`,`Encrypted`,`ScriptName`) VALUES')
    w(join_rows([("(%d, %d, %d, 0, '%s')" % (s['scene'], SCENE_FLAGS, s['package'], SCENE_SCRIPT_NAME), s['name'])
                 for s in scenes]))
    write_file(os.path.join(SOURCES, 'sql', 'custom', 'world', '%s_00_world_theramore_dialogue_scenes.sql' % date), out)


def write_file(path, lines):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print('ecrit', path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--date', default=datetime.date.today().strftime('%Y_%m_%d'))
    parser.add_argument('--force', action='store_true', help='ecraser les scripts modifies en base')
    options = parser.parse_args()
    try:
        scenes, scripts, resolver = build(options.date, options.force)
    except BuildError as error:
        print('ERREUR :', error, file=sys.stderr)
        return 1
    print('%d scenes, %d morceaux de script, %d repliques' % (len(scenes), len(scripts), len(resolver.texts)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
