#include "parser.hpp"
#include "socket.hpp"
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <vector>

void read_u32(std::vector<uint8_t> &src, uint32_t &dist, int size) {
  memcpy(&dist, src.data(), 4);
  dist = ntohl(dist);
}
void read_msg(std::vector<uint8_t> &src, std::vector<std::string> &cmd,
              int size) {
  std::string c(src.begin(), src.begin() + size);
  cmd.push_back(c);
}

void get_cmds(std::vector<uint8_t> &src, std::vector<std::string> &cmd,
              int size) {
  while (size > 0) {
    uint32_t cmd_size;
    read_u32(src, cmd_size, 4);
    rm_from_buffer(src, 4);

    read_msg(src, cmd, cmd_size);
    rm_from_buffer(src, cmd_size);

    size -= (4 + cmd_size);
  }
}
