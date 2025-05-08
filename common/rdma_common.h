#pragma once
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <syslog.h>
#include <arpa/inet.h>
#include <infiniband/mlx5dv.h>

#ifdef __linux__
#include <unistd.h>
#endif

#define STATUS int

enum STATUS_NUM{
    STATUS_OK,
    STATUS_ERR,
    STATUS_NO_DATA,
    STATUS_NO_MEM,
    STATUS_NOT_IMPLEMENTED,
    STATUS_INVALID_PARAM,
    STATUS_INVALID_STATE,
    STATUS_INVALID_OBJECT,
    STATUS_INVALID_OPERATION,
    STATUS_INVALID_ADDRESS,
    STATUS_INVALID_LENGTH,
    STATUS_INVALID_VALUE,
    STATUS_INVALID_SIZE,
    STATUS_INVALID_ALIGNMENT,
    STATUS_INVALID_HANDLE
};

inline bool FAILED(STATUS status) {
    return status != STATUS_OK;
}

#define RETURN_IF_FAILED(expr) \
    do { \
        STATUS _status = (expr); \
        if (FAILED(_status)) { \
            printf("Error: %s:%d in %s with status %d\n", __FILE__, __LINE__, __FUNCTION__, _status); \
            return _status; \
        } \
    } while (0)

// Mark unused static functions as unused to silence warnings
static uint32_t ilog2(uint32_t x) __attribute__((unused));
static uint32_t ilog2(uint32_t x) {
    uint32_t r = 0;
    while ((1U << r) < x) { ++r; }
    return r;
}

inline unsigned get_page_size_log() {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        // Fallback to 4KB
        return 12;
    }
    unsigned log_val = 0;
    while ((1UL << log_val) < static_cast<unsigned long>(page_size)) {
        log_val++;
    }
    return log_val;
}

inline unsigned get_page_size() {
    return 1UL << get_page_size_log();
}

inline size_t get_cache_line_size() {
#ifdef __linux__
    long cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (cache_line_size <= 0) {
        // Fallback to common 64 bytes if cannot determine
        return 64;
    }
    return static_cast<size_t>(cache_line_size);
#else
    // Fallback to common 64 bytes for non-Linux systems
    return 64;
#endif
}

enum LogLevel {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2
};

#ifdef LOG_LEVEL
static LogLevel g_log_level = static_cast<LogLevel>(LOG_LEVEL);
#else
static LogLevel g_log_level = LOG_LEVEL_ERROR;
#endif

inline void set_log_level(LogLevel level) {
    g_log_level = level;
}

// Corrected log_error
inline void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

inline void log_debug(const char* format, ...) {
    if (g_log_level >= LOG_LEVEL_DEBUG) {
        va_list args;
        va_start(args, format);
        fprintf(stdout, "[DEBUG] ");
        vfprintf(stdout, format, args);
        fprintf(stdout, "\n");
        va_end(args);
    }
}

inline void log_info(const char* format, ...) {
    if (g_log_level >= LOG_LEVEL_INFO) {
        va_list args;
        va_start(args, format);
        fprintf(stdout, "[INFO] ");
        vfprintf(stdout, format, args);
        fprintf(stdout, "\n");
        va_end(args);
    }
}

#define tlx_typeof(_type) \
    __typeof__(_type)

#define tlx_padding(_n, _alignment) \
    (((_alignment) - (_n) % (_alignment)) % (_alignment))


#define tlx_align_down(_n, _alignment) ((_n) - ((_n) % (_alignment)))


#define tlx_align_up(_n, _alignment) ((_n) + tlx_padding(_n, _alignment))


#define tlx_align_down_pow2(_n, _alignment) ((_n) & ~((_alignment)-1))


#define tlx_align_up_pow2(_n, _alignment) \
    tlx_align_down_pow2((_n) + (_alignment)-1, _alignment)


#define tlx_align_down_pow2_ptr(_ptr, _alignment) \
    ((tlx_typeof(_ptr))tlx_align_down_pow2((uintptr_t)(_ptr), (_alignment)))


#define tlx_align_up_pow2_ptr(_ptr, _alignment) \
    ((tlx_typeof(_ptr))tlx_align_up_pow2((uintptr_t)(_ptr), (_alignment)))


#define tlx_roundup_pow2(_n) \
    ({ \
        tlx_typeof(_n) pow2; \
        static_assert((_n) >= 1); \
        for (pow2 = 1; pow2 < (_n); pow2 <<= 1) \
            ; \
        pow2; \
    })


#define tlx_rounddown_pow2(_n) (ucs_roundup_pow2(_n + 1) / 2)


#define tlx_roundup_pow2_or0(_n) (((_n) == 0) ? 0 : tlx_roundup_pow2(_n))


