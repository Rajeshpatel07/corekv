#include "serializer.hpp"

#include <cstring>
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

void parseCommands(std::vector<uint8_t> &src, std::vector<std::string> &cmds, int totalSize) {
  int processed = 0;
  while (processed < totalSize && src.size() >= 4) {
    uint32_t cmdSize;
    readUint32(src, cmdSize);
    removeFromBuffer(src, 4);
    processed += 4;

    if (cmdSize > src.size()) {
      break;
    }

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
  dest.insert(dest.end(), head, head + 4);
  dest.insert(dest.end(), res.begin(), res.end());
}

} // namespace corekv
