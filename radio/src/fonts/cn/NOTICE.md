# GX12 Chinese font assets

The 8, 10, and 12 pixel source subsets are lossless records extracted from
TakWolf Fusion Pixel Font release 2026.07.01, copyright (c) 2022 TakWolf.
The 16 pixel U+25A1 source record is a lossless record extracted from TakWolf
Ark Pixel Font release 2026.07.01, Copyright (c) 2021, TakWolf
(https://takwolf.com). Its true 16x16
body is expanded to 32x32 by exact horizontal and vertical 2x replication.

Fusion Pixel Font, Ark Pixel Font, their checked-in subsets, and generated
bitmap derivatives are distributed under SIL OFL-1.1. The complete shared
license text and Fusion copyright notice are in `LICENSE-FUSION-OFL-1.1.txt`;
this NOTICE supplies the corresponding Ark source and copyright attribution.

Pinned URLs and complete full/subset SHA-256 checksums are in
`tools/cn_fonts/manifest.json`. Normal generation is offline and consumes only
the checked-in subsets. The generated C++ is intentionally not integrated into
the firmware in Phase 1.
