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
  put32(data, 84, crc);
  put32(data, 40, crc);
  fixHeaderCrc(data);
}

std::vector<uint8_t> makeFont()
{
  std::vector<uint8_t> data(kPayloadOffset + kStrikeLength, 0);
  const uint8_t magic[8] = {'E','T','X','C','N','F',0,0};
  memcpy(data.data(), magic, 8);
  put16(data, 8, 1); put16(data, 10, 64); put32(data, 12, 0x12345678);
  put16(data, 16, 0x4E00); put16(data, 18, 20992);
  data[20] = 1; put16(data, 22, 32); put32(data, 24, 64);
  put32(data, 28, kPayloadOffset); put32(data, 32, kStrikeLength);
  put32(data, 36, uint32_t(data.size()));
  data[64] = 10; data[65] = 10; data[66] = 10; data[67] = 2; data[68] = 11;
  put16(data, 70, 32); put16(data, 72, 20992);
  put32(data, 76, kPayloadOffset); put32(data, 80, kStrikeLength);
  for (uint32_t glyph = 0; glyph < 20992; ++glyph)
    for (uint32_t byte = 0; byte < 20; ++byte)
      data[kPayloadOffset + glyph * 32 + byte] = uint8_t(glyph + byte);
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
  bool linkMap = true;
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
  static bool map(void * p)
  {
    auto & self = *static_cast<MemoryReader *>(p);
    ++self.linkMapCalls;
    return self.linkMap;
  }
  static void close(void * p)
  {
    auto & self = *static_cast<MemoryReader *>(p);
    self.closed = true;
    ++self.closeCalls;
  }
  external_font::Reader reader() { return {this, size, seek, read, map, close}; }
};

struct ProgressCapture { bool cancel = false; std::vector<uint32_t> values; };

bool progress(void * context, uint32_t scanned, uint32_t)
{
  auto & capture = *static_cast<ProgressCapture *>(context);
  capture.values.push_back(scanned);
  return !capture.cancel || scanned == 0;
}

TEST(ExternalFont, PrepareAndActivateDoNotReadPayload)
{
  MemoryReader source;
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  EXPECT_EQ(0u, source.payloadReads);
  ASSERT_TRUE(manager.activate());
  EXPECT_EQ(0u, source.payloadReads);
  EXPECT_TRUE(manager.available(10));
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

TEST(ExternalFont, BadPayloadCrcIsOnlyRejectedByFullVerify)
{
  MemoryReader source;
  source.data[40] ^= 1;
  fixHeaderCrc(source.data);
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  ASSERT_TRUE(manager.activate());
  EXPECT_EQ(ExternalFontResult::Unavailable, manager.verifyFull(source.reader()));
  EXPECT_TRUE(source.closed);
}

TEST(ExternalFont, FullVerifyChecksPayloadStrikePaddingAndCancellation)
{
  for (int kind = 0; kind < 3; ++kind) {
    MemoryReader source;
    if (kind == 0) source.data[84] ^= 1;
    if (kind == 1) { source.data[kPayloadOffset + 20] = 1; fixPayloadCrcs(source.data); }
    if (kind == 2) source.data[40] ^= 1;
    external_font::Manager manager;
    EXPECT_EQ(ExternalFontResult::Unavailable, manager.verifyFull(source.reader())) << kind;
    EXPECT_TRUE(source.closed);
  }
  MemoryReader source;
  external_font::Manager manager;
  ProgressCapture capture; capture.cancel = true;
  EXPECT_EQ(ExternalFontResult::Cancelled,
            manager.verifyFull(source.reader(), progress, &capture));
  EXPECT_EQ(1u, source.closeCalls);
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
  MemoryReader source; source.linkMap = false;
  external_font::Manager manager;
  ASSERT_EQ(ExternalFontResult::Prepared, manager.prepare(source.reader()));
  EXPECT_FALSE(manager.activate());
  EXPECT_EQ(0u, source.payloadReads);
  EXPECT_TRUE(source.closed);
}

}  // namespace

#endif  // EDGETX_CN_STDLCD
