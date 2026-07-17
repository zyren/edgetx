import random
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

try:
    from . import external_font
except ImportError:  # Direct execution from tools/cn_fonts.
    import external_font


def synthetic_glyphs(strike, seed):
    geometry = external_font.GEOMETRIES[strike]
    body_length = geometry.width * geometry.bytes_per_column
    return tuple(
        bytes((seed + glyph_index * 17 + byte_index * 29) & 0xFF
              for byte_index in range(body_length))
        for glyph_index in range(external_font.GLYPH_COUNT)
    )


def refresh_header_crc(data):
    data = bytearray(data)
    data[44:48] = b"\0" * 4
    struct.pack_into("<I", data, 44, zlib.crc32(data[:64]) & 0xFFFFFFFF)
    return data


def refresh_all_data_crcs(data):
    data = bytearray(data)
    for index in range(data[20]):
        entry_offset = external_font.HEADER_SIZE + index * external_font.DIRECTORY_ENTRY_SIZE
        data_offset, data_length = struct.unpack_from("<II", data, entry_offset + 12)
        crc = zlib.crc32(data[data_offset:data_offset + data_length]) & 0xFFFFFFFF
        struct.pack_into("<I", data, entry_offset + 20, crc)
    payload_offset, payload_length = struct.unpack_from("<II", data, 28)
    payload_crc = zlib.crc32(data[payload_offset:payload_offset + payload_length]) & 0xFFFFFFFF
    struct.pack_into("<I", data, 40, payload_crc)
    return refresh_header_crc(data)


class ExternalFontTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.glyphs = {
            10: synthetic_glyphs(10, 3),
            12: synthetic_glyphs(12, 7),
            16: synthetic_glyphs(16, 11),
        }
        cls.data = external_font.build_container(cls.glyphs)
        cls.font = external_font.read_container(cls.data)

    def assertRejected(self, data, pattern=None):
        context = self.assertRaisesRegex(ValueError, pattern) if pattern else self.assertRaises(ValueError)
        with context:
            external_font.read_container(data)

    def test_byte_identical_reproducibility_and_canonical_order(self):
        reverse = {16: self.glyphs[16], 12: self.glyphs[12], 10: self.glyphs[10]}
        self.assertEqual(self.data, external_font.build_container(reverse))
        self.assertEqual([entry.id for entry in self.font.entries], [10, 12, 16])

    def test_header_total_size_offsets_and_directory_contract(self):
        fields = struct.unpack_from("<8sHHIHHBBHIIIIII16s", self.data)
        self.assertEqual(fields[0], b"ETXCNF\0\0")
        self.assertEqual(fields[1:6], (1, 64, 0x12345678, 0x4E00, 20992))
        self.assertEqual(fields[6:10], (3, 0, 32, 64))
        strike_length = external_font.GLYPH_COUNT * external_font.SLOT_SIZE
        self.assertEqual(fields[10], 512)
        self.assertEqual(fields[11], 3 * strike_length)
        self.assertEqual(fields[12], len(self.data))
        self.assertEqual(fields[12], 512 + 3 * strike_length)
        self.assertEqual(fields[15], b"\0" * 16)

        for index, expected_id in enumerate((10, 12, 16)):
            offset = 64 + index * 32
            entry = struct.unpack_from("<6BHHHIII8s", self.data, offset)
            geometry = external_font.GEOMETRIES[expected_id]
            self.assertEqual(entry[:6], (
                expected_id, geometry.width, geometry.height,
                geometry.bytes_per_column, geometry.advance, 0))
            self.assertEqual(entry[6:9], (32, 20992, 0))
            self.assertEqual(entry[9], 512 + index * strike_length)
            self.assertEqual(entry[9] % 512, 0)
            self.assertEqual(entry[10], strike_length)
            self.assertEqual(entry[12], b"\0" * 8)

    def test_three_strike_roundtrip_slots_and_compact_bitmaps(self):
        codepoints = [0x4E00, 0x4E01, 0x6ABC, 0x9FFE, 0x9FFF]
        for strike in (10, 12, 16):
            body_length = external_font.GEOMETRIES[strike].body_length
            for codepoint in codepoints:
                index = codepoint - external_font.FIRST_CODEPOINT
                expected = self.glyphs[strike][index]
                with self.subTest(strike=strike, codepoint=hex(codepoint)):
                    self.assertEqual(self.font.bitmap(strike, codepoint), expected)
                    entry = next(entry for entry in self.font.entries if entry.id == strike)
                    start = entry.data_offset + index * external_font.SLOT_SIZE
                    slot = self.data[start:start + external_font.SLOT_SIZE]
                    self.assertEqual(len(slot), 32)
                    self.assertEqual(slot[:body_length], expected)
                    self.assertEqual(slot[body_length:], b"\0" * (32 - body_length))

    def test_single_10px_strike_is_allowed(self):
        data = external_font.build_container({10: self.glyphs[10]})
        font = external_font.read_container(data)
        self.assertEqual([entry.id for entry in font.entries], [10])
        self.assertEqual(font.total_file_size, 512 + 20992 * 32)
        self.assertEqual(font.bitmap(10, 0x4E00), self.glyphs[10][0])

    def test_deterministic_random_and_boundary_codepoints(self):
        rng = random.Random(0x47583132)
        codepoints = [0x4E00, 0x9FFF]
        codepoints.extend(rng.randrange(0x4E00, 0xA000) for _ in range(25))
        for codepoint in codepoints:
            index = codepoint - 0x4E00
            strike = rng.choice((10, 12, 16))
            self.assertEqual(self.font.bitmap(strike, codepoint), self.glyphs[strike][index])
        for bad in (0x4DFF, 0xA000, -1, True, "U+4E00"):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                self.font.bitmap(10, bad)

    def test_truncation_magic_version_endian_reserved_and_trailing_rejected(self):
        cases = []
        cases.append((self.data[:63], "truncated"))
        cases.append((self.data[:-1], "truncated"))
        cases.append((self.data + b"x", "trailing"))
        bad = bytearray(self.data); bad[0] ^= 1; cases.append((bad, "magic"))
        bad = bytearray(self.data); struct.pack_into("<H", bad, 8, 2); cases.append((bad, "version"))
        bad = bytearray(self.data); struct.pack_into("<I", bad, 12, 0x78563412); cases.append((bad, "endian"))
        bad = bytearray(self.data); bad[48] = 1; cases.append((bad, "reserved"))
        bad = bytearray(self.data); bad[64 + 24] = 1; cases.append((bad, "reserved"))
        for data, pattern in cases:
            with self.subTest(pattern=pattern):
                self.assertRejected(data, pattern)

    def test_duplicate_unknown_and_invalid_geometry_rejected(self):
        bad = bytearray(self.data)
        bad[64 + 32] = 10
        self.assertRejected(bad, "duplicate")

        bad = bytearray(self.data)
        bad[64] = 8
        self.assertRejected(bad, "unknown")

        for byte_offset in (1, 2, 3, 4):
            bad = bytearray(self.data)
            bad[64 + byte_offset] ^= 1
            with self.subTest(byte_offset=byte_offset):
                self.assertRejected(bad, "geometry")

    def test_misaligned_overlap_length_out_of_bounds_and_overflow_rejected(self):
        first_data_offset = struct.unpack_from("<I", self.data, 64 + 12)[0]

        bad = bytearray(self.data)
        struct.pack_into("<I", bad, 64 + 12, first_data_offset + 1)
        self.assertRejected(bad, "aligned")

        bad = bytearray(self.data)
        struct.pack_into("<I", bad, 64 + 32 + 12, first_data_offset)
        self.assertRejected(bad, "overlap")

        bad = bytearray(self.data)
        struct.pack_into("<I", bad, 64 + 16, 20992 * 32 - 1)
        self.assertRejected(bad, "length")

        bad = bytearray(self.data)
        struct.pack_into("<I", bad, 64 + 12, 0xFFFFFE00)
        self.assertRejected(bad, "overflow")

        bad = bytearray(self.data)
        struct.pack_into("<II", bad, 28, 0xFFFFFF00, 0x1000)
        bad = refresh_header_crc(bad)
        self.assertRejected(bad, "overflow")

    def test_header_crc_uses_zeroed_header_crc_field(self):
        stored = struct.unpack_from("<I", self.data, 44)[0]
        header = bytearray(self.data[:64])
        header[44:48] = b"\0" * 4
        self.assertEqual(stored, zlib.crc32(header) & 0xFFFFFFFF)
        self.assertNotEqual(stored, zlib.crc32(self.data[:64]) & 0xFFFFFFFF)

        bad = bytearray(self.data)
        struct.pack_into("<I", bad, 44, zlib.crc32(self.data[:64]) & 0xFFFFFFFF)
        self.assertRejected(bad, "header CRC32")

    def test_payload_strike_crc_and_slot_padding_rejected(self):
        entry = self.font.entries[0]

        bad = bytearray(self.data)
        bad[entry.data_offset] ^= 1
        self.assertRejected(bad, "CRC32")

        bad = bytearray(self.data)
        bad[entry.data_offset + entry.body_length] = 1
        bad = refresh_all_data_crcs(bad)
        self.assertRejected(bad, "padding")

        bad = bytearray(self.data)
        struct.pack_into("<I", bad, 40, self.font.payload_crc32 ^ 1)
        bad = refresh_header_crc(bad)
        self.assertRejected(bad, "payload CRC32")

    def test_input_count_length_duplicate_and_unknown_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "glyph count"):
            external_font.build_container({10: self.glyphs[10][:-1]})
        wrong = list(self.glyphs[10]); wrong[0] = b"short"
        with self.assertRaisesRegex(ValueError, "expected 20 bytes"):
            external_font.build_container({10: wrong})
        with self.assertRaisesRegex(ValueError, "unknown"):
            external_font.build_container({8: self.glyphs[10]})
        with self.assertRaisesRegex(ValueError, "duplicate"):
            external_font.build_container([
                external_font.StrikeInput(10, self.glyphs[10]),
                external_font.StrikeInput(10, self.glyphs[10]),
            ])

    def test_cli_strict_validate(self):
        script = Path(external_font.__file__)
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            valid_path = directory / "font.etxcnf"
            valid_path.write_bytes(self.data)
            result = subprocess.run(
                [sys.executable, str(script), "validate", str(valid_path)],
                text=True, capture_output=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("valid", result.stdout)

            invalid_path = directory / "bad.etxcnf"
            invalid_path.write_bytes(self.data[:-1])
            result = subprocess.run(
                [sys.executable, str(script), "validate", str(invalid_path)],
                text=True, capture_output=True, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("error:", result.stderr)

if __name__ == "__main__":
    unittest.main()
