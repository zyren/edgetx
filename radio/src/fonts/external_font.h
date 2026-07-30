#pragma once

#include <stdint.h>

enum class ExternalFontResult : uint8_t {
  Prepared,
  Unavailable,
};

constexpr const char EXTERNAL_FONT_DEFAULT_PATH[] = "/FONTS/CN_BASIC.FNT";

ExternalFontResult externalFontPrepare(const char * path = EXTERNAL_FONT_DEFAULT_PATH);
bool externalFontActivate();
void externalFontShutdown();
bool externalFontReadGlyph(uint8_t strike, uint16_t codepoint, uint8_t out[32]);
