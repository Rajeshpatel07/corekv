#include "hashtable.hpp"
#include <cstdlib>

// FNV-1a hash function for generating hash codes from byte data
uint64_t hash(const uint8_t *data, size_t len) {
  uint64_t hash = FNV_OFFSET_BASIS_64;
  const unsigned char *bytes = static_cast<const unsigned char *>(data);

  for (size_t i = 0; i < len; ++i) {
    hash ^= bytes[i];     // XOR the current hash with the byte
    hash *= FNV_PRIME_64; // Multiply by the FNV prime
  }

  return hash;
}

// Compare keys of two Entry nodes for equality during lookup
bool matchkeys(HNode *lhs, HNode *rhs) {
  Entry *l = container_of(lhs, Entry, node);
  Entry *r = container_of(rhs, Entry, node);
  return l->key == r->key;
}

// Initialize a hashtable with the given size, allocating memory for the bucket array
void h_init(HTab *htab, size_t size) {
  htab->tab = (HNode **)calloc(size, sizeof(HNode *));
  htab->mask = size - 1;
  htab->size = 0;
}

// Insert a node into the hashtable at the computed bucket position
void h_insert(HTab *htab, HNode *node) {
  uint64_t pos = node->hcode & htab->mask;

  HNode *next = htab->tab[pos];
  node->next = next;
  htab->tab[pos] = node;
  htab->size++;
}

HNode **h_lookup(HTab *htab, HNode *node) {
  if (!htab) {
    return nullptr;
  }

  // Check if the table is initialized to avoid null pointer dereference
  if (!htab->tab) {
    return nullptr;
  }

  size_t pos = node->hcode & htab->mask;
  HNode **entry = &htab->tab[pos];

  for (HNode *cur; (cur = *entry) != nullptr; entry = &cur->next) {
    if (cur->hcode == node->hcode && matchkeys(cur, node)) {
      return entry;
    }
  }

  return nullptr;
}

// Remove and return the node from the hashtable at the given position
HNode *h_detach(HTab *htab, HNode **from) {
  HNode *node = *from;

  *from = node->next;
  htab->size--;
  return node;
}

// NOTE: Hmap functions;

// Trigger a rehash by swapping tables and allocating a larger new table
void hmap_trigger_rehash(HMap *hmap) {
  hmap->older = hmap->newer;

  h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
  hmap->migrate_pos = 0;
}

// Perform incremental rehashing by migrating nodes from older to newer table
void hmap_rehash(HMap *hmap) {
  size_t nwork = 0;
  while (nwork < k_rehashing_work && hmap->older.size > 0) {
    // Prevent out-of-bounds access if migrate_pos exceeds table size
    if (hmap->migrate_pos > hmap->older.mask) {
      break;
    }
    // find a non-empty slot
    HNode **from = &hmap->older.tab[hmap->migrate_pos];
    if (!*from) {
      hmap->migrate_pos++;
      continue; // empty slot
    }
    // move the first list item to the newer table
    h_insert(&hmap->newer, h_detach(&hmap->older, from));
    nwork++;
  }
  // discard the old table if done
  if (hmap->older.size == 0 && hmap->older.tab) {
    free(hmap->older.tab);
    hmap->older = HTab{};
  }
}

// Lookup a key in the hashmap, checking newer table first, then older during rehash
HNode *hmap_lookup(HMap *hmap, HNode *key) {
  HNode **from = h_lookup(&hmap->newer, key); // check new record

  if (!from) {
    from = h_lookup(&hmap->older, key); // check old record
  }

  return from ? *from : nullptr;
}

// Delete a key from the hashmap, checking both tables
HNode *hmap_delete(HMap *hmap, HNode *key) {
  if (HNode **from = h_lookup(&hmap->newer, key)) {
    return h_detach(&hmap->newer, from);
  }

  if (HNode **from = h_lookup(&hmap->older, key)) {
    return h_detach(&hmap->older, from);
  }

  return nullptr;
}

// Insert a node into the hashmap, triggering rehash if load factor is exceeded
void hmap_insert(HMap *hmap, HNode *node) {
  if (!hmap->newer.tab) {
    h_init(&hmap->newer, 4);
  }

  h_insert(&hmap->newer, node);

  if (!hmap->older.tab) {
    size_t loadfactor = (hmap->newer.mask + 1) * k_max_load_factor;
    if (hmap->newer.size >= loadfactor) {
      hmap_trigger_rehash(hmap);
    }
  }

  hmap_rehash(hmap);
}
