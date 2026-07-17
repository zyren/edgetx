#!/usr/bin/env python3
"""Build, read, and strictly validate the GX12 external CJK font container.

The format deliberately has no dependency on a C compiler, a packed struct, or
an external font library.  A glyph body is supplied in column-major, 1bpp
form.  The body is copied into a 32-byte slot and the unused tail of that slot
is zero-filled.

There must be exactly 20992 glyphs for every strike.  The compact body length
is 20, 24, or 32 bytes for the 10, 12, and 16 pixel strikes respectively.  The
formal CLI exposes strict ``validate FILE`` only; production generation lives
in :mod:`tools.cn_fonts.generate`.
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from collections.abc import Iterable, Mapping
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, BinaryIO, Optional, Union


# ---------------------------------------------------------------------------
# Fixed format constants

MAGIC = b"ETXCNF\x00\x00"
VERSION = 1
HEADER_SIZE = 64
DIRECTORY_ENTRY_SIZE = 32
SLOT_SIZE = 32
FIRST_CODEPOINT = 0x4E00
LAST_CODEPOINT = 0x9FFF
GLYPH_COUNT = LAST_CODEPOINT - FIRST_CODEPOINT + 1
ENDIAN_MARKER = 0x12345678
PAYLOAD_ALIGNMENT = 512
MAX_STRIKE_COUNT = 3
SUPPORTED_STRIKE_IDS = (10, 12, 16)
U32_MAX = 0xFFFFFFFF


@dataclass(frozen=True)
class StrikeGeometry:
    """The immutable geometry contract for one supported strike."""

    id: int
    width: int
    height: int
    bytes_per_column: int
    advance: int

    @property
    def body_length(self) -> int:
        return self.width * self.bytes_per_column


GEOMETRIES = {
    10: StrikeGeometry(10, 10, 10, 2, 11),
    12: StrikeGeometry(12, 12, 12, 2, 13),
    16: StrikeGeometry(16, 16, 16, 2, 17),
}


# The formats describe bytes only; they are not used as a C packed struct.
_HEADER_FORMAT = "<8sHHIHHBBHIIIIII16s"
_DIRECTORY_FORMAT = "<6BHHHIII8s"
_HEADER_STRUCT_SIZE = struct.calcsize(_HEADER_FORMAT)
_DIRECTORY_STRUCT_SIZE = struct.calcsize(_DIRECTORY_FORMAT)
if _HEADER_STRUCT_SIZE != HEADER_SIZE or _DIRECTORY_STRUCT_SIZE != DIRECTORY_ENTRY_SIZE:
    raise RuntimeError("internal format size declaration is inconsistent")


# ---------------------------------------------------------------------------
# Public data classes


@dataclass(frozen=True)
class StrikeInput:
    """Input for :func:`build_container`.

    ``width``, ``height``, and ``advance`` are optional convenience fields.
    When omitted, they are inferred from ``id`` and are still checked against
    the fixed 10/12/16 geometry contract.
    """

    id: int
    glyphs: Iterable[bytes]
    width: Optional[int] = None
    height: Optional[int] = None
    advance: Optional[int] = None



@dataclass(frozen=True)
class DirectoryEntry:
    """A validated directory entry from an external-font file."""

    id: int
    width: int
    height: int
    bytes_per_column: int
    advance: int
    flags: int
    slot_size: int
    glyph_count: int
    data_offset: int
    data_length: int
    strike_crc32: int

    @property
    def body_length(self) -> int:
        return self.width * self.bytes_per_column


@dataclass(frozen=True)
class ExternalFont:
    """An already strictly validated container.

    The private id map makes extraction independent of strike count and avoids
    scanning directory entries for each lookup.  Codepoint indexing is a
    direct multiplication by the fixed slot size.
    """

    data: bytes
    entries: tuple[DirectoryEntry, ...]
    payload_offset: int
    payload_length: int
    total_file_size: int
    payload_crc32: int
    header_crc32: int
    _entry_by_id: dict[int, DirectoryEntry] = field(
        init=False, repr=False, compare=False
    )

    def __post_init__(self) -> None:
        object.__setattr__(self, "_entry_by_id", {entry.id: entry for entry in self.entries})

    def _get_entry(self, strike: Union[int, DirectoryEntry]) -> DirectoryEntry:
        if isinstance(strike, DirectoryEntry):
            strike_id = strike.id
        elif _is_int(strike):
            strike_id = strike
        else:
            raise ValueError("strike must be a supported integer strike id")
        _check_uint(strike_id, 8, "strike id")
        try:
            return self._entry_by_id[strike_id]
        except KeyError as exc:
            raise ValueError(f"unknown strike id: {strike_id}") from exc

    def _slot(self, strike: Union[int, DirectoryEntry], codepoint: int) -> bytes:
        entry = self._get_entry(strike)
        index = _codepoint_index(codepoint)
        relative = _checked_mul(index, SLOT_SIZE, "glyph slot index")
        start = _checked_add(entry.data_offset, relative, "glyph slot offset")
        end = _checked_add(start, SLOT_SIZE, "glyph slot end")
        if end > len(self.data):
            raise ValueError("glyph slot is outside the file")
        result = self.data[start:end]
        if len(result) != SLOT_SIZE:
            raise ValueError("truncated glyph slot")
        return result

    def bitmap(self, strike: Union[int, DirectoryEntry], codepoint: int) -> bytes:
        """Return the compact column-major bitmap, without slot padding."""

        entry = self._get_entry(strike)
        return self._slot(strike, codepoint)[: entry.body_length]

# ---------------------------------------------------------------------------
# Checked arithmetic and primitive validation helpers


def _is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _check_uint(value: Any, bits: int, name: str) -> int:
    if not _is_int(value) or value < 0 or value > ((1 << bits) - 1):
        raise ValueError(f"{name} is not a valid unsigned {bits}-bit integer")
    return int(value)


def _checked_add(left: int, right: int, name: str) -> int:
    if not _is_int(left) or not _is_int(right) or left < 0 or right < 0:
        raise ValueError(f"{name}: negative or non-integer arithmetic")
    if left > U32_MAX or right > U32_MAX:
        raise ValueError(f"{name}: 32-bit integer overflow")
    result = left + right
    if result > U32_MAX:
        raise ValueError(f"{name}: 32-bit integer overflow")
    return result


def _checked_mul(left: int, right: int, name: str) -> int:
    if not _is_int(left) or not _is_int(right) or left < 0 or right < 0:
        raise ValueError(f"{name}: negative or non-integer arithmetic")
    if left > U32_MAX or right > U32_MAX:
        raise ValueError(f"{name}: 32-bit integer overflow")
    if left and right > U32_MAX // left:
        raise ValueError(f"{name}: 32-bit integer overflow")
    result = left * right
    if result > U32_MAX:
        raise ValueError(f"{name}: 32-bit integer overflow")
    return result


def _align_up(value: int, alignment: int, name: str = "alignment") -> int:
    if not _is_int(value) or value < 0:
        raise ValueError(f"{name}: invalid integer")
    if not _is_int(alignment) or alignment <= 0:
        raise ValueError(f"{name}: invalid alignment")
    remainder = value % alignment
    if remainder == 0:
        result = value
    else:
        result = _checked_add(value, alignment - remainder, name)
    if result > U32_MAX:
        raise ValueError(f"{name}: 32-bit integer overflow")
    return result


def _crc32(data: bytes) -> int:
    return zlib.crc32(data) & U32_MAX


def _codepoint_index(codepoint: int) -> int:
    if not _is_int(codepoint):
        raise ValueError("codepoint must be an integer")
    if codepoint < FIRST_CODEPOINT or codepoint > LAST_CODEPOINT:
        raise ValueError(f"codepoint outside U+{FIRST_CODEPOINT:04X}..U+{LAST_CODEPOINT:04X}")
    return codepoint - FIRST_CODEPOINT


def _as_bytes(source: Any) -> bytes:
    """Read a source without interpreting arbitrary integers as byte counts."""

    if isinstance(source, ExternalFont):
        return source.data
    if isinstance(source, bytes):
        return source
    if isinstance(source, (bytearray, memoryview)):
        return bytes(source)
    if hasattr(source, "read"):
        value = source.read()
        if not isinstance(value, (bytes, bytearray, memoryview)):
            raise ValueError("file-like source did not return bytes")
        return bytes(value)
    if isinstance(source, (str, Path)):
        return Path(source).read_bytes()
    try:
        # os.PathLike without importing os (and with a useful error for other
        # objects) is handled here.
        return Path(source).read_bytes()
    except (TypeError, ValueError) as exc:
        raise ValueError("container source must be bytes or a path") from exc


# ---------------------------------------------------------------------------
# Input normalization and deterministic serialization


_MISSING = object()


def _field(value: Any, names: tuple[str, ...], default: Any = _MISSING) -> Any:
    if isinstance(value, Mapping):
        for name in names:
            if name in value:
                return value[name]
    else:
        for name in names:
            if hasattr(value, name):
                return getattr(value, name)
    if default is not _MISSING:
        return default
    raise ValueError(f"missing strike field: {names[0]}")


def _coerce_id(value: Any, name: str = "strike id") -> int:
    # Decimal strings are useful for JSON object keys, but no other coercion is
    # performed.  In particular, floats and booleans are never silently turned
    # into format integers.
    if isinstance(value, str) and value.isdecimal():
        value = int(value, 10)
    return _check_uint(value, 8, name)


def _item_to_spec(item: Any, default_id: Any = _MISSING) -> StrikeInput:
    if isinstance(item, StrikeInput):
        return item

    if isinstance(item, Mapping):
        strike_id = _field(item, ("id", "strike", "size"), default_id)
        if strike_id is _MISSING:
            raise ValueError("strike input has no id")
        glyphs = _field(item, ("glyphs", "glyph_bodies", "bodies"))
        return StrikeInput(
            id=_coerce_id(strike_id),
            glyphs=glyphs,
            width=_field(item, ("width",), None),
            height=_field(item, ("height",), None),
            advance=_field(item, ("advance",), None),
        )

    if isinstance(item, (tuple, list)):
        if len(item) == 2:
            return StrikeInput(id=_coerce_id(item[0]), glyphs=item[1])
        if len(item) == 5:
            return StrikeInput(
                id=_coerce_id(item[0]),
                width=item[1],
                height=item[2],
                advance=item[3],
                glyphs=item[4],
            )
        raise ValueError("strike tuple must contain (id, glyphs) or five fields")

    # This also supports user-defined objects with id/glyphs attributes.
    try:
        strike_id = _field(item, ("id", "strike", "size"), default_id)
        glyphs = _field(item, ("glyphs", "glyph_bodies", "bodies"))
    except ValueError as exc:
        raise ValueError("invalid strike input") from exc
    return StrikeInput(
        id=_coerce_id(strike_id),
        glyphs=glyphs,
        width=_field(item, ("width",), None),
        height=_field(item, ("height",), None),
        advance=_field(item, ("advance",), None),
    )


def _iter_specs(strikes: Any) -> list[StrikeInput]:
    if isinstance(strikes, Mapping) and "strikes" in strikes:
        strikes = strikes["strikes"]

    if isinstance(strikes, Mapping):
        # A single dictionary strike can be passed directly.
        if any(key in strikes for key in ("glyphs", "glyph_bodies", "bodies")):
            return [_item_to_spec(strikes)]
        result = []
        for key, value in strikes.items():
            if isinstance(value, Mapping) and any(
                name in value for name in ("glyphs", "glyph_bodies", "bodies")
            ):
                result.append(_item_to_spec(value, key))
            else:
                result.append(StrikeInput(id=_coerce_id(key), glyphs=value))
        return result

    if isinstance(strikes, (bytes, bytearray, memoryview, str)):
        raise ValueError("strikes must be a collection of strike inputs")
    try:
        return [_item_to_spec(item) for item in strikes]
    except TypeError as exc:
        raise ValueError("strikes must be a collection of strike inputs") from exc


def _normalize_glyphs(glyphs: Any, geometry: StrikeGeometry) -> tuple[bytes, ...]:
    if isinstance(glyphs, (bytes, bytearray, memoryview, str)):
        raise ValueError("glyphs must contain one bytes body per codepoint")
    expected_length = _checked_mul(
        geometry.width, geometry.bytes_per_column, "compact glyph body length"
    )
    try:
        iterator = iter(glyphs)
    except TypeError as exc:
        raise ValueError("glyphs must be iterable") from exc

    result: list[bytes] = []
    for index, body in enumerate(iterator):
        if index >= GLYPH_COUNT:
            raise ValueError(f"strike {geometry.id}: glyph count exceeds {GLYPH_COUNT}")
        if not isinstance(body, (bytes, bytearray, memoryview)):
            raise ValueError(f"strike {geometry.id} glyph {index}: body must be bytes")
        body_bytes = bytes(body)
        if len(body_bytes) != expected_length:
            raise ValueError(
                f"strike {geometry.id} glyph {index}: expected {expected_length} bytes, "
                f"got {len(body_bytes)}"
            )
        result.append(body_bytes)
    if len(result) != GLYPH_COUNT:
        raise ValueError(
            f"strike {geometry.id}: glyph count expected {GLYPH_COUNT}, got {len(result)}"
        )
    return tuple(result)


@dataclass(frozen=True)
class _NormalizedStrike:
    geometry: StrikeGeometry
    glyphs: tuple[bytes, ...]
    data: bytes


def _normalize_strikes(strikes: Any) -> list[_NormalizedStrike]:
    specs = _iter_specs(strikes)
    if not specs:
        raise ValueError("at least one strike is required")
    if len(specs) > MAX_STRIKE_COUNT:
        raise ValueError(f"at most {MAX_STRIKE_COUNT} strikes are supported")

    result: list[_NormalizedStrike] = []
    seen: set[int] = set()
    for spec in specs:
        strike_id = _coerce_id(spec.id)
        if strike_id in seen:
            raise ValueError(f"duplicate strike id: {strike_id}")
        seen.add(strike_id)
        try:
            geometry = GEOMETRIES[strike_id]
        except KeyError as exc:
            raise ValueError(f"unknown strike geometry: {strike_id}") from exc

        for value, expected, name in (
            (spec.width, geometry.width, "width"),
            (spec.height, geometry.height, "height"),
            (spec.advance, geometry.advance, "advance"),
        ):
            if value is not None:
                _check_uint(value, 8, f"strike {strike_id} {name}")
                if value != expected:
                    raise ValueError(
                        f"strike {strike_id}: {name} must be {expected}, got {value}"
                    )

        compact = _normalize_glyphs(spec.glyphs, geometry)
        data_length = _checked_mul(GLYPH_COUNT, SLOT_SIZE, "strike data length")
        body_length = geometry.body_length
        if body_length > SLOT_SIZE:
            raise ValueError(f"strike {strike_id}: compact body does not fit in slot")
        data = bytearray(data_length)
        cursor = 0
        for body in compact:
            end = _checked_add(cursor, body_length, "glyph body end")
            data[cursor:end] = body
            # bytearray starts as zero, so [end:end + padding] is the required
            # all-zero slot padding.
            cursor = _checked_add(cursor, SLOT_SIZE, "glyph slot end")
        if cursor != data_length:
            raise ValueError(f"strike {strike_id}: internal slot size mismatch")
        result.append(_NormalizedStrike(geometry, compact, bytes(data)))

    result.sort(key=lambda item: item.geometry.id)
    return result


def _pack_header(
    strike_count: int,
    payload_offset: int,
    payload_length: int,
    total_file_size: int,
    payload_crc32: int,
    header_crc32: int,
) -> bytes:
    _check_uint(strike_count, 8, "strike count")
    for value, name in (
        (payload_offset, "payload offset"),
        (payload_length, "payload length"),
        (total_file_size, "total file size"),
        (payload_crc32, "payload CRC32"),
        (header_crc32, "header CRC32"),
    ):
        _check_uint(value, 32, name)
    return struct.pack(
        _HEADER_FORMAT,
        MAGIC,
        VERSION,
        HEADER_SIZE,
        ENDIAN_MARKER,
        FIRST_CODEPOINT,
        GLYPH_COUNT,
        strike_count,
        0,
        DIRECTORY_ENTRY_SIZE,
        HEADER_SIZE,
        payload_offset,
        payload_length,
        total_file_size,
        payload_crc32,
        header_crc32,
        b"\x00" * 16,
    )


def build_container(strikes: Any) -> bytes:
    """Build a deterministic external-font container in memory.

    ``strikes`` may be a mapping ``{10: glyph_bodies, ...}``, a sequence of
    ``StrikeInput``/mapping objects, or ``(id, glyph_bodies)`` pairs.  Strike
    order is canonicalized by id, so equivalent input order produces identical
    bytes.
    """

    normalized = _normalize_strikes(strikes)
    strike_count = len(normalized)
    directory_length = _checked_mul(
        strike_count, DIRECTORY_ENTRY_SIZE, "directory length"
    )
    directory_end = _checked_add(HEADER_SIZE, directory_length, "directory end")
    # The generated layout has a zero-filled gap from the directory to the
    # first 512-byte-aligned strike.  With at most three entries this is 512.
    payload_offset = _align_up(directory_end, PAYLOAD_ALIGNMENT, "payload offset")
    strike_data_length = _checked_mul(GLYPH_COUNT, SLOT_SIZE, "strike data length")
    payload_length = _checked_mul(strike_count, strike_data_length, "payload length")
    total_file_size = _checked_add(payload_offset, payload_length, "total file size")

    raw = bytearray(total_file_size)
    entries: list[tuple[StrikeGeometry, int, int, int]] = []
    cursor = payload_offset
    for item in normalized:
        if cursor % PAYLOAD_ALIGNMENT != 0:
            raise ValueError("internal strike data offset is not 512-byte aligned")
        end = _checked_add(cursor, len(item.data), "strike data end")
        if len(item.data) != strike_data_length:
            raise ValueError("internal strike data length mismatch")
        raw[cursor:end] = item.data
        entries.append((item.geometry, cursor, len(item.data), _crc32(item.data)))
        cursor = end
    if cursor != total_file_size:
        raise ValueError("internal payload length mismatch")

    payload_crc32 = _crc32(bytes(raw[payload_offset:total_file_size]))
    for index, (geometry, data_offset, data_length, strike_crc32) in enumerate(entries):
        directory_offset = _checked_add(
            HEADER_SIZE,
            _checked_mul(index, DIRECTORY_ENTRY_SIZE, "directory entry offset"),
            "directory entry offset",
        )
        packed = struct.pack(
            _DIRECTORY_FORMAT,
            geometry.id,
            geometry.width,
            geometry.height,
            geometry.bytes_per_column,
            geometry.advance,
            0,
            SLOT_SIZE,
            GLYPH_COUNT,
            0,
            data_offset,
            data_length,
            strike_crc32,
            b"\x00" * 8,
        )
        raw[directory_offset : directory_offset + DIRECTORY_ENTRY_SIZE] = packed

    header_without_crc = _pack_header(
        strike_count,
        payload_offset,
        payload_length,
        total_file_size,
        payload_crc32,
        0,
    )
    header_crc32 = _crc32(header_without_crc)
    header = _pack_header(
        strike_count,
        payload_offset,
        payload_length,
        total_file_size,
        payload_crc32,
        header_crc32,
    )
    raw[:HEADER_SIZE] = header
    return bytes(raw)


# ---------------------------------------------------------------------------
# Strict parser / validator


def _validate_header(raw: bytes) -> tuple[tuple[Any, ...], int, int]:
    if len(raw) < HEADER_SIZE:
        raise ValueError("truncated header")
    if len(raw) > U32_MAX:
        raise ValueError("file is larger than the 32-bit container limit")
    try:
        fields = struct.unpack_from(_HEADER_FORMAT, raw, 0)
    except struct.error as exc:
        raise ValueError("truncated header") from exc

    (
        magic,
        version,
        header_size,
        endian_marker,
        first_codepoint,
        glyph_count,
        strike_count,
        flags,
        dir_entry_size,
        dir_offset,
        payload_offset,
        payload_length,
        total_file_size,
        payload_crc32,
        header_crc32,
        reserved,
    ) = fields
    if magic != MAGIC:
        raise ValueError("invalid magic")
    if version != VERSION:
        raise ValueError("unsupported version")
    if header_size != HEADER_SIZE:
        raise ValueError("invalid header size")
    if endian_marker != ENDIAN_MARKER:
        raise ValueError("invalid endian marker")
    if first_codepoint != FIRST_CODEPOINT:
        raise ValueError("invalid first codepoint")
    if glyph_count != GLYPH_COUNT:
        raise ValueError("invalid glyph count")
    if strike_count < 1 or strike_count > MAX_STRIKE_COUNT:
        raise ValueError("invalid strike count")
    if flags != 0:
        raise ValueError("header flags must be zero")
    if dir_entry_size != DIRECTORY_ENTRY_SIZE:
        raise ValueError("invalid directory entry size")
    if dir_offset != HEADER_SIZE:
        raise ValueError("invalid directory offset")
    if reserved != b"\x00" * 16:
        raise ValueError("header reserved bytes must be zero")

    # The CRC is over all 64 header bytes with only this field cleared.
    header_for_crc = bytearray(raw[:HEADER_SIZE])
    header_for_crc[44:48] = b"\x00" * 4
    if _crc32(bytes(header_for_crc)) != header_crc32:
        raise ValueError("header CRC32 mismatch")

    if total_file_size != len(raw):
        if total_file_size < len(raw):
            raise ValueError("file has trailing data")
        raise ValueError("truncated file")
    if total_file_size < HEADER_SIZE:
        raise ValueError("total file size is smaller than the header")

    directory_length = _checked_mul(
        strike_count, DIRECTORY_ENTRY_SIZE, "directory length"
    )
    directory_end = _checked_add(dir_offset, directory_length, "directory end")
    if directory_end > total_file_size:
        raise ValueError("directory is outside the file")
    if payload_offset < directory_end:
        raise ValueError("payload overlaps the directory")
    payload_end = _checked_add(payload_offset, payload_length, "payload end")
    if payload_end != total_file_size:
        raise ValueError("payload length does not match total file size")
    expected_payload_offset = _align_up(
        directory_end, PAYLOAD_ALIGNMENT, "payload offset"
    )
    if payload_offset != expected_payload_offset:
        raise ValueError("payload offset does not match directory alignment")
    if raw[directory_end:payload_offset] != b"\x00" * (payload_offset - directory_end):
        raise ValueError("nonzero directory alignment padding")
    expected_strike_length = _checked_mul(
        GLYPH_COUNT, SLOT_SIZE, "strike data length"
    )
    expected_payload_length = _checked_mul(
        strike_count, expected_strike_length, "payload length"
    )
    if payload_length != expected_payload_length:
        raise ValueError("payload length does not match strike count")
    return fields, directory_end, payload_end


def _validate_directory(
    raw: bytes,
    fields: tuple[Any, ...],
    directory_end: int,
    payload_end: int,
) -> tuple[DirectoryEntry, ...]:
    strike_count = fields[6]
    payload_offset = fields[10]
    entries: list[DirectoryEntry] = []
    regions: list[tuple[int, int, int]] = []
    seen_ids: set[int] = set()
    seen_geometries: set[tuple[int, int, int, int]] = set()
    expected_data_length = _checked_mul(GLYPH_COUNT, SLOT_SIZE, "strike data length")

    for index in range(strike_count):
        entry_offset = _checked_add(
            HEADER_SIZE,
            _checked_mul(index, DIRECTORY_ENTRY_SIZE, "directory entry offset"),
            "directory entry offset",
        )
        if _checked_add(entry_offset, DIRECTORY_ENTRY_SIZE, "directory entry end") > len(raw):
            raise ValueError("truncated directory entry")
        try:
            (
                strike_id,
                width,
                height,
                bytes_per_column,
                advance,
                flags,
                slot_size,
                glyph_count,
                reserved,
                data_offset,
                data_length,
                strike_crc32,
                tail_reserved,
            ) = struct.unpack_from(_DIRECTORY_FORMAT, raw, entry_offset)
        except struct.error as exc:
            raise ValueError("truncated directory entry") from exc

        geometry = GEOMETRIES.get(strike_id)
        if geometry is None:
            raise ValueError(f"unknown strike geometry id: {strike_id}")
        if strike_id in seen_ids:
            raise ValueError("duplicate strike id")
        seen_ids.add(strike_id)
        geometry_key = (width, height, bytes_per_column, advance)
        if geometry_key in seen_geometries:
            raise ValueError("duplicate strike geometry")
        seen_geometries.add(geometry_key)
        if (
            width != geometry.width
            or height != geometry.height
            or bytes_per_column != geometry.bytes_per_column
            or advance != geometry.advance
        ):
            raise ValueError(f"invalid geometry for strike {strike_id}")
        if flags != 0:
            raise ValueError("directory flags must be zero")
        if slot_size != SLOT_SIZE:
            raise ValueError("invalid slot size")
        if glyph_count != GLYPH_COUNT:
            raise ValueError("invalid directory glyph count")
        if reserved != 0 or tail_reserved != b"\x00" * 8:
            raise ValueError("directory reserved fields must be zero")
        if data_offset % PAYLOAD_ALIGNMENT != 0:
            raise ValueError("strike data offset is not 512-byte aligned")
        if data_offset < payload_offset:
            raise ValueError("strike data starts before payload")
        if data_length != expected_data_length:
            raise ValueError("invalid strike data length")
        data_end = _checked_add(data_offset, data_length, "strike data end")
        if data_end > payload_end:
            raise ValueError("strike data is outside payload")
        if data_offset < directory_end:
            raise ValueError("strike data overlaps directory")
        entry = DirectoryEntry(
            id=strike_id,
            width=width,
            height=height,
            bytes_per_column=bytes_per_column,
            advance=advance,
            flags=flags,
            slot_size=slot_size,
            glyph_count=glyph_count,
            data_offset=data_offset,
            data_length=data_length,
            strike_crc32=strike_crc32,
        )
        entries.append(entry)
        regions.append((data_offset, data_end, index))

    sorted_regions = sorted(regions)
    previous_end: Optional[int] = None
    for start, end, _ in sorted_regions:
        if previous_end is not None and start < previous_end:
            raise ValueError("strike data regions overlap")
        if previous_end is None and start != payload_offset:
            raise ValueError("first strike does not start at payload offset")
        if previous_end is not None and start != previous_end:
            raise ValueError("strike data regions are not contiguous")
        previous_end = end
    if previous_end != payload_end:
        # This catches bytes appended after the last strike even when the
        # header's total/payload fields have been forged consistently.
        raise ValueError("payload contains trailing data after the strikes")

    # Validate every slot before accepting the container.  Only the bytes
    # beyond the compact body are padding; unused bits in the final bitmap byte
    # are intentionally left opaque to this container layer.
    for entry in entries:
        body_length = _checked_mul(
            entry.width, entry.bytes_per_column, "compact glyph body length"
        )
        if body_length > SLOT_SIZE:
            raise ValueError("compact body is larger than a slot")
        for glyph_index in range(GLYPH_COUNT):
            relative = _checked_mul(glyph_index, SLOT_SIZE, "glyph slot index")
            start = _checked_add(entry.data_offset, relative, "glyph slot offset")
            slot_end = _checked_add(start, SLOT_SIZE, "glyph slot end")
            if slot_end > len(raw):
                raise ValueError("glyph slot is outside the file")
            padding_start = _checked_add(start, body_length, "slot padding offset")
            if raw[padding_start:slot_end] != b"\x00" * (SLOT_SIZE - body_length):
                raise ValueError("nonzero slot padding")
        actual_crc = _crc32(raw[entry.data_offset : entry.data_offset + entry.data_length])
        if actual_crc != entry.strike_crc32:
            raise ValueError(f"strike {entry.id} CRC32 mismatch")

    payload_offset = fields[10]
    actual_payload_crc = _crc32(raw[payload_offset:payload_end])
    if actual_payload_crc != fields[13]:
        raise ValueError("payload CRC32 mismatch")
    return tuple(entries)


def read_container(source: Any) -> ExternalFont:
    """Read and strictly validate a path, bytes object, or binary stream."""

    raw = _as_bytes(source)
    fields, directory_end, payload_end = _validate_header(raw)
    entries = _validate_directory(raw, fields, directory_end, payload_end)
    return ExternalFont(
        data=raw,
        entries=entries,
        payload_offset=fields[10],
        payload_length=fields[11],
        total_file_size=fields[12],
        payload_crc32=fields[13],
        header_crc32=fields[14],
    )


# ---------------------------------------------------------------------------
# Strict validation command line interface
def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    subparsers = parser.add_subparsers(dest="command")

    validate_parser = subparsers.add_parser("validate", help="strictly validate FILE")
    validate_parser.add_argument("file", type=Path)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    if args.command == "validate":
        container = read_container(args.file)
        ids = ",".join(str(entry.id) for entry in container.entries)
        print(
            f"valid {args.file}: strikes={ids} "
            f"glyphs={GLYPH_COUNT} bytes={container.total_file_size}"
        )
        return 0
    parser.error("a command is required: validate")
    return 2  # pragma: no cover - argparse.error exits


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, TypeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
