/*
 * pc_os.h: the few virtual-memory calls the platform layer needs, one implementation per operating system
 * (pc_mem_win32.c: VirtualAlloc / VirtualProtect; pc_mem_posix.c: mmap / mprotect). Internal to port/src.
 */
#ifndef PC_OS_H
#define PC_OS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PC_OS_PAGE 4096u

/* Reserve address space at exactly `base` (no memory yet). Returns 1 on success. */
int pc_os_reserve_fixed(uintptr_t base, size_t size);
/* Make [addr, addr + size) of a reservation usable (read / write, zeroed on first use). Returns 1 on success. */
int pc_os_commit(uintptr_t addr, size_t size);
/* Make pages inaccessible (guard = 1) or read / write again (guard = 0). Returns 1 on success. */
int pc_os_guard(uintptr_t addr, size_t size, int guard);
/* Base address of the running executable image. */
uintptr_t pc_os_image_base(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_OS_H */