/* Return values: 0 - aligned, non-0 - unaligned */
#define tlx_check_if_align_pow2(_n, _p) ((_n) & ((_p)-1))


/* Return values: off-set from the alignment */
#define tlx_padding_pow2(_n, _p) tlx_check_if_align_pow2(_n, _p)


static unsigned __tlx_ilog2_u32(uint32_t n)
{
#if defined(__aarch64__) || defined(__arm__)
    int bit;
    asm ("clz %w0, %w1" : "=r" (bit) : "r" (n));
    return 31 - bit;
#else
    // Use compiler builtin for x86/x64 and other architectures
    return n ? 31 - __builtin_clz(n) : 0;
#endif
}

static unsigned __tlx_ilog2_u64(uint64_t n)
{
#if defined(__aarch64__) || defined(__arm__)
    int64_t bit;
    asm ("clz %0, %1" : "=r" (bit) : "r" (n));
    return 63 - bit;
#else
    // Use compiler builtin for x86/x64 and other architectures
    return n ? 63 - __builtin_clzll(n) : 0;
#endif
}

static unsigned tlx_ffs32(uint32_t n) __attribute__((unused));
static unsigned tlx_ffs32(uint32_t n)
{
    return __tlx_ilog2_u32(n & -n);
}

static unsigned tlx_ffs64(uint64_t n) __attribute__((unused));
static unsigned tlx_ffs64(uint64_t n)
{
    return __tlx_ilog2_u64(n & -n);
}

/* The i-th bit */
#define TLX_BIT(i)               (1ul << (i))

/* Mask of bits 0..i-1 */
#define TLX_MASK(_i)             (((_i) >= 64) ? ~0 : (TLX_BIT(_i) - 1))

/* The i-th bit */
#define TLX_BIT_GET(_value, _i)  (!!((_value) & TLX_BIT(_i)))

#define unlikely(x)      __builtin_expect(!!(x), 0)

//==============================================================================
// template memory allocation
//==============================================================================
template <typename T>
T* aligned_alloc(size_t size, size_t* allocated_size = nullptr) {
    const size_t page_line_size = get_page_size();
    const size_t alignment = (alignof(T) < page_line_size) ? page_line_size : alignof(T);
    
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size * sizeof(T)) != 0) {
        return nullptr;
    }

    log_debug("Allocated %zu bytes at %p with alignment %zu", size * sizeof(T), ptr, alignment);
    memset(ptr, 0, size * sizeof(T));  // Updated to use size instead of *allocated_size

    if (allocated_size) {
        *allocated_size = size * sizeof(T);
    }

    return static_cast<T*>(ptr);
}

#define MLX5_ALWAYS_INLINE      inline __attribute__ ((always_inline))

static MLX5_ALWAYS_INLINE
void mlx5_set_data_seg(struct mlx5_wqe_data_seg *seg,
			           uint32_t length, uint32_t lkey,
			           uintptr_t address)
{
	seg->byte_count = htonl(length);
	seg->lkey       = htonl(lkey);
	seg->addr       = htobe64((uintptr_t)address);
}

static MLX5_ALWAYS_INLINE
void mlx5_set_ctrl_qpn_ds(struct mlx5_wqe_ctrl_seg *ctrl, uint32_t qp_num, uint8_t ds)
{
    ctrl->qpn_ds = htonl((qp_num << 8) | ds);
}

static MLX5_ALWAYS_INLINE
void mlx5_set_ctrl_seg(struct mlx5_wqe_ctrl_seg *seg, uint16_t pi,
			           uint8_t opcode, uint8_t opmod, uint32_t qp_num,
			           uint8_t fm_ce_se, uint8_t ds,
			           uint8_t signature, uint32_t imm)
{
	seg->opmod_idx_opcode      = htobe32(((pi & 0xffff) << 8) | opcode | (opmod << 24));
    mlx5_set_ctrl_qpn_ds(seg, qp_num, ds);
	seg->fm_ce_se		       = fm_ce_se;
    seg->signature		       = signature;
    seg->dci_stream_channel_id = 0;
    seg->imm		           = imm;
	/*
	 * The caller should prepare "imm" in advance based on WR opcode.
	 * For IBV_WR_SEND_WITH_IMM and IBV_WR_RDMA_WRITE_WITH_IMM,
	 * the "imm" should be assigned as is.
	 * For the IBV_WR_SEND_WITH_INV, it should be htobe32(imm).
	 */

}

static MLX5_ALWAYS_INLINE
void mlx5_set_rdma_seg(struct mlx5_wqe_raddr_seg *raddr, 
                       void* rdma_raddr, uintptr_t rdma_rkey)
{

    raddr->raddr = htobe64((intptr_t)rdma_raddr);
    raddr->rkey  = htonl(rdma_rkey);
    raddr->reserved = 0;
}