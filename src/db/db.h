#pragma once

#include "hashtable.h"

namespace corekv::db {
// NOTE: For now this is just a simple db, implement shards and helper functions
struct Database {
  HashMap h_map;
};

extern Database db;
} // namespace corekv::db
