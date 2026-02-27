#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace corekv {

enum {
  TAG_NIL = 0,  // nil
  TAG_ERR = 1,  // error code + msg
  TAG_STR = 2,  // string
  TAG_INT = 3,  // int32
  TAG_LONG = 4, // int64
  TAG_DBL = 5,  // double
  TAG_ARR = 6,  // array
};

void appendToBuffer(std::vector<uint8_t> &dest, const uint8_t *data, int size);

void removeFromBuffer(std::vector<uint8_t> &src, int size);

void readUint32(std::vector<uint8_t> &src, uint32_t &dest);

void readMessage(std::vector<uint8_t> &src, std::string &msg, uint32_t size);

void parseCommands(std::vector<uint8_t> &src, std::vector<std::string> &cmds,
                   int totalSize);

void generateResponse(std::vector<uint8_t> &dest, const std::string &res);

void addTagNil(std::vector<uint8_t> &dest);

void addTagStr(std::vector<uint8_t> &dest, const uint8_t *data, int size);

void addTagErr(std::vector<uint8_t> &dest, const std::string msg);

void addTagInt(std::vector<uint8_t> &dest, int val);

void addTagLong(std::vector<uint8_t> &dest, long val);

void addTagDbl(std::vector<uint8_t> &dest, double val);

void addTagArr(std::vector<uint8_t> &dest, uint32_t len);

} // namespace corekv
