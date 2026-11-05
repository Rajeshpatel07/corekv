#pragma once

#include "db/hashtable.h"

namespace corekv::db {
// NOTE: Single shared database for now. Sharding and helper functions will be
// added later.
struct Database {
  HashMap hash_map;
};

extern Database database;
} // namespace corekv::db
