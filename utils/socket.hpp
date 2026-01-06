#pragma once

#include <cstdint>
#include <vector>
void set_socket_flags(int fd);

void add_to_buffer(std::vector<uint8_t> &dist, const uint8_t *data, int size);
void rm_from_buffer(std::vector<uint8_t> &src, int size);
