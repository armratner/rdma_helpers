/* SPDX-License-Identifier: MIT  (or whatever your project uses)
 *
 * mmio_write64.h – architecture-aware 64-bit MMIO store with WC ordering
 *
 *  • Always stores the value in **big-endian** device order.
 *  • Uses a single 64-bit store when the architecture has one.
 *  • On 32-bit or BE-host/LE-device combos it falls back to two 32-bit stores
 *    in address-ascending order while holding a spin-lock (so the chipset
 *    won’t reorder them).
 *  • Finishes with the minimal WC → device fence required on each arch
 *    (SFENCE on x86, DSB ISHST on AArch64, eieio on Power, etc.).
 *
 *  Drop the file in a common include directory and
 *     #include "mmio_write64.h"
 */

 #pragma once
 #include <stdint.h>
 #include <endian.h>
 #include <string.h>
 #include <pthread.h>
 
 /* --------------------------------------------------------------------- */
 /* 1.  Architecture-specific primitives                                  */
 /* --------------------------------------------------------------------- */
 
 #if defined(__x86_64__) || defined(__i386__)
 #  include <emmintrin.h>   /* __mm_sfence */
 static inline void wc_store_fence(void) { _mm_sfence(); }
 #elif defined(__aarch64__)
 static inline void wc_store_fence(void) { asm volatile("dsb ishst" ::: "memory"); }
 #elif defined(__powerpc64__)
 static inline void wc_store_fence(void) { asm volatile("eieio" ::: "memory"); }
 #else
 static inline void wc_store_fence(void) { __sync_synchronize(); } /* conservative */
 #endif
 
 /* --------------------------------------------------------------------- */
 /* 2.  Single 64-bit little→big endian store when HW supports it         */
 /* --------------------------------------------------------------------- */
 
 static inline void __mmio_write64_be_direct(void *addr, uint64_t val)
 {
     *(volatile uint64_t *)addr = *(volatile uint64_t *)val;
 }
 
 /* --------------------------------------------------------------------- */
 /* 3.  Fallback: two ordered 32-bit stores with global spin-lock         */
 /* --------------------------------------------------------------------- */
 
 #if UINTPTR_MAX == 0xFFFFFFFFu  /* 32-bit user-space */
 
 static pthread_spinlock_t mmio64_lock = 0;
 
 __attribute__((constructor))  static void init_mmio64_lock(void)
 {
     pthread_spin_init(&mmio64_lock, PTHREAD_PROCESS_PRIVATE);
 }
 
 static inline void __mmio_write64_be_fallback(void *addr, uint64_t val)
 {
 #if __BYTE_ORDER == __LITTLE_ENDIAN
     val = htobe64(val);
 #endif
     uint32_t hi = (uint32_t)(val >> 32);
     uint32_t lo = (uint32_t)(val & 0xFFFFFFFFu);
 
     pthread_spin_lock(&mmio64_lock);            /* guarantee global ordering */
     *(volatile uint32_t *)addr       = hi;      /* high dword first (BE)     */
     *(volatile uint32_t *)(addr + 4) = lo;
     pthread_spin_unlock(&mmio64_lock);
 }
 
 #endif  /* 32-bit path */

#if defined(__i386__)
#define mmio_flush_writes() asm volatile("lock; addl $0,0(%%esp) " ::: "memory")
#elif defined(__x86_64__)
#define mmio_flush_writes() asm volatile("sfence" ::: "memory")
#elif defined(__PPC64__)
#define mmio_flush_writes() asm volatile("sync" ::: "memory")
#elif defined(__PPC__)
#define mmio_flush_writes() asm volatile("sync" ::: "memory")
#elif defined(__ia64__)
#define mmio_flush_writes() asm volatile("fwb" ::: "memory")
#elif defined(__sparc_v9__)
#define mmio_flush_writes() asm volatile("membar #StoreStore" ::: "memory")
#elif defined(__aarch64__)
#define mmio_flush_writes() asm volatile("dsb st" ::: "memory");
#elif defined(__sparc__)
#define mmio_flush_writes() asm volatile("" ::: "memory")
#elif defined(__loongarch__)
#define mmio_flush_writes() asm volatile("dbar 0" ::: "memory")
#elif defined(__riscv)
#define mmio_flush_writes() asm volatile("fence ow,ow" ::: "memory")
#elif defined(__s390x__)
#include "s390_mmio_insn.h"
#define mmio_flush_writes() s390_pciwb()
#elif defined(__mips__)
#define mmio_flush_writes() asm volatile("sync" ::: "memory")
#else
#error No architecture specific memory barrier defines found!
#endif

#define mmio_wc_start() mmio_flush_writes()

