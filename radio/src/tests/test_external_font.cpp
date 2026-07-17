#include <gtest/gtest.h>

#if defined(EDGETX_CN_STDLCD)

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "fonts/external_font_priv.h"

namespace {

constexpr uint32_t kPayloadOffset = 512;
constexpr uint32_t kStrikeLength = 20992u * 32u;

uint32_t crc32(const uint8_t * data, size_t length)
{
  uint32_t crc = 0xFFFFFFFFu;
  while (length--) {
    crc ^= *data++;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc ^ 0xFFFFFFFFu;
}

void put16(std::vector<uint8_t> & data, size_t offset, uint16_t value)
{
  data[offset] = uint8_t(value); data[offset + 1] = uint8_t(value >> 8);
}

void put32(std::vector<uint8_t> & data, size_t offset, uint32_t value)
{
  for (int i = 0; i < 4; ++i) data[offset + i] = uint8_t(value >> (8 * i));
}

void fixHeaderCrc(std::vector<uint8_t> & data)
{
  put32(data, 44, 0);
  put32(data, 44, crc32(data.data(), 64));
}

void fixPayloadCrcs(std::vector<uint8_t> & data)
{
  const uint32_t crc = crc32(data.data() + kPayloadOffset, data.size() - kPayloadOffset);
  put32(data, 40, crc);
  for (size_t strike = 0; strike < 3; ++strike) {
    const size_t payload = kPayloadOffset + strike * kStrikeLength;
    put32(data, 64 + strike * 32 + 20,
          crc32(data.data() + payload, kStrikeLength));
  }
  fixHeaderCrc(data);
}

std::vector<uint8_t> makeFont()
{
  std::vector<uint8_t> data(kPayloadOffset + 3 * kStrikeLength, 0);
  const uint8_t magic[8] = {'E','T','X','C','N','F',0,0};
  memcpy(data.data(), magic, 8);
  put16(data, 8, 1); put16(data, 10, 64); put32(data, 12, 0x12345678);
  put16(data, 16, 0x4E00); put16(data, 18, 20992);
  data[20] = 3; put16(data, 22, 32); put32(data, 24, 64);
  put32(data, 28, kPayloadOffset); put32(data, 32, 3 * kStrikeLength);
  put32(data, 36, uint32_t(data.size()));
  const uint8_t ids[] = {10, 12, 16};
  for (size_t strike = 0; strike < 3; ++strike) {
    const size_t entry = 64 + strike * 32;
    data[entry] = ids[strike];
    data[entry + 1] = ids[strike];
    data[entry + 2] = ids[strike];
    data[entry + 3] = 2;
    data[entry + 4] = ids[strike] + 1;
    put16(data, entry + 6, 32); put16(data, entry + 8, 20992);
    put32(data, entry + 12, kPayloadOffset + uint32_t(strike) * kStrikeLength);
    put32(data, entry + 16, kStrikeLength);
    for (uint32_t glyph = 0; glyph < 20992; ++glyph)
      for (uint32_t byte = 0; byte < uint32_t(ids[strike]) * 2; ++byte)
        data[kPayloadOffset + strike * kStrikeLength + glyph * 32 + byte] =
            uint8_t(strike * 0x40 + glyph + byte);
  }
  fixPayloadCrcs(data);
  return data;
}

struct MemoryReader {
  std::vector<uint8_t> data = makeFont();
  uint32_t position = 0;
  unsigned reads = 0;
  unsigned payloadReads = 0;
  unsigned linkMapCalls = 0;
  unsigned closeCalls = 0;
  bool failReads = false;
  FRESULT linkMapResult = FR_OK;
  bool closed = false;

  static uint32_t size(void * p) { return uint32_t(static_cast<MemoryReader *>(p)->data.size()); }
  static bool seek(void * p, uint32_t offset)
  {
    auto & self = *static_cast<MemoryReader *>(p);
    if (offset > self.data.size()) return false;
    self.position = offset;
    return true;
  }
  static bool read(void * p, void * out, uint32_t length)
  {
    auto & self = *static_cast<MemoryReader *>(p);
    ++self.reads;
    if (self.position >= kPayloadOffset) ++self.payloadReads;
    if (self.failReads || length > self.data.size() - self.position) return false;
    memcpy(out, self.data.data() + self.position, length);
    self.position += length;
    return true;
  }
  static FRESULT map(void * p)
  {
    auto & self = *static_cast<MemoryReader *>(p);
    ++self.linkMapCalls;
    return self.linkMapResult;
  }
  static void close(void * p)
  {
    auto & self = *static_cast<MemoryReader *>(p);
    self.closed = true;
    ++self.closeCalls;
  }
  external_font::Reader reader() { return {this, size, seek, read, map, close}; }
};

TEST(ExternalFont, ThreeStrikePrepareDefersPayloadAndGlyphsReadAfterActivate)
{
  MemoryReader source;
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  EXPECT_EQ(0u, source.payloadReads);
  ASSERT_TRUE(manager.activate());
  EXPECT_EQ(0u, source.payloadReads);
  const uint8_t strikes[] = {10, 12, 16};
  for (uint8_t strike : strikes) {
    EXPECT_TRUE(manager.available(strike));
    uint8_t glyph[32];
    ASSERT_TRUE(manager.readGlyph(strike, 0x4E00, glyph));
    const size_t index = strike == 10 ? 0 : strike == 12 ? 1 : 2;
    EXPECT_EQ(uint8_t(index * 0x40), glyph[0]);
    EXPECT_TRUE(std::all_of(glyph + strike * 2, glyph + 32,
                            [](uint8_t byte) { return byte == 0; }));
  }
  EXPECT_EQ(3u, source.payloadReads);
}

TEST(ExternalFont, PreparedFontIsUnreadableUntilActivated)
{
  MemoryReader source;
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  uint8_t glyph[32]; memset(glyph, 0xA5, sizeof(glyph));
  EXPECT_FALSE(manager.readGlyph(10, 0x4E00, glyph));
  EXPECT_TRUE(std::all_of(glyph, glyph + 32, [](uint8_t b) { return b == 0; }));
  ASSERT_TRUE(manager.activate());
  EXPECT_TRUE(manager.readGlyph(10, 0x4E00, glyph));
}

TEST(ExternalFont, PrepareRejectsMetadataErrorsAndCloses)
{
  for (int kind = 0; kind < 6; ++kind) {
    MemoryReader source;
    if (kind == 0) source.data[0] = 'X';
    if (kind == 1) source.data[44] ^= 1;
    if (kind == 2) put32(source.data, 36, 1);
    if (kind == 3) source.data[88] = 1;
    if (kind == 4) put32(source.data, 76, 513);
    if (kind == 5) put32(source.data, 80, 0xFFFFFFFFu);
    if (kind != 1) fixHeaderCrc(source.data);
    external_font::Manager manager;
    EXPECT_EQ(ExternalFontResult::Unavailable, manager.prepare(source.reader())) << kind;
    EXPECT_TRUE(source.closed) << kind;
  }
  MemoryReader shortRead;
  shortRead.failReads = true;
  external_font::Manager manager;
  EXPECT_EQ(ExternalFontResult::Unavailable, manager.prepare(shortRead.reader()));
  EXPECT_TRUE(shortRead.closed);
}

TEST(ExternalFont, GlyphIoFailureClearsOutputAndShutsDown)
{
  MemoryReader source;
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  ASSERT_TRUE(manager.activate());
  source.failReads = true;
  uint8_t glyph[32]; memset(glyph, 0xA5, sizeof(glyph));
  EXPECT_FALSE(manager.readGlyph(10, 0x4E00, glyph));
  EXPECT_TRUE(std::all_of(glyph, glyph + 32, [](uint8_t b) { return b == 0; }));
  EXPECT_TRUE(source.closed);
  EXPECT_FALSE(manager.available(10));
}

TEST(ExternalFont, GlyphPaddingFailureClearsOutputAndFallsBack)
{
  MemoryReader source;
  source.data[kPayloadOffset + 20] = 1;
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  ASSERT_TRUE(manager.activate());
  uint8_t glyph[32]; memset(glyph, 0xA5, sizeof(glyph));
  EXPECT_FALSE(manager.readGlyph(10, 0x4E00, glyph));
  EXPECT_TRUE(std::all_of(glyph, glyph + 32, [](uint8_t b) { return b == 0; }));
  EXPECT_TRUE(manager.available(10));
}

TEST(ExternalFont, ActivationFailureClosesPreparedFileWithoutPayloadRead)
{
  MemoryReader source; source.linkMapResult = FR_DISK_ERR;
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  EXPECT_FALSE(manager.activate());
  EXPECT_EQ(0u, source.payloadReads);
  EXPECT_TRUE(source.closed);
}

TEST(ExternalFont, LinkMapOutOfMemoryFallsBackToNormalSeeking)
{
  MemoryReader source; source.linkMapResult = FR_NOT_ENOUGH_CORE;
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  EXPECT_TRUE(manager.activate());
  EXPECT_FALSE(source.closed);
  uint8_t glyph[32];
  EXPECT_TRUE(manager.readGlyph(10, 0x4E00, glyph));
}

}  // namespace

#endif  // EDGETX_CN_STDLCD
