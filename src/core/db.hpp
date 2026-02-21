#pragma once

#include "hash_table.hpp"

namespace corekv {

struct Database {
  HMap store;
};

extern Database db;

} // namespace corekv
