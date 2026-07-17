#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fonts/external_font.h"

namespace external_font {

struct Reader {
  void * context;
  uint32_t (*size)(void * context);
  bool (*seek)(void * context, uint32_t offset);
  bool (*read)(void * context, void * data, uint32_t length);
  bool (*createLinkMap)(void * context);
  void (*close)(void * context);
};

using ProgressCallback = bool (*)(void * context, uint32_t scannedBytes,
                                  uint32_t totalBytes);

class Manager {
 public:
  ExternalFontResult prepare(const Reader & reader);
  ExternalFontResult verifyFull(const Reader & reader,
                                ProgressCallback callback = nullptr,
                                void * context = nullptr);
  bool activate();
  void shutdown();
  bool available(uint8_t strike) const;
  uint8_t advance(uint8_t strike) const;
  bool readGlyph(uint8_t strike, uint16_t codepoint, uint8_t out[32]);

 private:
  struct Strike {
    uint32_t offset;
    uint32_t length;
    uint32_t crc;
    uint8_t id;
    uint8_t bodyLength;
    uint8_t advance;
  };
  struct CacheEntry {
    uint8_t data[32];
    uint16_t codepoint;
    uint32_t age;
    uint8_t strike;
    bool valid;
  };

  const Strike * findStrike(uint8_t strike) const;
  void clearState();

  Reader reader_{};
  Strike strikes_[3]{};
  CacheEntry cache_[16]{};
  uint8_t strikeCount_ = 0;
  uint32_t clock_ = 0;
  uint32_t fileSize_ = 0;
  uint32_t payloadOffset_ = 0;
  uint32_t payloadLength_ = 0;
  uint32_t payloadCrc_ = 0;
  enum class State : uint8_t { Closed, Preparing, Prepared, Ready };
  State state_ = State::Closed;
};

}  // namespace external_font
