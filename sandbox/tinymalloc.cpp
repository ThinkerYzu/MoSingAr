/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>

#include <asm/unistd.h>
#include <unistd.h>

#include <new>

#ifndef TEST
void* operator new(std::size_t count, void* ptr);
void* operator new[](std::size_t count, void* ptr);
#endif

#define assert(x) do { if(!(x)) { write(2, "\n", 1); write(2, #x, sizeof(#x) - 1); write(2, "\n", 1); abort(); } } while(0)

extern "C" {
extern long (*td__syscall_trampo)(long, ...);
}
#ifdef TEST
#define SYSCALL syscall
#else
#define SYSCALL td__syscall_trampo
#endif

struct chunkinfo_t {
  void* begin;
  chunkinfo_t* next;
  uint32_t elm_size;
  uint32_t bytes;
  uint32_t nfree;

  chunkinfo_t(uint32_t element_size, void* begin, uint32_t bytes)
    : begin(begin)
    , next(nullptr)
    , elm_size(element_size)
    , bytes(bytes)
    , nfree(max_elements()) {
    auto to_clean = first_element();
    auto usebits = (uint8_t*)begin;
    for (uint32_t i = 0; i < to_clean; i++) {
      usebits[i] = 0;
    }
  }

  uint32_t first_element() {
    auto nelm = bytes / elm_size;
    auto usebits_size = (nelm + 7) / 8;
    auto first_elm = (usebits_size + 7) & ~0x7;
    return first_elm;
  }

  uint32_t max_elements() {
    return (bytes - first_element()) / elm_size;
  }

  int32_t alloc_free_range(uint32_t start, uint32_t stop) {
    auto usebits = (uint8_t*)begin;
    for (auto i = start; i < stop; i += 8) {
      auto byte = usebits[i / 8];
      if (byte == 0xff) {
        continue;
      }
      for (int j = 0; j < 8; j++) {
        if (0x1 & (byte >> j)) {
          continue;
        }
        auto found = i + j;
        if (found < stop) {
          usebits[i / 8] |= 0x1 << j;
          nfree--;
          return found;
        }
      }
    }
    return -1;
  }

  int32_t alloc_free() {
    if (nfree == 0) {
      return -1;
    }
    static uint32_t rand = 0;
    auto max = max_elements();
    auto start = (rand++ % 8) * max / 8;
    start = start & ~0x7;
    auto found = alloc_free_range(start, max);
    if (found == -1 && start != 0) {
      found = alloc_free_range(0, start);
    }
    return found;
  }

  bool is_part(void* ptr) {
    auto off = (char*)ptr - (char*)begin;
    return off >= first_element() && off < bytes;
  }

  void* alloc() {
    auto found = alloc_free();
    if (found < 0) {
      return nullptr;
    }
    return (char*)begin + first_element() + found * elm_size;
  }

  void free(void* ptr) {
    assert (is_part(ptr));
    auto pos = ((char*)ptr - (char*)begin - first_element()) / elm_size;
    auto usebits = (uint8_t*)begin;
    usebits[pos / 8] &= ~(0x1 << (pos % 8));
    nfree++;
  }

  bool has_free() {
    return nfree != 0;
  }
};

#define LARGE_CHUNK 0xff
#define SMALL_ALLOC_LOWER_POW 2
#define SMALL_ALLOC_UPPER_POW 8
#define SMALL_ALLOC_TYPES (SMALL_ALLOC_UPPER_POW - SMALL_ALLOC_LOWER_POW + 1)
#define SMALL_ALLOC_UPPER (1 << SMALL_ALLOC_UPPER_POW)
#define MAX_CHUNKS ((SMALL_ALLOC_UPPER_POW - SMALL_ALLOC_UPPER_POW + 1) * 3)

class mem_block_t {
public:
  void *start;
  void *next_free;
  unsigned int size;
  unsigned int used;

  bool in_alloc_chunk;

  chunkinfo_t* buckets[SMALL_ALLOC_TYPES + 1];
  uint32_t nfrees[SMALL_ALLOC_TYPES + 1];

  chunkinfo_t* large;
  chunkinfo_t* large_free;

  mem_block_t(void *mem, uint32_t size)
    : start(mem)
    , next_free(mem)
    , size(size)
    , used(0)
    , in_alloc_chunk(false)
    , large(nullptr)
    , large_free(nullptr) {
    for (int i = 0; i < SMALL_ALLOC_TYPES + 1; i++) {
      buckets[i] = nullptr;
      nfrees[i] = 0;
    }
  }

