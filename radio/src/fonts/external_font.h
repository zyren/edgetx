#pragma once

#include <stdint.h>

enum class ExternalFontResult : uint8_t {
  Prepared,
  Unavailable,
  Cancelled,
};

typedef bool (*ExternalFontProgressCallback)(void * context,
                                             uint32_t scannedBytes,
                                             uint32_t totalBytes);

ExternalFontResult externalFontPrepare(const char * path = "/FONTS/CN_BASIC.FNT");
ExternalFontResult externalFontVerifyFull(
    const char * path = "/FONTS/CN_BASIC.FNT",
    ExternalFontProgressCallback callback = nullptr, void * context = nullptr);
bool externalFontActivate();
void externalFontShutdown();
bool externalFontAvailable(uint8_t strike);
uint8_t externalFontAdvance(uint8_t strike);
bool externalFontReadGlyph(uint8_t strike, uint16_t codepoint, uint8_t out[32]);
