import gzip
import io
import json
import tarfile
import unittest
import zipfile

try:
    from . import external_font, generate
except ImportError:
    import external_font, generate


def one_pixel(size, x, y):
    body = [[0] * size for _ in range(size)]
    body[y][x] = 1
    return body


def tiny_bdf(size, glyphs, reverse=False):
    items = list(glyphs.items())
    if reverse:
        items.reverse()
    records = []
    storage_bits = ((size + 7) // 8) * 8
    row_chars = storage_bits // 4
    for codepoint, body in items:
        rows = []
        for row in body:
            value = sum(1 << (storage_bits - 1 - x) for x, bit in enumerate(row) if bit)
            rows.append(f"{value:0{row_chars}X}")
        records.append(
            f"STARTCHAR u{codepoint:04X}\n"
            f"ENCODING {codepoint}\n"
            "SWIDTH 1000 0\n"
            f"DWIDTH {size} 0\n"
            f"BBX {size} {size} 0 0\n"
            "BITMAP\n" + "\n".join(rows) + "\nENDCHAR\n"
        )
    return (
        "STARTFONT 2.1\n"
        "STARTPROPERTIES 2\n"
        f"FONT_ASCENT {size}\n"
        "FONT_DESCENT 0\n"
        "ENDPROPERTIES\n"
        f"CHARS {len(records)}\n" + "".join(records) + "ENDFONT\n"
    ).encode("ascii")


def source_spec(size, name):
    return {
        "kind": "bdf-gz",
        "archive": name + ".bdf.gz",
        "archive_sha256": "0" * 64,
        "member_sha256": "1" * 64,
        "source_size": size,
    }


def test_config():
    return {
        "format": "CN_BASIC.FNT",
        "first_codepoint": "U+4E00",
        "last_codepoint": "U+9FFF",
        "glyph_count": 20992,
        "slot_size": 32,
        "alignment": 512,
        "column_major": True,
        "bits_per_pixel": 1,
        "sources": {
            "p10": source_spec(10, "p10"),
            "p12": source_spec(12, "p12"),
            "p16": source_spec(16, "p16"),
            "s16": source_spec(16, "s16"),
        },
        "strikes": {
            "10": {"id": 10, "width": 10, "height": 10, "priority": ["p10", "s16"], "transforms": {"s16": "center-nearest"}},
            "12": {"id": 12, "width": 12, "height": 12, "priority": ["p12", "s16"], "transforms": {"s16": "center-nearest"}},
            "16": {"id": 16, "width": 16, "height": 16, "priority": ["p16", "s16"], "transforms": {}},
        },
    }


def test_sources(reverse=False):
    fallback = generate.FALLBACK
    secondary = {
        fallback: one_pixel(16, 15, 15),
        0x4E00: one_pixel(16, 14, 14),
        0x4E01: one_pixel(16, 8, 8),
        0x9FFF: one_pixel(16, 15, 0),
    }
    source_items = [
        ("p10", tiny_bdf(10, {fallback: one_pixel(10, 0, 0), 0x4E00: one_pixel(10, 1, 1)}, reverse)),
        ("p12", tiny_bdf(12, {fallback: one_pixel(12, 0, 0), 0x4E00: one_pixel(12, 2, 2)}, reverse)),
        ("p16", tiny_bdf(16, {fallback: one_pixel(16, 0, 0), 0x4E00: one_pixel(16, 3, 3)}, reverse)),
        ("s16", tiny_bdf(16, secondary, reverse)),
    ]
    if reverse:
        source_items.reverse()
    return dict(source_items)


class ExternalGenerationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = test_config()
        cls.sources = test_sources()
        cls.result = generate.build_external_font(cls.config, cls.sources)
        cls.font = external_font.read_container(cls.result.data)

    def test_pinned_manifest_sources_and_priorities(self):
        manifest = json.loads(generate.MANIFEST.read_text(encoding="utf-8"))
        external = generate.validate_external_manifest(manifest)
        for name, pin in generate.EXTERNAL_SOURCE_PINS.items():
            for field, value in pin.items():
                self.assertEqual(external["sources"][name][field], value)
        self.assertEqual(external["strikes"]["16"]["priority"], ["wqy_bold", "unifont17"])
        self.assertEqual(external["strikes"]["12"]["priority"], ["fusion12", "wqy_medium", "unifont17"])
        self.assertEqual(external["strikes"]["10"]["priority"], ["fusion10", "wqy9_medium", "unifont17"])

    def test_priority_missing_fallback_and_boundary_mapping(self):
        for size, primary in ((10, "p10"), (12, "p12"), (16, "p16")):
            with self.subTest(size=size):
                expected_primary = generate.pack(
                    one_pixel(size, {10: 1, 12: 2, 16: 3}[size], {10: 1, 12: 2, 16: 3}[size]),
                    size, size, 0)
                self.assertEqual(self.font.bitmap(size, 0x4E00), expected_primary)

                native_last = one_pixel(16, 15, 0)
                expected_last = native_last if size == 16 else generate.nearest_neighbor(native_last, size)
                self.assertEqual(self.font.bitmap(size, 0x9FFF), generate.pack(expected_last, size, size, 0))

                expected_fallback = generate.pack(one_pixel(size, 0, 0), size, size, 0)
                self.assertEqual(self.font.bitmap(size, 0x4E02), expected_fallback)
                self.assertEqual(self.result.source_counts[size][primary], 1)
                self.assertEqual(self.result.source_counts[size]["s16"], 2)
                self.assertEqual(self.result.source_counts[size]["fallback"], 20989)
                self.assertEqual(sum(self.result.source_counts[size].values()), 20992)

    def test_nearest_neighbor_is_deterministic_and_center_sampled(self):
        body = [[(x + y * 3) & 1 for x in range(16)] for y in range(16)]
        for target in (10, 12):
            first = generate.nearest_neighbor(body, target)
            self.assertEqual(first, generate.nearest_neighbor(body, target))
            expected = [[
                body[((2 * y + 1) * 16) // (2 * target)][((2 * x + 1) * 16) // (2 * target)]
                for x in range(target)] for y in range(target)]
            self.assertEqual(first, expected)
        body12 = [[(x * 2 + y) % 3 == 0 for x in range(12)] for y in range(12)]
        expected10 = [[body12[((2 * y + 1) * 12) // 20][((2 * x + 1) * 12) // 20]
                       for x in range(10)] for y in range(10)]
        self.assertEqual(generate.nearest_neighbor(body12, 10), expected10)

    def test_build_pipeline_center_scales_12px_source_to_10px(self):
        config = test_config()
        config["sources"]["s12"] = source_spec(12, "s12")
        config["strikes"]["10"]["priority"] = ["p10", "s12", "s16"]
        config["strikes"]["10"]["transforms"] = {"s12": "center-nearest", "s16": "center-nearest"}
        native = one_pixel(12, 6, 6)
        sources = {**self.sources, "s12": tiny_bdf(12, {generate.FALLBACK: one_pixel(12, 0, 0), 0x4E01: native})}
        result = generate.build_external_font(config, sources)
        font = external_font.read_container(result.data)
        expected = generate.nearest_neighbor(native, 10)
        self.assertEqual(font.bitmap(10, 0x4E01), generate.pack(expected, 10, 10, 0))
        self.assertEqual(result.source_counts[10]["s12"], 1)

    def test_controlled_embolden_exact_rule_and_boundary_protection(self):
        body = [[0] * 5 for _ in range(3)]
        body[0][0] = 1
        body[0][2] = 1                 # cavity x=1 is protected; x=2 extends into x=3
        body[1][3] = 1                 # extends into x=4, never outside the row
        body[2][0] = 1                 # boundary candidate at x=1 is allowed
        expected = [[1, 0, 1, 1, 0], [0, 0, 0, 1, 1], [1, 1, 0, 0, 0]]
        self.assertEqual(generate.controlled_horizontal_embolden(body), expected)

    def test_metric_crop_preserves_bottom_row_and_rejects_outside_pixels(self):
        source = {
            **source_spec(12, "metric"),
            "metric_crop": {"canvas": [12, 14], "rect": [0, 1, 12, 12], "require_zero_clipped": True},
        }
        glyph = generate.Glyph(0x4E00, (12, 0), (12, 11, 0, -1), [0] * 10 + [0x10], b"")
        self.assertEqual(generate.external_native_raster(glyph, 12, 3, source)[-1][-1], 1)
        bad = {**source, "metric_crop": {"canvas": [12, 14], "rect": [0, 0, 12, 12], "require_zero_clipped": True}}
        with self.assertRaisesRegex(ValueError, "metric crop removed"):
            generate.external_native_raster(glyph, 12, 3, bad)

    def test_manifest_transform_scope_is_strike_specific(self):
        manifest = json.loads(generate.MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(manifest["external_font"]["strikes"]["16"]["transforms"], {"unifont17": "controlled-horizontal-embolden"})
        self.assertNotIn("controlled-horizontal-embolden", manifest["external_font"]["strikes"]["10"]["transforms"].values())
        self.assertNotIn("controlled-horizontal-embolden", manifest["external_font"]["strikes"]["12"]["transforms"].values())

    def test_embolden_applies_only_to_selected_16px_secondary(self):
        config = test_config()
        config["strikes"]["16"]["transforms"] = {"s16": "controlled-horizontal-embolden"}
        result = generate.build_external_font(config, self.sources)
        font = external_font.read_container(result.data)

        secondary16 = one_pixel(16, 8, 8)
        expected_emboldened = generate.controlled_horizontal_embolden(secondary16)
        self.assertEqual(font.bitmap(16, 0x4E01), generate.pack(expected_emboldened, 16, 16, 0))

        primary16 = one_pixel(16, 3, 3)
        self.assertEqual(font.bitmap(16, 0x4E00), generate.pack(primary16, 16, 16, 0))
        self.assertNotEqual(font.bitmap(16, 0x4E00), generate.pack(generate.controlled_horizontal_embolden(primary16), 16, 16, 0))

        for size in (10, 12):
            expected = generate.nearest_neighbor(secondary16, size)
            self.assertEqual(font.bitmap(size, 0x4E01), generate.pack(expected, size, size, 0))
            self.assertNotEqual(font.bitmap(size, 0x4E01), generate.pack(generate.controlled_horizontal_embolden(expected), size, size, 0))

    def test_all_body_counts_lengths_high_bits_and_strict_roundtrip(self):
        self.assertEqual(set(self.result.bodies), {10, 12, 16})
        for size, bodies in self.result.bodies.items():
            blocks = (size + 7) // 8
            self.assertEqual(len(bodies), 20992)
            self.assertTrue(all(len(body) == size * blocks for body in bodies))
            if size % 8:
                high_mask = (0xFF << (size % 8)) & 0xFF
                self.assertTrue(all(
                    body[x * blocks + blocks - 1] & high_mask == 0
                    for body in bodies for x in range(size)))
        self.assertEqual([entry.id for entry in self.font.entries], [10, 12, 16])
        self.assertEqual(self.result.sha256, generate.sha256(self.result.data))

    def test_source_and_bdf_record_order_do_not_change_output(self):
        reordered = generate.build_external_font(self.config, test_sources(reverse=True))
        self.assertEqual(self.result.data, reordered.data)
        self.assertEqual(self.result.source_counts, reordered.source_counts)

    def test_hash_pinned_gzip_zip_and_tar_extraction(self):
        bdf = tiny_bdf(10, {generate.FALLBACK: one_pixel(10, 0, 0)})
        archives = []

        gz = gzip.compress(bdf, mtime=0)
        archives.append((gz, {"kind": "bdf-gz"}))

        stream = io.BytesIO()
        with zipfile.ZipFile(stream, "w", zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("font.bdf", bdf)
        archives.append((stream.getvalue(), {"kind": "bdf-zip", "member": "font.bdf"}))

        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode="w:gz") as archive:
            info = tarfile.TarInfo("font.bdf"); info.size = len(bdf)
            archive.addfile(info, io.BytesIO(bdf))
        archives.append((stream.getvalue(), {"kind": "bdf-tar-gz", "member": "font.bdf"}))

        for blob, source in archives:
            source = {
                **source,
                "archive_sha256": generate.sha256(blob),
                "member_sha256": generate.sha256(bdf),
            }
            with self.subTest(kind=source["kind"]):
                self.assertEqual(generate.external_extract_bdf(blob, source), bdf)
                corrupt = bytearray(blob); corrupt[-1] ^= 1
                with self.assertRaisesRegex(ValueError, "archive SHA256"):
                    generate.external_extract_bdf(bytes(corrupt), source)

        tar_blob, tar_source = archives[-1]
        sized = {**tar_source, "archive_sha256": generate.sha256(tar_blob),
                 "member_sha256": generate.sha256(bdf), "member_size": len(bdf)}
        self.assertEqual(generate.external_extract_bdf(tar_blob, sized), bdf)
        with self.assertRaisesRegex(ValueError, "member size"):
            generate.external_extract_bdf(tar_blob, {**sized, "member_size": len(bdf) + 1})


if __name__ == "__main__":
    unittest.main()