  static int get_type(uint32_t size) {
    if (size <= (1 << SMALL_ALLOC_LOWER_POW)) {
      return SMALL_ALLOC_LOWER_POW;
    }
    if (size <= (1 << SMALL_ALLOC_UPPER_POW)) {
      int i = SMALL_ALLOC_LOWER_POW +1;
      while (size > (uint32_t)(1 << i)) {
        i++;
      }
      return i - SMALL_ALLOC_LOWER_POW;
    }
    return LARGE_CHUNK;
  }

  static uint32_t get_type_pow(uint32_t pow) {
    return pow - SMALL_ALLOC_LOWER_POW;
  }

  static int get_size(uint32_t type) {
    return 1 << (type + SMALL_ALLOC_LOWER_POW);
  }

  void init() {
    // Use a temporary chunk, boot_chunk, to allocate space for the
    // real first chunk.
    auto type = get_type(sizeof(chunkinfo_t));
    chunkinfo_t boot_chunk(get_size(type), start, 256);

    // After initialize the first chunk, allocate space from the real
    // first chunk to take it's space from itself.
    auto ptr = boot_chunk.alloc();
    auto first_chunk = new(ptr) chunkinfo_t(get_size(type), start, 256);
    first_chunk->alloc();

    next_free = (char*)next_free + first_chunk->bytes;

    put_front(first_chunk, nullptr);
    nfrees[type] = first_chunk->nfree;
  }

  uint32_t get_nfree_type(uint32_t type) {
    return nfrees[type];
  }

  void make_sure() {
    auto type = get_type(sizeof(chunkinfo_t));
    auto nfree = get_nfree_type(type);
    if (nfree == 1) {
      alloc_chunk(type);
    }
  }

  void put_front(chunkinfo_t* chunk, chunkinfo_t* prev) {
    auto type = get_type(chunk->elm_size);
    if (chunk == buckets[type]) {
      return;
    }
    if (prev != nullptr) {
      // So, it is not a new chunk!
      prev->next = chunk->next;
    }
    chunk->next = buckets[type];
    buckets[type] = chunk;
  }

  void put_large(chunkinfo_t* chunk) {
    auto ptr = large;
    if (ptr == nullptr || ptr->elm_size >= chunk->elm_size) {
      chunk->next = large;
      large = chunk;
      return;
    }
    chunkinfo_t* last = nullptr;
    while (ptr != nullptr) {
      if (ptr->elm_size < chunk->elm_size) {
        last = ptr;
        ptr = ptr->next;
        continue;
      }
      chunk->next = ptr;
      last->next = chunk;
      return;
    }
    last->next = chunk;
  }

  void put_large_free(chunkinfo_t* chunk) {
    auto ptr = large_free;
    if (ptr == nullptr || ptr->elm_size >= chunk->elm_size) {
      chunk->next = large_free;
      large_free = chunk;
      return;
    }
    chunkinfo_t* last = nullptr;
    while (ptr != nullptr) {
      if (ptr->elm_size < chunk->elm_size) {
        last = ptr;
        ptr = ptr->next;
        continue;
      }
      chunk->next = ptr;
      last->next = chunk;
      return;
    }
    last->next = chunk;
  }

  chunkinfo_t* _alloc_chunk(uint32_t type, uint32_t bytes) {
    auto ptr = _alloc_small(get_type(sizeof(chunkinfo_t)));
    if (ptr == nullptr) {
      return nullptr;
    }
    auto elm_size = type == LARGE_CHUNK ? bytes : get_size(type);
    auto chunk = new(ptr) chunkinfo_t(elm_size, next_free, bytes);
    next_free = (char*)next_free + chunk->bytes;

    if (type == LARGE_CHUNK) {
      put_large(chunk);
    } else {
      put_front(chunk, nullptr);
      nfrees[type] += chunk->nfree;
    }

    return chunk;
  }
  chunkinfo_t* alloc_chunk(uint32_t type, uint32_t bytes = 1024) {
    assert(!in_alloc_chunk);
    in_alloc_chunk = true;

    auto chunk = _alloc_chunk(type, bytes);

    in_alloc_chunk = false;
    return chunk;
  }

  void* _alloc_small(uint32_t type) {
    // Find a chunk having free memory or create a new chunk.
    chunkinfo_t* chunk = nullptr;
    chunkinfo_t* prev = nullptr;
    if (nfrees[type]) {
      chunk = buckets[type];
      while (chunk && !chunk->has_free()) {
        prev = chunk;
        chunk = chunk->next;
      }
    } else {
      chunk = alloc_chunk(type);
    }
    if (chunk == nullptr) {
      return nullptr;
    }

    auto ptr = chunk->alloc();

    if (chunk->has_free()) {
      put_front(chunk, prev);
    }
    nfrees[type]--;

    return ptr;
  }

  void* alloc_small(uint32_t type) {
    make_sure();
    return _alloc_small(type);
  }