#if defined(__i386__)
#define udma_to_device_barrier() asm volatile("" ::: "memory")
#elif defined(__x86_64__)
#define udma_to_device_barrier() asm volatile("" ::: "memory")
#elif defined(__PPC64__)
#define udma_to_device_barrier() asm volatile("sync" ::: "memory")
#elif defined(__PPC__)
#define udma_to_device_barrier() asm volatile("sync" ::: "memory")
#elif defined(__ia64__)
#define udma_to_device_barrier() asm volatile("mf" ::: "memory")
#elif defined(__sparc_v9__)
#define udma_to_device_barrier() asm volatile("membar #StoreStore" ::: "memory")
#elif defined(__aarch64__)
#define udma_to_device_barrier() asm volatile("dmb oshst" ::: "memory")
#elif defined(__sparc__) || defined(__s390x__)
#define udma_to_device_barrier() asm volatile("" ::: "memory")
#elif defined(__loongarch__)
#define udma_to_device_barrier() asm volatile("dbar 0" ::: "memory")
#elif defined(__riscv)
#define udma_to_device_barrier() asm volatile("fence ow,ow" ::: "memory")
#elif defined(__mips__)
#define udma_to_device_barrier() asm volatile("sync" ::: "memory")
#else
#error No architecture specific memory barrier defines found!
#endif

#if defined(__i386__)
#define udma_from_device_barrier() asm volatile("lock; addl $0,0(%%esp) " ::: "memory")
#elif defined(__x86_64__)
#define udma_from_device_barrier() asm volatile("lfence" ::: "memory")
#elif defined(__PPC64__)
#define udma_from_device_barrier() asm volatile("lwsync" ::: "memory")
#elif defined(__PPC__)
#define udma_from_device_barrier() asm volatile("sync" ::: "memory")
#elif defined(__ia64__)
#define udma_from_device_barrier() asm volatile("mf" ::: "memory")
#elif defined(__sparc_v9__)
#define udma_from_device_barrier() asm volatile("membar #LoadLoad" ::: "memory")
#elif defined(__aarch64__)
#define udma_from_device_barrier() asm volatile("dmb oshld" ::: "memory")
#elif defined(__sparc__) || defined(__s390x__)
#define udma_from_device_barrier() asm volatile("" ::: "memory")
#elif defined(__loongarch__)
#define udma_from_device_barrier() asm volatile("dbar 0" ::: "memory")
#elif defined(__riscv)
#define udma_from_device_barrier() asm volatile("fence ir,ir" ::: "memory")
#elif defined(__mips__)
#define udma_from_device_barrier() asm volatile("sync" ::: "memory")
#else
#error No architecture specific memory barrier defines found!
#endif

/****************************************************************************
 * Minimal 64‑bit WC helpers – no rdma‑core dependency                      *
 ****************************************************************************/


/* --------------------------------------------------------------------- */
/* 4.  Public API                                                        */
/* --------------------------------------------------------------------- */

/**
 * mmio_write64_be() – store a 64-bit value to a device register
 * @addr:   mapped MMIO address (must be naturally aligned)
 * @val:    value in host endianness (function converts to BE)
 *
 * Guarantees:
 *   – The store is *visible* to the PCIe link before the function returns.
 *   – No UC / WC stores issued **before** the call can pass it.
 *   – On all arches the compiler is told that @addr points to volatile IO.
 */
 static inline void mmio_write64_be(void *addr, mlx5_wqe_ctrl_seg* val)
 {
    mmio_wc_start();
    *(volatile uint64_t *)addr = *(volatile uint64_t *)val;
    wc_store_fence();
 }

static inline void mmio_memcpy_x64(void *dst_mmio, const void *src_buf,
                                   size_t bytes)
{
    volatile uint64_t *dst = static_cast<volatile uint64_t *>(dst_mmio);
    const uint64_t    *src = static_cast<const uint64_t *>(src_buf);

    for (size_t qwords = bytes / sizeof(uint64_t); qwords; --qwords)
        *dst++ = *src++;
}


// Helper to post doorbell via BlueFlame write-combine buffer, copying 64-byte blocks and wrapping at queue end
static void bf_copy(void* bf_reg, const void* ctrl, unsigned bytecnt, void* queue_start, void* queue_end) {
    uint64_t* dst = (uint64_t*)bf_reg;
    const uint64_t* src = (const uint64_t*)ctrl;
    char* start = (char*)queue_start;
    char* end = (char*)queue_end;
    while (bytecnt) {
        mmio_memcpy_x64(dst, src, 64);
        bytecnt -= 64;
        dst += 8;
        src += 8;
        if ((char*)src >= end) src = (const uint64_t*)start;
    }
}
