#pragma once
#include <cstdint>
#include <string>
#include <vector>

void add_to_buffer(std::vector<uint8_t> &dist, const uint8_t *data, int size);
void rm_from_buffer(std::vector<uint8_t> &src, int size);

void read_u32(std::vector<uint8_t> &src, uint32_t &dist, int size);
void read_msg(std::vector<uint8_t> &src, std::vector<std::string> &cmd,
              int size);

void get_cmds(std::vector<uint8_t> &src, std::vector<std::string> &cmd,
              int size);

void generate_response(std::vector<uint8_t> &dist, std::string &res);