  void* alloc_large(uint32_t bytes) {
    if (large_free) {
      auto ptr = large_free;
      if (ptr->elm_size >= bytes) {
        large_free = ptr->next;
        put_large(ptr);
        return ptr->begin;
      }
      chunkinfo_t* last = nullptr;
      while (ptr) {
        if (ptr->elm_size < bytes) {
          last = ptr;
          ptr = ptr->next;
          continue;
        }
        last->next = ptr->next;
        put_large(ptr);
        return ptr->begin;
      }
    }
    make_sure();
    bytes = (bytes + 0xff) & ~0xff;
    auto chunk = alloc_chunk(LARGE_CHUNK, bytes);
    return chunk->begin;
  }

  void* alloc(uint32_t size) {
    auto type = get_type(size);
    if (type == LARGE_CHUNK) {
      return alloc_large(size);
    }
    return alloc_small(type);
  }

  chunkinfo_t* find_chunk_type(uint32_t type, void* ptr, chunkinfo_t** ptrprev) {
    auto chunk = buckets[type];
    chunkinfo_t* prev = nullptr;
    while (chunk) {
      if (chunk->is_part(ptr)) {
        if (ptrprev) {
          *ptrprev = prev;
        }
        return chunk;
      }
      prev = chunk;
      chunk = chunk->next;
    }
    return nullptr;
  }

  chunkinfo_t* find_chunk(void* ptr, chunkinfo_t** ptrprev) {
    for (int i = SMALL_ALLOC_LOWER_POW; i < SMALL_ALLOC_UPPER_POW; i++) {
      auto chunk = find_chunk_type(get_type_pow(i), ptr, ptrprev);
      if (chunk) {
        return chunk;
      }
    }
    if (large) {
      auto chunk = large;
      if (chunk->begin == ptr) {
        large = chunk->next;
        return chunk;
      }
      chunkinfo_t* last = nullptr;
      while (chunk) {
        if (chunk->begin != ptr) {
          last = chunk;
          chunk = chunk->next;
          continue;
        }
        last->next = chunk->next;
        return chunk;
      }
    }
    return nullptr;
  }

  void free(void* ptr) {
    chunkinfo_t* prev;
    auto chunk = find_chunk(ptr, &prev);
    assert(chunk);

    if (chunk->elm_size != chunk->bytes) {
      chunk->free(ptr);

      put_front(chunk, prev);

      auto type = get_type(chunk->elm_size);
      nfrees[type]++;
    } else {
      put_large_free(chunk);
    }
  }
};

static mem_block_t *global_mem_block = nullptr;

#ifndef TEST
extern "C" {
void tinymalloc_init() {
  static char buf[sizeof(mem_block_t)];
  assert(global_mem_block == nullptr);

  auto mem = (void*)SYSCALL(__NR_mmap,
                            nullptr,
                            1024 * 1024,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
  auto block = new(buf) mem_block_t(mem, 1024 * 1024);
  block->init();
  global_mem_block = block;
}

void* malloc(size_t size) {
  return global_mem_block->alloc(size);
}

void free(void* ptr) {
  global_mem_block->free(ptr);
}
}

void* operator new(unsigned long count) {
  return malloc((size_t)count);
}

void* operator new[](unsigned long count) {
  return malloc((size_t)count);
}

void* operator new(unsigned long count, std::nothrow_t& nothrow) {
  return malloc((size_t)count);
}

void* operator new[](unsigned long count, std::nothrow_t& nothrow) {
  return malloc((size_t)count);
}

std::nothrow_t nothrow;

void* operator new(std::size_t count, void* ptr) {
  return ptr;
}

void* operator new[](std::size_t count, void* ptr) {
  return ptr;
}

void operator delete(void* ptr) throw() {
  free(ptr);
}

void operator delete[](void* ptr) throw() {
  free(ptr);
}

#endif // !TEST

#ifdef TEST

#include <stdio.h>

static char mem[8192];

int
main(int argc, char * const argv[]) {
  mem_block_t block(mem, sizeof(mem));
  block.init();
  for (int i = 0; i < 20; i++) {
    printf("%p\n", block.alloc_small(5));
  }
  printf("try free\n");
  for (int i = 0; i < 11; i++) {
    block.alloc_small(2);
  }
  auto ptr = block.alloc_small(2);
  for (int i = 0; i < 10; i++) {
    printf("%p\n", ptr);
    block.free(ptr);
    ptr = block.alloc_small(2);
  }
  printf("1024byts\n");
  for (int i = 0; i < 10; i++) {
    auto ptr = block.alloc(1024);
    auto ptr2 = block.alloc(768);
    printf("%p %p\n", ptr, ptr2);
    block.free(ptr);
    block.free(ptr2);
  }
}

#endif
