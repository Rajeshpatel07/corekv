#pragma once
#include <cstdint>
#include <string>
#include <vector>

void read_u32(std::vector<uint8_t> &src, uint32_t &dist, int size);
void read_msg(std::vector<uint8_t> &src, std::vector<std::string> &cmd,
              int size);

void get_cmds(std::vector<uint8_t> &src, std::vector<std::string> &cmd,
              int size);
