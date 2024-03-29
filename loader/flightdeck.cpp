/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "flightdeck.h"
#include "ptracetools.h"
#include "loader.h"

#include "errhandle.h"

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
    , shstrtab(nullptr)
    , dynsym(nullptr)
    , dynstr(nullptr) {
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
    if (dynsym) {
      delete dynsym;
    }
    if (dynstr) {
      delete dynstr;
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
    _EI(read, fd, &hdr, sizeof(hdr));
    hdr_valid = true;
    return 0;
  }

  int parse_prog_headers() {
    assert(fd >= 0);
    assert(hdr_valid);
    assert(phdrs == nullptr);
    assert(hdr.e_phentsize == sizeof(*phdrs));

    _EI(lseek, fd, hdr.e_phoff, SEEK_SET);
    auto bytes = hdr.e_phentsize * hdr.e_phnum;
    phdrs = new Elf64_Phdr[hdr.e_phnum];
    _EI(read, fd, phdrs, bytes);

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
        _EI(lseek, fd, phdr->p_offset, SEEK_SET);
        _EI(read, fd, dyns, bytes);
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

    _EI(lseek, fd, hdr.e_shoff, SEEK_SET);
    auto bytes = hdr.e_shentsize * hdr.e_shnum;
    shdrs = new Elf64_Shdr[hdr.e_shnum];
    _EI(read, fd, shdrs, bytes);

    return 0;
  }

  unsigned int get_sect_header_num() const {
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

    _EI(lseek, fd, shdr->sh_offset, SEEK_SET);
    _EI(read, fd, shstrtab, shdr->sh_size);

    return 0;
  }

  const char* get_shstrtab() const {
    return shstrtab;
  }

  unsigned int get_shstrtab_size() const {
    return shstrtab_bytes;
  }

  const char* get_sect_name(unsigned int ndx) const {
    assert(ndx < get_sect_header_num());
    auto shdr = get_sect_headers() + ndx;
    assert(shdr->sh_name < get_shstrtab_size());
    return get_shstrtab() + shdr->sh_name;
  }

  int find_section(const char* name) const {
    for (unsigned int i = 0; i < get_sect_header_num(); i++) {
      if (strcmp(name, get_sect_name(i)) == 0) {
        return i;
      }
    }
    return -1;
  }

  int parse_dynsym() {
    assert(hdr_valid);
    assert(shdrs);
    assert(shstrtab);

    auto dynsym_ndx = find_section(".dynsym");
    assert(dynsym_ndx >= 0);
    auto shdr = get_sect_headers() + dynsym_ndx;
    assert(shdr->sh_entsize == sizeof(Elf64_Sym));
    dynsym_num = shdr->sh_size / shdr->sh_entsize;
    dynsym = new Elf64_Sym[dynsym_num];
    _EI(lseek, fd, shdr->sh_offset, SEEK_SET);
    _EI(read, fd, dynsym, shdr->sh_size);
    return 0;
  }

  Elf64_Sym* get_dynsym() const {
    return dynsym;
  }

  unsigned int get_dynsym_num() const {
    return dynsym_num;
  }

  int parse_dynstr() {
    assert(hdr_valid);
    assert(shdrs);
    assert(shstrtab);

    auto dynstr_ndx = find_section(".dynstr");
    assert(dynstr_ndx >= 0);
    auto shdr = get_sect_headers() + dynstr_ndx;
    dynstr = new char[shdr->sh_size];
    _EI(lseek, fd, shdr->sh_offset, SEEK_SET);
    _EI(read, fd, dynstr, shdr->sh_size);
    dynstr_bytes = shdr->sh_size;
    return 0;
  }

  const char* get_dynstr() const {
    return dynstr;
  }

  unsigned int get_dynstr_size() const {
    return dynstr_bytes;
  }

  const char* get_dynsym_name(unsigned int ndx) const {
    assert(ndx < get_dynsym_num());
    auto sym = get_dynsym() + ndx;
    assert(sym->st_name < get_dynstr_size());
    return get_dynstr() + sym->st_name;
  }

  int find_dynsym(const char* name) {
    for (unsigned int i = 0; i < get_dynsym_num(); i++) {
      if (strcmp(name, get_dynsym_name(i)) == 0) {
        return i;
      }
    }
    return -1;
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

  Elf64_Sym* dynsym;
  int dynsym_num;
  char* dynstr;
  int dynstr_bytes;
};

const char* libmosingar_so_path = "../sandbox/libmosingar.so";

namespace {
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
 *    object,
 *  - a list of relocation records, and
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
static trapped_shellcode*
prepare_shellcode(unsigned long global_flags) {
  auto so_path = libmosingar_so_path;

  ElfParser solib(so_path);
  solib.open();
  solib.parse_header();
  solib.parse_prog_headers();
  solib.parse_dyanmic();
  solib.parse_sect_headers();
  solib.parse_shstrtab();
  solib.parse_dynsym();
  solib.parse_dynstr();

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
  rela_num++;                   // for global_flags in bootstrap.cpp
  std::unique_ptr<void *[]> rela(new void*[rela_num * 2 + 1]);
  Elf64_Rela rela_ent;
  _ENull(lseek, solib.get_fd(), rela_offset, SEEK_SET);
  for (unsigned int i = 0; i < (rela_num - 1); i++) {
    _ENull(read, solib.get_fd(), &rela_ent, rela_entsize);
    auto rela_type = 0xffffffff & rela_ent.r_info;
    // Assume only this type of entries are there.
    assert(rela_type == R_X86_64_RELATIVE ||
           rela_type == R_X86_64_GLOB_DAT ||
           rela_type == R_X86_64_64);
    rela[i * 2] = (void*)rela_ent.r_offset;
    switch (rela_type) {
    case R_X86_64_RELATIVE:
      rela[i * 2 + 1] = (void*)rela_ent.r_addend;
      break;

    case R_X86_64_GLOB_DAT:
    case R_X86_64_64:
      auto sym = solib.get_dynsym() + (rela_ent.r_info >> 32);
      rela[i * 2 + 1] = (void*)sym->st_value;
      break;
    }
  }
  // By hacking addend of a relocation, we can change the value of a
  // global variable, global_flags is our target.  However, the value
  // will be relocated with the real address of the variable.  By
  // subtracting the value of the variable with the address of itself,
  // the real value can be recovered.
  auto global_flags_ndx = solib.find_dynsym("global_flags");
  assert(global_flags_ndx >= 0);
  auto global_flags_sym = solib.get_dynsym() + global_flags_ndx;
  rela[rela_num * 2 - 2] = (void*)global_flags_sym->st_value;
  rela[rela_num * 2 - 1] = (void*)(global_flags + global_flags_sym->st_value);
  rela[rela_num * 2] = nullptr;

  auto funcall_trap_bytes =
    (char*)shellcode_funcall_trap_end - (char*)shellcode_funcall_trap;
  auto loader_bytes = (char*)loader_end - (char*)loader_start;
  auto shellcode = new trapped_shellcode;
  shellcode->base = nullptr;

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
  auto rela_bytes = sizeof(void*) * (rela_num * 2 + 1);
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

} // namespace

namespace flightdeck {
/**
 * Make a scout taking off for a subject/process.
 *
 * |scout_takeoff()| inject the loader to the target prcoess to load
 * libmosingar.so sandboxing the process.
 */
long
scout_takeoff(pid_t pid, unsigned long global_flags) {
  std::unique_ptr<trapped_shellcode> shellcode(prepare_shellcode(global_flags));

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

} // namespace flightdeck

#ifdef TEST

#include <sys/ptrace.h>
#include <sys/wait.h>

void
child() {
  for (int i = 0; i < 7; i++) {
    printf("child %d\n", i);
    sleep(1);
  }
  open("/dev/null", O_RDONLY);
  sleep(1);
  write(1, "\nExit child\n", 12);
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

  printf("takeoff scout\n");
  r = flightdeck::scout_takeoff(pid, 0);
  if (r < 0) {
    printf("fails to takeoff scout (%lx)!\n", r);
    return;
  }

  ptrace_getregs(pid, regs);
  printf("rip %llx\n", regs.rip);

  printf("cont\n");
  r = ptrace_cont(pid);
  if (r < 0) {
    return;
  }

  printf("Go to sleep 10s\n");
  sleep(10);
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
