#pragma once
#include <cstdio>
#include <cstdint>
#include <syslog.h>

#define STATUS int

enum STATUS_NUM{
    STATUS_OK,
    STATUS_ERR,
    STATUS_NO_DATA,
    STATUS_NO_MEM
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


#ifndef MLX5_CMD_OP_CREATE_QP
#define MLX5_CMD_OP_CREATE_QP 0x500
#endif
#ifndef MLX5_CMD_OP_RST2INIT_QP
#define MLX5_CMD_OP_RST2INIT_QP 0x502
#endif
#ifndef MLX5_CMD_OP_INIT2RTR_QP
#define MLX5_CMD_OP_INIT2RTR_QP 0x503
#endif
#ifndef MLX5_CMD_OP_RTR2RTS_QP
#define MLX5_CMD_OP_RTR2RTS_QP 0x504
#endif

#ifdef __linux__
#include <unistd.h>
#endif

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

enum class wqe_op : uint8_t {
    SEND = 0x0,
    RDMA_WRITE = 0x8,
    RDMA_READ = 0x10,
    ATOMIC_CS = 0x11,
    ATOMIC_FA = 0x12,
};

constexpr static uint32_t WQE_STRIDE = 64;