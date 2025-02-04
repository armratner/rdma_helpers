#pragma once
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <chrono>

#include "mlx5_ifc.h"
#include "common.h"
#include "profiler_singleton.h"

class base_object {
public:
    base_object() : _initialized(false) {}
    virtual ~base_object() = default;
    virtual void destroy() = 0;

    template<typename T, typename... Args>
    STATUS initialize(Args&&... args) {
        if (_initialized) {
            return STATUS_OK;
        }
        T* derived = static_cast<T*>(this);
        STATUS res = derived->initialize(std::forward<Args>(args)...);
        _initialized = (res == STATUS_OK);
        return res;
    }

    bool is_initialized() const { 
        return _initialized; 
    }

protected:
    bool _initialized;
};

class rdma_device : public base_object {
public:
    rdma_device();
    ~rdma_device();
    
    void destroy() override;
    STATUS initialize(const std::string& device_name);
    void print_device_attr() const;
    
    struct ibv_device* get() const;
    struct ibv_context* get_context() const;
    const ibv_device_attr* get_device_attr() const;

private:
    struct ibv_device** _device_list;
    struct ibv_device* _device;
    struct ibv_context* _context;
    struct ibv_device_attr* _device_attr;
};

class protection_domain : public base_object {
public:
    protection_domain();
    ~protection_domain();
    void destroy() override;
    STATUS initialize(struct ibv_context* context);
    struct ibv_pd* get() const;
    uint32_t get_pdn() const;

private:
    struct ibv_pd* _pd;
    uint32_t _pdn;
};

class user_memory : public base_object {
public:
    user_memory();
    ~user_memory();
    void destroy() override;
    STATUS initialize(ibv_context* context, size_t size);
    mlx5dv_devx_umem* get() const;
    void* addr() const;
    size_t size() const;
    uint32_t umem_id() const;

private:
    mlx5dv_devx_umem* _umem;
    void* _addr;
    size_t _size;
    uint32_t _umem_id;
};

class completion_queue : public base_object {
public:
    completion_queue();
    ~completion_queue();
    void destroy() override;
    STATUS initialize(struct ibv_context* context, int cqe);
    struct ibv_cq_ex* get() const;
    uint32_t get_cqn() const;
    STATUS wait_for_event_and_poll(ibv_wc* wc);
    STATUS wait_for_event_and_poll_timestamps();

private:
    STATUS create_completion_channel(struct ibv_context* context);
    void destroy_completion_channel();
    STATUS poll_cq(ibv_wc* wc);

    struct ibv_comp_channel* _channel;
    struct ibv_cq_ex* _cq;
    struct mlx5dv_cq* _dv_cq;
    rdma_profiler_singleton* _profiler;
};

class uar : public base_object {
public:
    uar();
    ~uar();
    void destroy() override;
    STATUS initialize(ibv_context* ctx);
    mlx5dv_devx_uar* get() const;

private:
    mlx5dv_devx_uar* _uar;
};

class wqe_builder {
public:
    wqe_builder(uint8_t* wqe_ptr) : wqe_(wqe_ptr) {
        memset(wqe_, 0, WQE_STRIDE);
    }

    void build_ctrl(wqe_op op, uint32_t qpn, uint32_t wqe_idx, bool signal) {
        auto* ctrl = get_seg<mlx5_wqe_ctrl_seg>(0);
        ctrl->opmod_idx_opcode = (static_cast<uint32_t>(op) << 24) | (wqe_idx & 0xffffff);
        ctrl->qpn_ds = (qpn << 8) | DS_CNT;
        ctrl->fm_ce_se = signal ? MLX5_WQE_CTRL_CQ_UPDATE : 0;
    }

    void build_raddr(uint64_t remote_addr, uint32_t rkey) {
        auto* raddr = get_seg<mlx5_wqe_raddr_seg>(16);
        raddr->raddr = remote_addr;
        raddr->rkey = rkey;
    }

    void build_data(uint64_t addr, uint32_t lkey, uint32_t length) {
        auto* dseg = get_seg<mlx5_wqe_data_seg>(32);
        dseg->addr = addr;
        dseg->lkey = lkey;
        dseg->byte_count = length;
    }

private:
    template<typename T>
    T* get_seg(uint32_t offset) { return reinterpret_cast<T*>(wqe_ + offset); }

    static constexpr uint32_t WQE_STRIDE = 64;
    static constexpr uint8_t DS_CNT = 3;
    uint8_t* wqe_;
};

class sq_manager {
public:
    void init(void* base, uint32_t size) {
        base_ = static_cast<uint8_t*>(base);
        size_ = size;
    }

    wqe_builder get_wqe() {
        uint32_t idx = head_ % size_;
        return wqe_builder(base_ + (idx * WQE_STRIDE));
    }

    uint32_t get_idx() const { return head_ % size_; }
    void advance() { head_++; }
    uint32_t get_head() const { return head_; }

private:
    static constexpr uint32_t WQE_STRIDE = 64;
    uint8_t* base_ = nullptr;
    uint32_t size_ = 0;
    uint32_t head_ = 0;
};

