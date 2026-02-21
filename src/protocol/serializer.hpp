#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace corekv {

void appendToBuffer(std::vector<uint8_t> &dest, const uint8_t *data, int size);

void removeFromBuffer(std::vector<uint8_t> &src, int size);

void readUint32(std::vector<uint8_t> &src, uint32_t &dest);

void readMessage(std::vector<uint8_t> &src, std::string &msg, uint32_t size);

void parseCommands(std::vector<uint8_t> &src, std::vector<std::string> &cmds, int totalSize);

void generateResponse(std::vector<uint8_t> &dest, const std::string &res);

} // namespace corekv
