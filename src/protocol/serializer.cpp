#include "serializer.hpp"

#include <cstdint>
#include <cstring>
#include <endian.h> // For htobe64 (big-endian conversion for 64-bit values)
#include <netinet/in.h>

namespace corekv {

void appendToBuffer(std::vector<uint8_t> &dest, const uint8_t *data, int size) {
  dest.insert(dest.end(), data, data + size);
}

void removeFromBuffer(std::vector<uint8_t> &src, int size) {
  if (size >= static_cast<int>(src.size())) {
    src.clear();
  } else {
    src.erase(src.begin(), src.begin() + size);
  }
}

void reservePrefixHeader(std::vector<uint8_t> &dest, uint32_t &headerIdx) {
  headerIdx = dest.size();
  uint32_t header = 0;
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(&header), 4);
}

void readUint32(std::vector<uint8_t> &src, uint32_t &dest) {
  if (src.size() < 4) {
    dest = 0;
    return;
  }
  std::memcpy(&dest, src.data(), 4);
  dest = ntohl(dest);
}

void readMessage(std::vector<uint8_t> &src, std::string &msg, uint32_t size) {
  if (size > src.size()) {
    size = static_cast<uint32_t>(src.size());
  }
  msg.assign(src.begin(), src.begin() + size);
}

void parseCommands(std::vector<uint8_t> &src, std::vector<std::string> &cmds,
                   int totalSize) {
  int processed = 0;
  while (processed < totalSize) {
    uint32_t cmdSize;
    readUint32(src, cmdSize);
    removeFromBuffer(src, 4);
    processed += 4;

    std::string cmd;
    readMessage(src, cmd, cmdSize);
    cmds.push_back(cmd);
    removeFromBuffer(src, cmdSize);
    processed += static_cast<int>(cmdSize);
  }
}

void generateResponse(std::vector<uint8_t> &dest, const std::string &res) {
  uint32_t len = htonl(static_cast<uint32_t>(res.size()));
  auto *head = reinterpret_cast<uint8_t *>(&len);

  appendToBuffer(dest, head, 4);
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(res.data()),
                 (int)res.size());
}

void addTagNil(std::vector<uint8_t> &dest) {

  uint8_t tag = TAG_NIL;
  appendToBuffer(dest, &tag, sizeof(tag));
}

void addTagStr(std::vector<uint8_t> &dest, const uint8_t *data, int size) {
  uint8_t tag = TAG_STR;
  appendToBuffer(dest, &tag, sizeof(tag));

  uint32_t size_be = htonl(static_cast<uint32_t>(size));
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(&size_be), 4);
  appendToBuffer(dest, data, size);
}

void addTagErr(std::vector<uint8_t> &dest, const std::string msg) {
  uint8_t tag = TAG_ERR;
  appendToBuffer(dest, &tag, sizeof(tag));

  uint32_t size_be = htonl(static_cast<uint32_t>(msg.size()));
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(&size_be), 4);
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(msg.data()),
                 msg.size());
}

void addTagInt(std::vector<uint8_t> &dest, int val) {
  uint8_t tag = TAG_INT;
  appendToBuffer(dest, &tag, sizeof(tag));
  // Convert to network byte order (big-endian) for cross-platform compatibility
  uint32_t val_be = htonl(static_cast<uint32_t>(val));
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(&val_be), 4);
}

void addTagLong(std::vector<uint8_t> &dest, long val) {
  uint8_t tag = TAG_LONG;
  appendToBuffer(dest, &tag, sizeof(tag));
  // Convert 64-bit value to big-endian for network byte order
  uint64_t val_be = htobe64(static_cast<uint64_t>(val));
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(&val_be), 8);
}

void addTagDbl(std::vector<uint8_t> &dest, double val) {
  uint8_t tag = TAG_DBL;
  appendToBuffer(dest, &tag, sizeof(tag));
  // Convert double to big-endian byte order for network transmission
  uint64_t val_be = htobe64(static_cast<uint64_t>(val));
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(&val_be), 8);
}

void addTagArr(std::vector<uint8_t> &dest, uint32_t len) {
  uint8_t tag = TAG_ARR;
  appendToBuffer(dest, &tag, sizeof(tag));

  // Store the array element count in network byte order
  // Note: This is the count of elements, not total bytes
  uint32_t len_be = htonl(len);
  appendToBuffer(dest, reinterpret_cast<const uint8_t *>(&len_be), 4);
}

} // namespace corekv