class queue_pair : public base_object {
public:
    struct qp_params {
        uint32_t sq_size;
        uint32_t rq_size;
        uint32_t max_send_wr;
        uint32_t max_recv_wr;
        uint32_t max_send_sge;
        uint32_t max_recv_sge;
        volatile uint64_t*   uarMap=nullptr;
        
        qp_params() 
            : sq_size(32)
            , rq_size(32)
            , max_send_wr(32)
            , max_recv_wr(32)
            , max_send_sge(1)
            , max_recv_sge(1)
        {}

        qp_params(uint32_t sq, uint32_t rq,
                  uint32_t send_wr, uint32_t recv_wr,
                  uint32_t send_sge, uint32_t recv_sge)
            : sq_size(sq)
            , rq_size(rq)
            , max_send_wr(send_wr)
            , max_recv_wr(recv_wr)
            , max_send_sge(send_sge)
            , max_recv_sge(recv_sge)
        {}

        qp_params& set_sq_size(uint32_t size) {
            sq_size = size;
            return *this;
        }
        qp_params& set_rq_size(uint32_t size) {
            rq_size = size;
            return *this;
        }
        qp_params& set_max_send_wr(uint32_t val) {
            max_send_wr = val;
            return *this;
        }
        qp_params& set_max_recv_wr(uint32_t val) {
            max_recv_wr = val;
            return *this;
        }
        qp_params& set_max_send_sge(uint32_t val) {
            max_send_sge = val;
            return *this;
        }
        qp_params& set_max_recv_sge(uint32_t val) {
            max_recv_sge = val;
            return *this;
        }
    };

    queue_pair() 
        : _qp(nullptr)
        , _qpn(0)
        , _db_record(nullptr)
        , _uar_map(nullptr)
    {
        _sq.init(nullptr, 0);
    }

    queue_pair(uint32_t qpn, void* db_record, volatile uint64_t* uar_map)
        : _qp(nullptr)
        , _qpn(qpn)
        , _db_record(db_record)
        , _uar_map(uar_map)
    {
        _sq.init(nullptr, 0);
    }

    void set_sq_info(void* base, uint32_t size) {
        _sq.init(base, size);
    }

    ~queue_pair();
    
    void destroy() override;
    
    STATUS initialize(
        ibv_context* ctx,
        uint32_t pdn,
        uint32_t cqn,
        uint32_t umem_id_sq,
        uint32_t umem_id_db,
        const qp_params& params = qp_params()
    );
    
    STATUS reset_to_init();
    STATUS init_to_rtr(uint32_t remote_qpn, uint32_t remote_psn);
    STATUS rtr_to_rts(uint32_t psn);
    
    mlx5dv_devx_obj* get() const;
    uint32_t get_qpn() const;

    STATUS post_send(uintptr_t addr, uint32_t lkey, size_t length, bool signal = true) {
        return post_op(wqe_op::SEND, 0, 0, addr, lkey, length, signal);
    }

    STATUS post_write(uint64_t remote_addr, uint32_t rkey,
                     uintptr_t local_addr, uint32_t local_lkey,
                     size_t length, bool signal = true) {
        return post_op(wqe_op::RDMA_WRITE, remote_addr, rkey, 
                      local_addr, local_lkey, length, signal);
    }

    STATUS post_read(uint64_t remote_addr, uint32_t rkey,
                    uintptr_t local_addr, uint32_t local_lkey,
                    size_t length, bool signal = true) {
        return post_op(wqe_op::RDMA_READ, remote_addr, rkey,
                      local_addr, local_lkey, length, signal);
    }

    uint32_t get_rkey() const { return _rkey; }
    uint64_t get_addr() const { return _addr; }

private:
    STATUS post_op(wqe_op op, uint64_t remote_addr, uint32_t rkey,
                  uintptr_t local_addr, uint32_t local_lkey,
                  size_t length, bool signal);

    mlx5dv_devx_obj* _qp;
    sq_manager _sq;
    void* _db_record;
    volatile uint64_t* _uar_map;
    uint32_t _qpn;
    rdma_profiler_singleton* _profiler;

    uint32_t _rkey{0};
    uint64_t _addr{0};

private:
    // Add helper methods
    static uint32_t ilog2(uint32_t x) {
        uint32_t r = 0;
        while ((1U << r) < x) { ++r; }
        return r;
    }

    // Bit positions for doorbell fields
    static constexpr uint32_t DB_HEAD_SHIFT = 48;
    static constexpr uint32_t DB_HEAD_MASK = 0xffff;
    static constexpr uint32_t DB_CNT_SHIFT = 30;  // Make sure this is less than 32
    static constexpr uint32_t DB_CNT_VAL = 1;
    static constexpr uint32_t DB_OP_SHIFT = 24;
    static constexpr uint32_t DB_IDX_MASK = 0xffffff;

    // Pack doorbell value components
    uint64_t pack_doorbell(uint32_t head, wqe_op op, uint32_t idx) const {
        return (static_cast<uint64_t>(head & DB_HEAD_MASK) << DB_HEAD_SHIFT) |
               (static_cast<uint64_t>(DB_CNT_VAL) << DB_CNT_SHIFT) |
               (static_cast<uint32_t>(op) << DB_OP_SHIFT) |
               (idx & 0xFFFF);
    }
};
