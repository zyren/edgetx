import io
import tempfile
import unittest
from pathlib import Path
import zipfile

try:
    from . import external_font, package_release
except ImportError:
    import external_font, package_release


def glyphs(size, seed):
    length = external_font.GEOMETRIES[size].body_length
    return tuple(bytes([seed]) * length for _ in range(external_font.GLYPH_COUNT))


class PackageReleaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.font = external_font.build_container({
            10: glyphs(10, 1), 12: glyphs(12, 2), 16: glyphs(16, 3)})
        cls.release = {"size": len(cls.font), "sha256": package_release.sha256(cls.font)}
        cls.members = {
            "firmware.bin": b"firmware\0",
            "CN_BASIC.FNT": cls.font,
            "NOTICE.md": b"notice\n",
            "licenses/example.txt": b"license\n",
            package_release.INSTALL_NAME: package_release.installation_text("firmware.bin"),
        }

    def test_archive_is_byte_identical_sorted_and_metadata_fixed(self):
        first = package_release.build_archive(self.font, self.release, self.members)
        self.assertEqual(first, package_release.build_archive(self.font, self.release, self.members))
        with zipfile.ZipFile(io.BytesIO(first)) as archive:
            expected = sorted((*self.members, package_release.CHECKSUM_NAME))
            self.assertEqual(archive.namelist(), expected)
            for info in archive.infolist():
                self.assertEqual(info.date_time, package_release.FIXED_TIME)
                self.assertEqual(info.compress_type, zipfile.ZIP_STORED)
                self.assertEqual(info.external_attr >> 16, 0o100644)
            sums = archive.read(package_release.CHECKSUM_NAME).decode("utf-8").splitlines()
            self.assertEqual(len(sums), len(self.members))
            self.assertEqual(sums, [
                f"{package_release.sha256(self.members[name])}  {name}"
                for name in sorted(self.members)])

    def test_release_size_hash_and_font_member_are_enforced(self):
        for release, members, pattern in (
                ({**self.release, "size": 1}, self.members, "size mismatch"),
                ({**self.release, "sha256": "0" * 64}, self.members, "SHA256 mismatch"),
                (self.release, {**self.members, "CN_BASIC.FNT": b"bad"}, "different")):
            with self.subTest(pattern=pattern), self.assertRaisesRegex(ValueError, pattern):
                package_release.build_archive(self.font, release, members)

    def test_release_members_include_firmware_font_instructions_and_exact_licenses(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            firmware = root / "gx12.bin"
            font = root / "font.fnt"
            firmware.write_bytes(b"firmware")
            font.write_bytes(self.font)
            assets = root / "assets"
            assets.mkdir()
            with self.assertRaisesRegex(ValueError, "missing"):
                package_release.release_members(firmware, font, assets)
            (assets / "NOTICE.md").write_text("notice\n", encoding="utf-8")
            (assets / "licenses").mkdir()
            with self.assertRaisesRegex(ValueError, "missing licenses"):
                package_release.release_members(firmware, font, assets)
            license_data = b"exact\r\nlicense\0"
            for name in package_release.REQUIRED_LICENSES:
                (assets / "licenses" / name).write_bytes(license_data + name.encode("ascii"))
            members = package_release.release_members(firmware, font, assets)
            self.assertEqual(members["gx12.bin"], b"firmware")
            self.assertEqual(members["CN_BASIC.FNT"], self.font)
            for name in package_release.REQUIRED_LICENSES:
                self.assertEqual(members[f"licenses/{name}"], license_data + name.encode("ascii"))
            instructions = members[package_release.INSTALL_NAME].decode("utf-8")
            self.assertIn("/FONTS/CN_BASIC.FNT", instructions)
            self.assertIn("/FIRMWARE/", instructions)
            self.assertIn("gx12.bin", instructions)

    def test_missing_or_invalid_firmware_and_font_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = root / "assets"
            (assets / "licenses").mkdir(parents=True)
            (assets / "NOTICE.md").write_bytes(b"notice")
            for name in package_release.REQUIRED_LICENSES:
                (assets / "licenses" / name).write_bytes(b"license")
            firmware = root / "gx12.bin"
            font = root / "font.fnt"
            with self.assertRaisesRegex(ValueError, "firmware must be a regular"):
                package_release.release_members(firmware, font, assets)
            firmware.write_bytes(b"")
            with self.assertRaisesRegex(ValueError, "firmware must be non-empty"):
                package_release.release_members(firmware, font, assets)
            firmware.write_bytes(b"firmware")
            with self.assertRaisesRegex(ValueError, "font must be a regular"):
                package_release.release_members(firmware, font, assets)


if __name__ == "__main__":
    unittest.main()
