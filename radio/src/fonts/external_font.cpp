#include "fonts/external_font.h"
#include "fonts/external_font_priv.h"

#include <string.h>

#include "ff.h"
#include "memory_sections.h"

namespace external_font {
namespace {

constexpr uint32_t kHeaderSize = 64;
constexpr uint32_t kDirectorySize = 32;
constexpr uint32_t kSlotSize = 32;
constexpr uint32_t kGlyphCount = 20992;
constexpr uint32_t kStrikeLength = kGlyphCount * kSlotSize;
constexpr uint16_t kFirstCodepoint = 0x4E00;
constexpr uint16_t kLastCodepoint = 0x9FFF;
constexpr uint32_t kAlignment = 512;
constexpr uint32_t kLinkMapWords = 64;

uint8_t ioBuffer[512] __DMA;

uint16_t get16(const uint8_t * p)
{
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

uint32_t get32(const uint8_t * p)
{
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

bool zero(const uint8_t * p, size_t length)
{
  while (length--)
    if (*p++ != 0) return false;
  return true;
}

uint32_t crcUpdate(uint32_t crc, const uint8_t * data, size_t length)
{
  while (length--) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc;
}

bool addOk(uint32_t a, uint32_t b, uint32_t & result)
{
  result = a + b;
  return result >= a;
}

bool multiplyOk(uint32_t a, uint32_t b, uint32_t & result)
{
  if (a && b > 0xFFFFFFFFu / a) return false;
  result = a * b;
  return true;
}

bool geometry(uint8_t id, uint8_t width, uint8_t height, uint8_t columns,
              uint8_t advance, uint8_t & bodyLength)
{
  if (columns != 2) return false;
  if ((id == 10 && width == 10 && height == 10 && advance == 11) ||
      (id == 12 && width == 12 && height == 12 && advance == 13) ||
      (id == 16 && width == 16 && height == 16 && advance == 17)) {
    bodyLength = width * columns;
    return true;
  }
  return false;
}

}  // namespace

void Manager::clearState()
{
  memset(strikes_, 0, sizeof(strikes_));
  memset(cache_, 0, sizeof(cache_));
  strikeCount_ = 0;
  clock_ = 0;
  fileSize_ = 0;
  payloadOffset_ = 0;
  payloadLength_ = 0;
  payloadCrc_ = 0;
  state_ = State::Closed;
  reader_ = {};
}

void Manager::shutdown()
{
  if (state_ != State::Closed && reader_.close) reader_.close(reader_.context);
  clearState();
}

const Manager::Strike * Manager::findStrike(uint8_t strike) const
{
  for (uint8_t i = 0; i < strikeCount_; ++i)
    if (strikes_[i].id == strike) return &strikes_[i];
  return nullptr;
}

ExternalFontResult Manager::prepare(const Reader & reader)
{
  shutdown();
  reader_ = reader;
  state_ = State::Preparing;
  auto unavailable = [this]() { shutdown(); return ExternalFontResult::Unavailable; };
  if (!reader_.size || !reader_.seek || !reader_.read ||
      !reader_.createLinkMap || !reader_.close)
    return unavailable();

  fileSize_ = reader_.size(reader_.context);
  if (fileSize_ < kHeaderSize || !reader_.seek(reader_.context, 0) ||
      !reader_.read(reader_.context, ioBuffer, kHeaderSize))
    return unavailable();

  const uint8_t magic[8] = {'E','T','X','C','N','F',0,0};
  if (memcmp(ioBuffer, magic, sizeof(magic)) != 0 || get16(ioBuffer + 8) != 1 ||
      get16(ioBuffer + 10) != kHeaderSize || get32(ioBuffer + 12) != 0x12345678u ||
      get16(ioBuffer + 16) != kFirstCodepoint || get16(ioBuffer + 18) != kGlyphCount)
    return unavailable();

  strikeCount_ = ioBuffer[20];
  const uint32_t dirOffset = get32(ioBuffer + 24);
  payloadOffset_ = get32(ioBuffer + 28);
  payloadLength_ = get32(ioBuffer + 32);
  const uint32_t declaredSize = get32(ioBuffer + 36);
  payloadCrc_ = get32(ioBuffer + 40);
  const uint32_t headerCrc = get32(ioBuffer + 44);
  if (strikeCount_ < 1 || strikeCount_ > 3 || ioBuffer[21] != 0 ||
      get16(ioBuffer + 22) != kDirectorySize || dirOffset != kHeaderSize ||
      !zero(ioBuffer + 48, 16) || declaredSize != fileSize_)
    return unavailable();

  uint8_t headerCopy[kHeaderSize];
  memcpy(headerCopy, ioBuffer, sizeof(headerCopy));
  memset(headerCopy + 44, 0, 4);
  if ((crcUpdate(0xFFFFFFFFu, headerCopy, sizeof(headerCopy)) ^ 0xFFFFFFFFu) != headerCrc)
    return unavailable();

  const uint32_t directoryLength = uint32_t(strikeCount_) * kDirectorySize;
  uint32_t directoryEnd;
  uint32_t payloadEnd;
  const uint32_t expectedPayloadOffset =
      (kHeaderSize + directoryLength + kAlignment - 1) & ~(kAlignment - 1);
  if (!addOk(dirOffset, directoryLength, directoryEnd) ||
      !addOk(payloadOffset_, payloadLength_, payloadEnd) ||
      payloadOffset_ != expectedPayloadOffset || payloadOffset_ < directoryEnd ||
      payloadLength_ != uint32_t(strikeCount_) * kStrikeLength || payloadEnd != fileSize_)
    return unavailable();

  if (!reader_.seek(reader_.context, dirOffset) ||
      !reader_.read(reader_.context, ioBuffer, directoryLength))
    return unavailable();
  for (uint8_t i = 0; i < strikeCount_; ++i) {
    const uint8_t * p = ioBuffer + uint32_t(i) * kDirectorySize;
    Strike & strike = strikes_[i];
    strike.id = p[0];
    strike.advance = p[4];
    if (!geometry(p[0], p[1], p[2], p[3], p[4], strike.bodyLength) ||
        p[5] != 0 || get16(p + 6) != kSlotSize || get16(p + 8) != kGlyphCount ||
        get16(p + 10) != 0 || !zero(p + 24, 8))
      return unavailable();
    strike.offset = get32(p + 12);
    strike.length = get32(p + 16);
    strike.crc = get32(p + 20);
    uint32_t strikeEnd;
    if ((strike.offset % kAlignment) != 0 || strike.length != kStrikeLength ||
        !addOk(strike.offset, strike.length, strikeEnd) || strikeEnd > payloadEnd)
      return unavailable();
    for (uint8_t j = 0; j < i; ++j)
      if (strikes_[j].id == strike.id) return unavailable();
  }

  for (uint32_t pos = directoryEnd; pos < payloadOffset_;) {
    const uint32_t amount = payloadOffset_ - pos > sizeof(ioBuffer) ?
                            sizeof(ioBuffer) : payloadOffset_ - pos;
    if (!reader_.seek(reader_.context, pos) || !reader_.read(reader_.context, ioBuffer, amount) ||
        !zero(ioBuffer, amount)) return unavailable();
    pos += amount;
  }

  uint8_t order[3] = {0, 1, 2};
  for (uint8_t i = 0; i < strikeCount_; ++i)
    for (uint8_t j = i + 1; j < strikeCount_; ++j)
      if (strikes_[order[j]].offset < strikes_[order[i]].offset) {
        uint8_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
      }
  uint32_t expected = payloadOffset_;
  for (uint8_t i = 0; i < strikeCount_; ++i)
    if (strikes_[order[i]].offset != expected ||
        !addOk(expected, strikes_[order[i]].length, expected)) return unavailable();
  if (expected != payloadEnd) return unavailable();

  memset(cache_, 0, sizeof(cache_));
  state_ = State::Prepared;
  return ExternalFontResult::Prepared;
}

ExternalFontResult Manager::verifyFull(const Reader & reader,
                                       ProgressCallback callback, void * context)
{
  const ExternalFontResult prepared = prepare(reader);
  if (prepared != ExternalFontResult::Prepared) return prepared;
  auto unavailable = [this]() { shutdown(); return ExternalFontResult::Unavailable; };
  auto cancelled = [this]() { shutdown(); return ExternalFontResult::Cancelled; };
  if (!reader_.seek(reader_.context, payloadOffset_)) return unavailable();

  uint8_t order[3] = {0, 1, 2};
  for (uint8_t i = 0; i < strikeCount_; ++i)
    for (uint8_t j = i + 1; j < strikeCount_; ++j)
      if (strikes_[order[j]].offset < strikes_[order[i]].offset) {
        uint8_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
      }
  const uint32_t payloadEnd = payloadOffset_ + payloadLength_;
  uint32_t payloadRunning = 0xFFFFFFFFu;
  uint32_t strikeRunning = 0xFFFFFFFFu;
  uint8_t current = 0;
  for (uint32_t pos = payloadOffset_; pos < payloadEnd;) {
    Strike & strike = strikes_[order[current]];
    const uint32_t remaining = strike.offset + strike.length - pos;
    const uint32_t amount = remaining > sizeof(ioBuffer) ? sizeof(ioBuffer) : remaining;
    const uint32_t scanned = pos - payloadOffset_;
    if (callback && !callback(context, scanned, payloadLength_)) return cancelled();
    if (!reader_.read(reader_.context, ioBuffer, amount)) return unavailable();
    payloadRunning = crcUpdate(payloadRunning, ioBuffer, amount);
    strikeRunning = crcUpdate(strikeRunning, ioBuffer, amount);
    for (uint32_t slot = 0; slot < amount; slot += kSlotSize)
      if (!zero(ioBuffer + slot + strike.bodyLength, kSlotSize - strike.bodyLength))
        return unavailable();
    pos += amount;
    if (callback && !callback(context, pos - payloadOffset_, payloadLength_)) return cancelled();
    if (pos == strike.offset + strike.length) {
      if ((strikeRunning ^ 0xFFFFFFFFu) != strike.crc) return unavailable();
      strikeRunning = 0xFFFFFFFFu;
      ++current;
    }
  }
  if ((payloadRunning ^ 0xFFFFFFFFu) != payloadCrc_) return unavailable();
  return ExternalFontResult::Prepared;
}

bool Manager::activate()
{
  if (state_ != State::Prepared || !reader_.createLinkMap(reader_.context)) {
    if (state_ != State::Closed) shutdown();
    return false;
  }
  state_ = State::Ready;
  return true;
}

bool Manager::available(uint8_t strike) const
{
  return state_ == State::Ready && findStrike(strike) != nullptr;
}

uint8_t Manager::advance(uint8_t strike) const
{
  const Strike * found = state_ == State::Ready ? findStrike(strike) : nullptr;
  return found ? found->advance : 0;
}

bool Manager::readGlyph(uint8_t strike, uint16_t codepoint, uint8_t out[32])
{
  if (!out) return false;
  memset(out, 0, kSlotSize);
  const Strike * found = state_ == State::Ready ? findStrike(strike) : nullptr;
  if (!found || codepoint < kFirstCodepoint || codepoint > kLastCodepoint) return false;
  for (auto & entry : cache_) {
    if (entry.valid && entry.strike == strike && entry.codepoint == codepoint) {
      entry.age = ++clock_;
      memcpy(out, entry.data, kSlotSize);
      return true;
    }
  }
  uint32_t glyphOffset, offset, end, strikeEnd;
  if (!multiplyOk(uint32_t(codepoint - kFirstCodepoint), kSlotSize, glyphOffset) ||
      !addOk(found->offset, glyphOffset, offset) || !addOk(offset, kSlotSize, end) ||
      !addOk(found->offset, found->length, strikeEnd) || end > strikeEnd || end > fileSize_) {
    return false;
  }
  if (!reader_.seek(reader_.context, offset) || !reader_.read(reader_.context, out, kSlotSize)) {
    shutdown();
    memset(out, 0, kSlotSize);
    return false;
  }
  if (!zero(out + found->bodyLength, kSlotSize - found->bodyLength)) {
    memset(out, 0, kSlotSize);
    return false;
  }
  CacheEntry * victim = &cache_[0];
  for (auto & entry : cache_) {
    if (!entry.valid) { victim = &entry; break; }
    if (entry.age < victim->age) victim = &entry;
  }
  memcpy(victim->data, out, kSlotSize);
  victim->codepoint = codepoint;
  victim->strike = strike;
  victim->age = ++clock_;
  victim->valid = true;
  return true;
}

}  // namespace external_font

namespace {
static_assert(sizeof(FSIZE_t) <= sizeof(uint32_t),
              "ETXCNF offsets require a FatFs FSIZE_t no wider than 32 bits");
FIL fontFile;
DWORD clusterMap[64];
external_font::Manager manager;

uint32_t fatSize(void * context) { return uint32_t(f_size(static_cast<FIL *>(context))); }
bool fatSeek(void * context, uint32_t offset) { return f_lseek(static_cast<FIL *>(context), offset) == FR_OK; }
bool fatRead(void * context, void * data, uint32_t length)
{
  UINT read = 0;
  return f_read(static_cast<FIL *>(context), data, UINT(length), &read) == FR_OK && read == length;
}
bool fatLinkMap(void * context)
{
  FIL * file = static_cast<FIL *>(context);
#if FF_USE_FASTSEEK
  clusterMap[0] = external_font::kLinkMapWords;
  file->cltbl = clusterMap;
  return f_lseek(file, CREATE_LINKMAP) == FR_OK;
#else
  (void)file;
  return false;
#endif
}
void fatClose(void * context) { f_close(static_cast<FIL *>(context)); }
}

ExternalFontResult externalFontPrepare(const char * path)
{
  externalFontShutdown();
  if (!path || f_open(&fontFile, path, FA_OPEN_EXISTING | FA_READ) != FR_OK)
    return ExternalFontResult::Unavailable;
  const external_font::Reader reader = {&fontFile, fatSize, fatSeek, fatRead, fatLinkMap, fatClose};
  return manager.prepare(reader);
}

ExternalFontResult externalFontVerifyFull(const char * path,
                                          ExternalFontProgressCallback callback,
                                          void * context)
{
  externalFontShutdown();
  if (!path || f_open(&fontFile, path, FA_OPEN_EXISTING | FA_READ) != FR_OK)
    return ExternalFontResult::Unavailable;
  const external_font::Reader reader = {&fontFile, fatSize, fatSeek, fatRead, fatLinkMap, fatClose};
  return manager.verifyFull(reader, callback, context);
}

bool externalFontActivate() { return manager.activate(); }
void externalFontShutdown() { manager.shutdown(); }
bool externalFontAvailable(uint8_t strike) { return manager.available(strike); }
uint8_t externalFontAdvance(uint8_t strike) { return manager.advance(strike); }
bool externalFontReadGlyph(uint8_t strike, uint16_t codepoint, uint8_t out[32])
{
  return manager.readGlyph(strike, codepoint, out);
}
