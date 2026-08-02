#!/usr/bin/env python3
"""Drive the Ozone Maximizer's IRC **Mode** headlessly.

The Maximizer's IRC mode is NOT an automatable VST3 parameter (pedalboard only sees
Character / Input Gain / Output Level / soft-clip), so it cannot be set the way
`mbl_plugin_verify.py` sets our own params. It lives in the plugin's opaque state chunk:

    VST3 preset container (48-byte header + body + 'List' chunk)
      -> iZotope state blob (12-byte prefix + raw_len + zlib payload + b'L')
        -> JSON: /DSP State/Value/DSP Elements/Value/Maximizer/Value/Mode/Value  (UInt)

This module unpacks that container, edits the JSON, and repacks it with every offset and
size recomputed, so the rig can render the same source through every IRC mode unattended.

Verified layout (Ozone 11 Maximizer, state len 1727):
    header   : 'VST3' + int32 version + 32-byte classID + int64 listOffset(=1679)
    body     : [48:1679]  -> magic 0x00688ade, uint32 ver=4, uint32 payload=1618,
                            uint32 raw_len=26127, zlib[1614], b'L'
    List     : 2 entries -> Comp(offset=48, size=1631), Cont(offset=1679, size=0)

The mode->IRC-name mapping is deliberately NOT hardcoded: `mbl_frontier.py` identifies
IRC 1 empirically by matching the known pre-rendered Ozone-11 reference render, and labels
the rest relative to it.

Claude's role (orchestration + measurement); no DSP is implemented here.
"""
import json
import struct
import zlib

# VST3 preset container header
_HDR = struct.Struct("<4si32sq")
# iZotope state blob prefix: magic, version, payload size (raw_len field + zlib bytes)
_BLOB = struct.Struct("<III")
_BLOB_MAGIC = 0x00688ADE

MODE_PATH = ("DSP State", "DSP Elements", "Maximizer", "Mode")


def unpack(preset_data: bytes):
    """preset_data -> (state_dict, rebuild_context)."""
    magic, version, class_id, list_offset = _HDR.unpack_from(preset_data, 0)
    if magic != b"VST3":
        raise ValueError(f"not a VST3 preset container: {magic!r}")

    body = preset_data[_HDR.size:list_offset]
    blob_magic, blob_ver, payload_size = _BLOB.unpack_from(body, 0)
    if blob_magic != _BLOB_MAGIC:
        raise ValueError(f"unexpected iZotope state magic 0x{blob_magic:08x}")

    payload = body[_BLOB.size:_BLOB.size + payload_size]
    raw_len = struct.unpack_from("<I", payload, 0)[0]
    raw = zlib.decompress(payload[4:])
    if len(raw) != raw_len:
        raise ValueError(f"raw_len mismatch: header {raw_len}, got {len(raw)}")

    ctx = dict(version=version, class_id=class_id, blob_ver=blob_ver,
               body_trailer=body[_BLOB.size + payload_size:],
               tail=preset_data[list_offset:])
    return json.loads(raw), ctx


def pack(state: dict, ctx: dict) -> bytes:
    """(state_dict, rebuild_context) -> preset_data, with all offsets/sizes recomputed."""
    raw = json.dumps(state, indent=3).encode("utf-8")
    payload = struct.pack("<I", len(raw)) + zlib.compress(raw, 9)

    body = (_BLOB.pack(_BLOB_MAGIC, ctx["blob_ver"], len(payload))
            + payload + ctx["body_trailer"])
    list_offset = _HDR.size + len(body)

    tail = _rebuild_list(ctx["tail"], body_size=len(body), list_offset=list_offset)
    return _HDR.pack(b"VST3", ctx["version"], ctx["class_id"], list_offset) + body + tail


def _rebuild_list(tail: bytes, body_size: int, list_offset: int) -> bytes:
    """The 'List' chunk stores (offset, size) per entry. Recompressing changes the body
    length, so Comp's size AND Cont's offset both move."""
    if tail[:4] != b"List":
        raise ValueError(f"expected List chunk, got {tail[:4]!r}")
    count = struct.unpack_from("<i", tail, 4)[0]

    out = bytearray(tail)
    pos = 8
    for _ in range(count):
        entry_id = bytes(out[pos:pos + 4])
        offset, size = struct.unpack_from("<qq", out, pos + 4)
        if entry_id == b"Comp":
            struct.pack_into("<qq", out, pos + 4, _HDR.size, body_size)
        elif entry_id == b"Cont":
            struct.pack_into("<qq", out, pos + 4, list_offset, size)
        pos += 20
    return bytes(out)


def _maximizer(state: dict) -> dict:
    """The Maximizer element's field dict. Root is a bare dict; every level below it is
    wrapped in {"Type","Value"}."""
    node = state[MODE_PATH[0]]
    for key in MODE_PATH[1:-1]:
        node = node["Value"][key]
    return node["Value"]


def get_params(preset_data: bytes, *names):
    """Read Maximizer state fields, e.g. get_params(d, "Mode", "Gain", "Margin")."""
    mx = _maximizer(unpack(preset_data)[0])
    return {n: mx[n]["Value"] for n in names}


def set_params(preset_data: bytes, **fields) -> bytes:
    """Return new preset_data with Maximizer state fields set atomically.

    Setting Mode/Gain/Margin together in ONE state write avoids the ordering trap where a
    later `preset_data` assignment resets parameters set through pedalboard attributes.

    Field types are taken from the existing leaf, so ints stay UInt and floats stay Float.
    """
    state, ctx = unpack(preset_data)
    mx = _maximizer(state)
    for name, value in fields.items():
        if name not in mx:
            raise KeyError(f"no Maximizer state field {name!r}")
        leaf = mx[name]
        kind = leaf["Type"]
        leaf["Value"] = (int(value) if kind == "UInt"
                         else bool(value) if kind == "Bool"
                         else float(value))
    return pack(state, ctx)


def get_mode(preset_data: bytes) -> int:
    return int(get_params(preset_data, "Mode")["Mode"])


def set_mode(preset_data: bytes, mode: int) -> bytes:
    return set_params(preset_data, Mode=mode)


def num_modes(plugin, base: bytes, probe_max: int = 12) -> int:
    """How many IRC modes this Maximizer build accepts.

    Must round-trip through the PLUGIN, not just the JSON: an out-of-range mode is clamped
    by the plugin on load, so it is only detectable by reading `preset_data` back.
    """
    n = 0
    for m in range(probe_max):
        plugin.preset_data = set_mode(base, m)
        if get_mode(bytes(plugin.preset_data)) != m:
            break
        n = m + 1
    return n
