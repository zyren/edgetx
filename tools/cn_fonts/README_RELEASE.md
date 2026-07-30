# GX12 CN release packaging

Generate the font separately, then package an existing verified GX12 firmware
and font with the notices and deterministic checksums:

```powershell
python tools/cn_fonts/package_release.py `
  EdgeTX-GX12-CN-v2.12.2-cn-release.bin `
  build/cn_fonts/CN_BASIC.FNT `
  build/release/EdgeTX-GX12-CN-v2.12.2.zip
```

The packager validates `CN_BASIC.FNT` against the size, SHA-256, and container
contract in `manifest.json`. It requires a non-empty regular `.bin` file and
all notice/license inputs. ZIP members are sorted and stored without
compression using a fixed timestamp and file mode. `SHA256SUMS.txt` covers
every other archive member, including the firmware, font, installation guide,
notice, and exact license bytes.
