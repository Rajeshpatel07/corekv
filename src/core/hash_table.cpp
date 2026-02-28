#include "hash_table.hpp"
#include "protocol/serializer.hpp"
#include <cstdlib>
#include <iostream>

namespace corekv {

uint64_t hash(const uint8_t *data, size_t len) {
  uint64_t h = kFnvOffsetBasis64;
  const auto *bytes = static_cast<const unsigned char *>(data);

  for (size_t i = 0; i < len; ++i) {
    h ^= bytes[i];
    h *= kFnvPrime64;
  }

  return h;
}

bool matchKeys(HNode *lhs, HNode *rhs) {
  Entry *l = CONTAINER_OF(lhs, Entry, node);
  Entry *r = CONTAINER_OF(rhs, Entry, node);
  return l->key == r->key;
}

void hInit(HTab *htab, size_t size) {
  htab->tab = static_cast<HNode **>(std::calloc(size, sizeof(HNode *)));
  htab->mask = size - 1;
  htab->size = 0;
}

void hInsert(HTab *htab, HNode *node) {
  uint64_t pos = node->hcode & htab->mask;

  HNode *next = htab->tab[pos];
  node->next = next;
  htab->tab[pos] = node;
  htab->size++;
}

HNode **hLookup(HTab *htab, HNode *node) {
  if (!htab || !htab->tab) {
    return nullptr;
  }

  size_t pos = node->hcode & htab->mask;
  HNode **entry = &htab->tab[pos];

  for (HNode *cur; (cur = *entry) != nullptr; entry = &cur->next) {
    if (cur->hcode == node->hcode && matchKeys(cur, node)) {
      return entry;
    }
  }

  return nullptr;
}

HNode *hDetach(HTab *htab, HNode **from) {
  HNode *node = *from;

  *from = node->next;
  htab->size--;
  return node;
}

void hmapTriggerRehash(HMap *hmap) {
  hmap->older = hmap->newer;

  hInit(&hmap->newer, (hmap->newer.mask + 1) * 2);
  hmap->migrate_pos = 0;
}

void hmapRehash(HMap *hmap) {
  size_t nwork = 0;
  while (nwork < kRehashingWork && hmap->older.size > 0) {
    if (hmap->migrate_pos > hmap->older.mask) {
      break;
    }

    HNode **from = &hmap->older.tab[hmap->migrate_pos];
    if (!*from) {
      hmap->migrate_pos++;
      continue;
    }

    hInsert(&hmap->newer, hDetach(&hmap->older, from));
    nwork++;
  }

  if (hmap->older.size == 0 && hmap->older.tab) {
    std::free(hmap->older.tab);
    hmap->older = HTab{};
  }
}

HNode *hmapLookup(HMap *hmap, HNode *key) {
  HNode **from = hLookup(&hmap->newer, key);

  if (!from) {
    from = hLookup(&hmap->older, key);
  }

  return from ? *from : nullptr;
}

HNode *hmapDelete(HMap *hmap, HNode *key) {
  if (HNode **from = hLookup(&hmap->newer, key)) {
    return hDetach(&hmap->newer, from);
  }

  if (HNode **from = hLookup(&hmap->older, key)) {
    return hDetach(&hmap->older, from);
  }

  return nullptr;
}

void hmapInsert(HMap *hmap, HNode *node) {
  if (!hmap->newer.tab) {
    hInit(&hmap->newer, 4);
  }

  hInsert(&hmap->newer, node);

  if (!hmap->older.tab) {
    size_t loadfactor = (hmap->newer.mask + 1) * kMaxLoadFactor;
    if (hmap->newer.size >= loadfactor) {
      hmapTriggerRehash(hmap);
    }
  }

  hmapRehash(hmap);
}

size_t hmapSize(HMap *hmap) { return hmap->newer.size + hmap->older.size; }

void hmapForEach(HTab *htab, std::vector<uint8_t> &dest) {
  // Iterate through ALL buckets (0 to mask inclusive)
  // Note: mask = size - 1, so if size=4, mask=3, buckets are 0,1,2,3
  for (size_t i = 0; htab->tab != nullptr && i <= htab->mask; ++i) {
    for (HNode *mover = htab->tab[i]; mover != nullptr; mover = mover->next) {
      Entry *record = CONTAINER_OF(mover, Entry, node);
      addTagStr(dest, reinterpret_cast<const uint8_t *>(record->key.data()),
                record->key.size());
    }
  }
}

void hmapKeys(HMap *hmap, std::vector<uint8_t> &dest) {
  hmapForEach(&hmap->newer, dest);
  hmapForEach(&hmap->older, dest);
}

// Free all entries in a single hash table (does not free the table itself)
static void hTabDestroy(HTab *htab) {
  if (!htab || !htab->tab) {
    return;
  }
  // Iterate through all buckets and free each Entry
  for (size_t i = 0; i <= htab->mask; ++i) {
    HNode *node = htab->tab[i];
    while (node) {
      HNode *next = node->next;
      // Free the Entry containing this HNode
      Entry *entry = CONTAINER_OF(node, Entry, node);
      delete entry;
      node = next;
    }
  }
  // Free the bucket array itself
  std::free(htab->tab);
  htab->tab = nullptr;
  htab->mask = 0;
  htab->size = 0;
}

// Destroy entire hash map and free all allocated memory
// This should be called when shutting down the server
void hmapDestroy(HMap *hmap) {
  if (!hmap) {
    return;
  }
  // Free both newer and older tables along with all entries
  hTabDestroy(&hmap->newer);
  hTabDestroy(&hmap->older);
  hmap->migrate_pos = 0;
}

} // namespace corekv
