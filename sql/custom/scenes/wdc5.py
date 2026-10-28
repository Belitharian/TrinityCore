"""Lecteur WDC5 minimal : renvoie {id: [valeurs des champs]} (entiers seulement)."""
import struct, sys


def read_wdc5(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'WDC5', d[:4]
    o = 4 + 4 + 128
    (record_count, field_count, record_size, string_table_size, table_hash, layout_hash,
     min_id, max_id, locale) = struct.unpack_from('<9I', d, o); o += 36
    flags, id_index = struct.unpack_from('<HH', d, o); o += 4
    (total_field_count, bitpacked_data_offset, lookup_column_count, field_storage_info_size,
     common_data_size, pallet_data_size, section_count) = struct.unpack_from('<7I', d, o); o += 28
    sections = []
    for _ in range(section_count):
        s = struct.unpack_from('<Q8I', d, o); o += 40
        sections.append(dict(file_offset=s[1], record_count=s[2], string_table_size=s[3],
                             offset_records_end=s[4], id_list_size=s[5], relationship_data_size=s[6],
                             offset_map_id_count=s[7], copy_table_count=s[8]))
    o += total_field_count * 4  # field_structure
    infos = []
    for _ in range(field_storage_info_size // 24):
        infos.append(struct.unpack_from('<HHIIIII', d, o)); o += 24
    pallet_start = o
    o += pallet_data_size
    common_start = o
    o += common_data_size

    # Offsets de palette / donnees communes par champ
    pal_off, com_off = [], []
    p = c = 0
    for info in infos:
        pal_off.append(p); com_off.append(c)
        if info[3] in (3, 4):
            p += info[2]
        elif info[3] == 2:
            c += info[2]

    common = {}
    for fi, info in enumerate(infos):
        if info[3] == 2:
            m = {}
            base = common_start + com_off[fi]
            for k in range(info[2] // 8):
                rid, val = struct.unpack_from('<II', d, base + k * 8)
                m[rid] = val
            common[fi] = (m, info[4])

    rows = {}
    for s in sections:
        if s['record_count'] == 0:
            continue
        rec = s['file_offset']
        if flags & 1:
            raise NotImplementedError('offset map')
        id_list_off = rec + s['record_count'] * record_size + s['string_table_size']
        ids = struct.unpack_from('<%dI' % s['record_count'], d, id_list_off) if s['id_list_size'] else None
        for r in range(s['record_count']):
            base = rec + r * record_size
            raw = int.from_bytes(d[base:base + record_size] + b'\0' * 8, 'little')
            rid = ids[r] if ids else None
            vals = []
            for fi, info in enumerate(infos):
                off_bits, size_bits, add, stype, v1, v2, v3 = info
                if stype == 0:
                    v = (raw >> off_bits) & ((1 << size_bits) - 1)
                elif stype in (1, 5):
                    v = (raw >> off_bits) & ((1 << v2) - 1)
                    if stype == 5 and v & (1 << (v2 - 1)):
                        v -= 1 << v2
                elif stype == 3:
                    idx = (raw >> off_bits) & ((1 << v2) - 1)
                    v = struct.unpack_from('<I', d, pallet_start + pal_off[fi] + idx * 4)[0]
                elif stype == 2:
                    v = None  # resolu apres l'ID
                else:
                    v = '?'
                vals.append(v)
            rows[rid] = vals
        for fi, (m, default) in common.items():
            for rid in rows:
                rows[rid][fi] = m.get(rid, default)
    return rows
