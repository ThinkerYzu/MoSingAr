/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "ptracetools.h"
#include "loader.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <assert.h>
#include <memory>

#include <elf.h>

#include <sys/mman.h>


#define _E(name, args...) do {                  \
    auto r = name(args); \
    if (r < 0) { perror(#name); return r; } \
  } while( 0)
#define _ENull(name, args...) do {                  \
    auto r = name(args); \
    if (r < 0) { perror(#name); return nullptr; } \
  } while( 0)

extern "C" {
extern void shellcode_funcall_trap();
extern void shellcode_funcall_trap_end();
}

/**
 * The shell code will be called as a function and be trapped when it
 * returns.  The return value will be in rax.
 */
struct trapped_shellcode {
  int size;
  void* code;
  void* base;

  const char* so_path;

  prog_header* headers;
  int header_num;

  void (**init_funcs)();

  void** rela;

  ~trapped_shellcode() {
    delete (char*)code;
  }

private:
  template<typename T>
  T* rptr(T* ptr) {
    return reinterpret_cast<T*>((char*)base + ((char*)ptr - (char*)code));
  }

public:
  /**
   * Relocate pointers.
   *
   * When the code block has been injected to the target process, it
   * might be placed in an different address.  This function reloactes
   * pointers considering different base/begin address.
   */
  void relocate(void* begin) {
    assert(base == nullptr);
    base = begin;

    // Relocate the entry point
    auto funcall_trap_bytes =
      (char*)shellcode_funcall_trap_end - (char*)shellcode_funcall_trap;
    auto entry_ptr = (void**)((char*)code + (funcall_trap_bytes - sizeof(void*)));
    *entry_ptr = rptr(*entry_ptr);

    so_path = rptr(so_path);
    headers = rptr(headers);
    init_funcs = rptr(init_funcs);
    rela = rptr(rela);
  }
};

class ElfParser {
public:
  ElfParser(const char* so_path)
    : so_path(so_path)
    , fd(-1)
    , hdr_valid(false)
    , phdrs(nullptr)
    , dyns(nullptr)
    , dyn_num(0)
    , shdrs(nullptr)
    , shstrtab(nullptr) {
  }
  ~ElfParser() {
    if (fd >= 0) {
      close(fd);
    }
    if (phdrs) {
      delete[] phdrs;
    }
    if (dyns) {
      delete[] dyns;
    }
    if (shdrs) {
      delete[] shdrs;
    }
    if (shstrtab) {
      delete shstrtab;
    }
  }

  int open() {
    assert(fd < 0);
    fd = ::open(so_path, O_RDONLY);
    if (fd < 0) {
      perror("open");
      return -1;
    }
    return 0;
  }

  int get_fd() {
    return fd;
  }

  int parse_header() {
    assert(fd >= 0);
    assert(!hdr_valid);
    _E(read, fd, &hdr, sizeof(hdr));
    hdr_valid = true;
    return 0;
  }

  int parse_prog_headers() {
    assert(fd >= 0);
    assert(hdr_valid);
    assert(phdrs == nullptr);
    assert(hdr.e_phentsize == sizeof(*phdrs));

    _E(lseek, fd, hdr.e_phoff, SEEK_SET);
    auto bytes = hdr.e_phentsize * hdr.e_phnum;
    phdrs = new Elf64_Phdr[hdr.e_phnum];
    _E(read, fd, phdrs, bytes);

    return 0;
  }

  int get_prog_header_num() const {
    assert(hdr_valid);
    return hdr.e_phnum;
  }

  const Elf64_Phdr* get_prog_headers() const {
    return phdrs;
  }

  int parse_dyanmic() {
    assert(fd >= 0);
    assert(phdrs);
    assert(dyns == nullptr);
    assert(dyn_num == 0);
    for (int i = 0; i < get_prog_header_num(); i++) {
      auto phdr = get_prog_headers() + i;
      if (phdr->p_type == PT_DYNAMIC) {
        auto bytes = phdr->p_filesz;
        dyn_num = bytes / sizeof(Elf64_Dyn);
        assert((bytes % sizeof(Elf64_Dyn)) == 0);
        dyns = new Elf64_Dyn[dyn_num];
        _E(lseek, fd, phdr->p_offset, SEEK_SET);
        _E(read, fd, dyns, bytes);
        break;
      }
    }
    return 0;
  }

  int get_dynamic_num() {
    return dyn_num;
  }

  Elf64_Dyn* get_dynamics() {
    return dyns;
  }

  int parse_sect_headers() {
    assert(fd >= 0);
    assert(hdr_valid);
    assert(shdrs == nullptr);
    assert(hdr.e_shentsize == sizeof(*shdrs));

    _E(lseek, fd, hdr.e_shoff, SEEK_SET);
    auto bytes = hdr.e_shentsize * hdr.e_shnum;
    shdrs = new Elf64_Shdr[hdr.e_shnum];
    _E(read, fd, shdrs, bytes);

    return 0;
  }

  int get_sect_header_num() const {
    assert(hdr_valid);
    return hdr.e_shnum;
  }

  const Elf64_Shdr* get_sect_headers() const {
    return shdrs;
  }

  int parse_shstrtab() {
    assert(shdrs);
    assert(hdr_valid);
    assert(shstrtab == nullptr);

    auto shdr = shdrs + hdr.e_shstrndx;
    shstrtab = new char[shdr->sh_size];
    shstrtab_bytes = shdr->sh_size;

    _E(lseek, fd, shdr->sh_offset, SEEK_SET);
    _E(read, fd, shstrtab, shdr->sh_size);

    return 0;
  }

  const char* get_shstrtab() {
    return shstrtab;
  }

  unsigned int get_shstrtab_size() {
    return shstrtab_bytes;
  }

private:
  const char* so_path;
  int fd;

  bool hdr_valid;
  Elf64_Ehdr hdr;
  Elf64_Phdr* phdrs;
  Elf64_Dyn* dyns;
  int dyn_num;
  Elf64_Shdr* shdrs;

  char* shstrtab;
  unsigned int shstrtab_bytes;
};

const char* libtongdao_so_path = "../sandbox/libtongdao.so";

/**
 * The whole shellcode comprises following components in order.
 * They are
 *  - shellcode_funcall_trap;
 *    - which has a pointer at last 8 bytes for x86_64 to the entry
 *      point of the loader,
 *  - the path of the shared object to load,
 *  - a list of prog_header; that describe how to load code and
 *    data of the shared object to memory,
 *  - a list of offsets of init functions to initialize the shared
 *    object, and
 *  - the code of the loader which load the shared object to memory.
 *
 * The shellcode_funcall_trap is at the begining of the whole code.
 * It is responsible to call the loader and trigger the breakpoint, a
 * trap, once the loader return.  By detecting a trap, the carrier
 * know that the loader has returned, and can read the return value
 * from the register rax for x86_64.
 *
 * The shellcode_funcall_trap assumes that the last 8 bytes of itself
 * is the address of the entry point of the loader.
 *
 * Other data in-between the shellcode_funcall_trap and the loader is
 * the content of arguments passed to the loader.  The
 * shellcode_funcall_trap does not pass arguments, instead the carrier
 * should set the registers and the content on the stack properly to
 * pass the arguments.
 */
trapped_shellcode*
prepare_shellcode() {
  auto so_path = libtongdao_so_path;

  ElfParser solib(so_path);
  solib.open();
  solib.parse_header();
  solib.parse_prog_headers();
  solib.parse_dyanmic();
  solib.parse_sect_headers();
  solib.parse_shstrtab();

  // Parse program headers
  int load_num = 0;
  for (int i = 0; i < solib.get_prog_header_num(); i++) {
    auto phdr = solib.get_prog_headers() + i;
    if (phdr->p_type == PT_LOAD) {
      load_num++;
    }
  }
  std::unique_ptr<prog_header[]> phdrs(new prog_header[load_num]);
  auto phdrs_bytes = sizeof(prog_header[load_num]);
  int phidx = 0;
  for (int i = 0; i < solib.get_prog_header_num(); i++) {
    auto phdr = solib.get_prog_headers() + i;
    if (phdr->p_type == PT_LOAD) {
      phdrs[phidx].offset = phdr->p_offset;
      phdrs[phidx].addr = phdr->p_vaddr;
      phdrs[phidx].file_size = phdr->p_filesz;
      phdrs[phidx].mem_size = phdr->p_memsz;
      phidx++;
    }
  }

  auto mem_to_file_offset = [&](long addr) -> auto {
    for (int i = 0; i < load_num; i++) {
      if (phdrs[i].addr <= addr && phdrs[i].addr + phdrs[i].mem_size > addr) {
        return phdrs[i].offset + addr - phdrs[i].addr;
      }
    }
    printf("fail to mapping the address %lx to file offset.\n", addr);
    abort();
  };

  // Parse .init_array
  std::unique_ptr<void *[]> init_array;
  unsigned int init_array_bytes = 0;
  unsigned int init_array_num = 0;
  long init_array_offset = 0;
  for (int i = 0; i < solib.get_dynamic_num(); i++) {
    auto dyn = solib.get_dynamics() + i;
    if (dyn->d_tag == DT_INIT_ARRAY) {
      init_array_offset = mem_to_file_offset(dyn->d_un.d_val);
    } else if (dyn->d_tag == DT_INIT_ARRAYSZ) {
      init_array_bytes = dyn->d_un.d_val;
    }
    if (!init_array_offset || !init_array_bytes) {
      continue;
    }

    assert(init_array_bytes % sizeof(void*) == 0);
    init_array_num = init_array_bytes / sizeof(void*);
    init_array = std::unique_ptr<void *[]>(new void*[init_array_num + 1]);

    _ENull(lseek, solib.get_fd(), init_array_offset, SEEK_SET);
    _ENull(read, solib.get_fd(), init_array.get(), init_array_bytes);
    init_array[init_array_num] = nullptr;
    init_array_bytes += sizeof(void*);

    break;
  }

  // Parse rela
  unsigned int rela_offset;
  unsigned int rela_elf_bytes;
  unsigned int rela_entsize;
  for (int i = 0; i < solib.get_dynamic_num(); i++) {
    auto dyn = solib.get_dynamics() + i;
    if (dyn->d_tag == DT_RELA) {
      rela_offset = mem_to_file_offset(dyn->d_un.d_val);
    } else if (dyn->d_tag == DT_RELAENT) {
      rela_entsize = dyn->d_un.d_val;
    } else if (dyn->d_tag == DT_RELASZ) {
      rela_elf_bytes = dyn->d_un.d_val;
    }
  }
  assert(rela_entsize == sizeof(Elf64_Rela));
  assert(rela_elf_bytes % rela_entsize == 0);
  unsigned int rela_num = rela_elf_bytes / rela_entsize;
  std::unique_ptr<void *[]> rela(new void*[rela_num + 1]);
  Elf64_Rela rela_ent;
  _ENull(lseek, solib.get_fd(), rela_offset, SEEK_SET);
  for (unsigned int i = 0; i < rela_num; i++) {
    _ENull(read, solib.get_fd(), &rela_ent, rela_entsize);
    // Assume only this type of entries are there.
    assert(rela_ent.r_info == R_X86_64_RELATIVE);
    rela[i] = (void*)rela_ent.r_offset;
  }
  rela[rela_num] = nullptr;

  auto funcall_trap_bytes =
    (char*)shellcode_funcall_trap_end - (char*)shellcode_funcall_trap;
  auto loader_bytes = (char*)loader_end - (char*)loader_start;
  auto shellcode = new trapped_shellcode;

#define ROUND8(x) do { x = (x + 0x7) & ~0x7; } while(0)

  // Compute the size of the whole shellcode.
  shellcode->size = funcall_trap_bytes;
  ROUND8(shellcode->size);
  shellcode->size += strlen(so_path) + 1;
  ROUND8(shellcode->size);
  shellcode->size += phdrs_bytes;
  ROUND8(shellcode->size);
  shellcode->size += init_array_bytes;
  ROUND8(shellcode->size);
  auto rela_bytes = sizeof(void*) * (rela_num + 1);
  shellcode->size += rela_bytes;
  ROUND8(shellcode->size);
  shellcode->size += loader_bytes;
  ROUND8(shellcode->size);

  auto code = new char[shellcode->size];
  shellcode->code = code;

  int pos = 0;

  // Copy code and data into the memory pointed by |code|.

  // shellcode_funcall_trap
  memcpy(code + pos, (void*)shellcode_funcall_trap, funcall_trap_bytes);
  pos += funcall_trap_bytes;
  ROUND8(pos);

  // The path of the shared object.
  shellcode->so_path = code + pos;
  strcpy(code + pos, so_path);
  pos += strlen(so_path) + 1;
  ROUND8(pos);

  // The progam headers
  shellcode->headers = (prog_header*)(code + pos);
  shellcode->header_num = load_num;
  memcpy(code + pos, phdrs.get(), phdrs_bytes);
  pos += phdrs_bytes;
  ROUND8(pos);

  // The init functions
  shellcode->init_funcs = (void (**)())(code + pos);
  memcpy(code + pos, init_array.get(), init_array_bytes);
  pos += init_array_bytes;
  ROUND8(pos);

  // The rela
  shellcode->rela = (void **)(code + pos);
  memcpy(code + pos, rela.get(), rela_bytes);
  pos += rela_bytes;
  ROUND8(pos);

  // The loader function
  auto entry = code + pos + ((char*)load_shared_object - (char*)loader_start);
  memcpy(code + pos, (void*)loader_start, loader_bytes);
  pos += loader_bytes;

  // Set entry pointer at the end of the shellcode_funcall_trap
  auto entry_ptr = (void**)(code + (funcall_trap_bytes - sizeof(void*)));
  *entry_ptr = entry;

  return shellcode;
}

/**
 * Make the sandbox taking off.
 *
 * |takeoff()| inject the loader to the target prcoess to load
 * libtongdao.so sandboxizing the process.
 */
long
takeoff(pid_t pid) {
  std::unique_ptr<trapped_shellcode> shellcode(prepare_shellcode());

  user_regs_struct saved_regs;
  ptrace_getregs(pid, saved_regs);

  auto request_size = (shellcode->size + 16384 + 4095) & ~4095;
  auto addr = inject_mmap(pid, nullptr, request_size,
                          PROT_EXEC | PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0,
                          &saved_regs);
  shellcode->relocate(addr);

  auto regs = saved_regs;
  // Set up where to inject
  regs.rip = (long long unsigned int)addr;
  // Set up the stack pointer.
  regs.rbp = regs.rsp = (long long unsigned int)((char*)addr + request_size);

  auto r = inject_run_funcall_nosave(pid,
                                     shellcode->code,
                                     shellcode->size,
                                     (char*)shellcode->code + 2, // skip 2 nops
                                     (unsigned long long)shellcode->so_path,
                                     (unsigned long long)shellcode->headers,
                                     shellcode->header_num,
                                     (unsigned long long)shellcode->init_funcs,
                                     (unsigned long long)shellcode->rela,
                                     0,
                                     &regs);

  // Restore registers
  r = ptrace_setregs(pid, saved_regs);

  return r;
}

#ifdef TEST

#include <sys/ptrace.h>
#include <sys/wait.h>

void
child() {
  for (int i = 0; i < 35; i++) {
    printf("child %d\n", i);
    sleep(1);
  }
  open("/dev/null", O_RDONLY);
  sleep(1);
  write(2, "\nExit child\n", 12);
}

void
parent(int pid) {
  printf("sleep 2s.\n");
  sleep(2);
  printf("attach %d\n", pid);
  auto r = ptrace(PTRACE_ATTACH, pid, nullptr, 0);
  if (r < 0) {
    perror("ptrace");
    return;
  }

  int status;
  do {
    printf("waitpid\n");
    r = waitpid(pid, &status, 0);
    if (r < 0) {
      perror("waitpid");
      exit(255);
    }
  } while(!WIFSTOPPED(status) && WSTOPSIG(status) != SIGTRAP);

  user_regs_struct regs;
  ptrace_getregs(pid, regs);
  printf("rip %llx\n", regs.rip);

  printf("takeoff\n");
  r = takeoff(pid);
  if (r < 0) {
    printf("fails to takeoff (%lx)!\n", r);
    return;
  }

  ptrace_getregs(pid, regs);
  printf("rip %llx\n", regs.rip);

  printf("cont\n");
  r = ptrace_cont(pid);
  if (r < 0) {
    return;
  }

  printf("Go to sleep 30s\n");
  sleep(30);
  printf("detach\n");
  r = ptrace(PTRACE_DETACH, pid, nullptr, 0);
}

int
main(int argc, char*const* argv) {
  auto pid = fork();
  if (pid < 0) {
    perror("fork");
    return 255;
  }
  if (pid) {
    parent(pid);
  } else {
    child();
  }
  return 0;
}

#endif /* TEST */
