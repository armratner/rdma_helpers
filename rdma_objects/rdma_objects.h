#pragma once

#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <chrono>
#include <vector>
#include <map>

#include "mlx5_ifc.h"
#include "../common/rdma_common.h"
#include "../common/auto_ref.h"


inline void dump_wqe(unsigned char* wqe_buf) {
    for (int i = 0; i < 64; i += 16) {
        log_debug("WQE [%02x]: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                  i,
                  wqe_buf[i+0], wqe_buf[i+1], wqe_buf[i+2], wqe_buf[i+3], 
                  wqe_buf[i+4], wqe_buf[i+5], wqe_buf[i+6], wqe_buf[i+7],
                  wqe_buf[i+8], wqe_buf[i+9], wqe_buf[i+10], wqe_buf[i+11], 
                  wqe_buf[i+12], wqe_buf[i+13], wqe_buf[i+14], wqe_buf[i+15]);
    }
}

using namespace std;

//==============================================================================
// HCA Capabilities Structure
//==============================================================================

struct hca_capabilities {
    uint8_t log_max_srq_sz;
    uint8_t log_max_qp_sz;
    uint8_t log_max_qp;
    uint8_t log_max_srq;
    uint8_t log_max_cq_sz;
    uint8_t log_max_cq;
    uint8_t log_max_eq_sz;
    uint8_t log_max_mkey;
    uint8_t log_max_eq;
    uint8_t log_max_klm_list_size;
    uint8_t log_max_ra_req_qp;
    uint8_t log_max_ra_res_qp;
    uint8_t log_max_msg;
    uint8_t max_tc;
    uint8_t log_max_mcg;
    uint8_t log_max_pd;
    uint8_t log_max_xrcd;
    uint8_t log_max_rq;
    uint8_t log_max_sq;
    uint8_t log_max_tir;
    uint8_t log_max_tis;
    uint8_t log_max_rmp;
    uint8_t log_max_rqt;
    uint8_t log_max_rqt_size;
    uint8_t log_max_tis_per_sq;
    uint8_t log_max_stride_sz_rq;
    uint8_t log_min_stride_sz_rq;
    uint8_t log_max_stride_sz_sq;
    uint8_t log_min_stride_sz_sq;
    uint8_t log_max_hairpin_queues;
    uint8_t log_max_hairpin_wq_data_sz;
    uint8_t log_max_hairpin_num_packets;
    uint8_t log_max_wq_sz;
    uint8_t log_max_vlan_list;
    uint8_t log_max_current_mc_list;
    uint8_t log_max_current_uc_list;
    uint8_t log_max_transport_domain;
    uint8_t log_max_flow_counter_bulk;
    uint8_t log_max_l2_table;
    uint8_t log_uar_page_sz;
    uint8_t log_max_pasid;
    uint8_t log_max_dct_connections;
    uint8_t log_max_atomic_size_qp;
    uint8_t log_max_atomic_size_dc;
    uint8_t log_max_xrq;
    uint8_t native_port_num;
    uint8_t num_ports;
};

//==============================================================================
// Base class for all RDMA objects
//==============================================================================

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

//==============================================================================
// RDMA Device
//==============================================================================

class rdma_device : public base_object {
public:
    rdma_device();
    ~rdma_device();
    
    void destroy() override;
    STATUS initialize(const std::string& device_name);
    STATUS query_port_attr();
    
    void print_device_attr() const;
    void print_port_attr() const;
    void print_port_dv_attr() const;

    uint8_t get_port_num() const;
    struct ibv_device* get() const;
    struct ibv_context* get_context() const;
    const ibv_device_attr* get_device_attr() const;
    const ibv_port_attr* get_port_attr(uint8_t port_num) const;
    STATUS query_hca_capabilities();

    struct hca_capabilities get_hca_cap() const {
        return _hca_cap;
    }

private:
    struct ibv_device** _device_list;
    struct ibv_device* _device;
    struct ibv_context* _context;
    struct ibv_device_attr* _device_attr;
    map<uint8_t, struct ibv_port_attr*> _port_attr_map;
    map<uint8_t, struct mlx5dv_port*> _port_dv_attr_map;
    uint8_t _port_num;
    struct hca_capabilities _hca_cap; 
};

//==============================================================================
// Protection Domain
//==============================================================================

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

//==============================================================================
// User Memory
//==============================================================================

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
    void* get_umem_buf() const { return _umem_buf; }

private:
    mlx5dv_devx_umem* _umem;
    size_t _size;
    uint32_t _umem_id;
    void* _umem_buf;
};

//==============================================================================
// UAR
//==============================================================================

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

//==============================================================================
// Mkey
//==============================================================================

class memory_key : public base_object {
public:
    memory_key();
    ~memory_key();
    void destroy() override;
    STATUS initialize(ibv_pd* pd, uint32_t access, uint32_t num_entries);
    mlx5dv_mkey* get_mkey() const;
    uint32_t get_lkey() const;
    uint32_t get_rkey() const;
private:
    mlx5dv_mkey* _mkey;
};

//==============================================================================
// completion queue
//==============================================================================

struct cq_creation_params {
    ibv_context* context;
    ibv_cq_init_attr_ex *cq_attr_ex;
    mlx5dv_cq_init_attr *dv_cq_attr;
};

struct cq_hw_params
{
    uint8_t  log_cq_size              = 9;
    uint8_t  log_page_size            = 12; // Added for CQ page size
    uint8_t  cqe_sz                   = 1;
    bool     cqe_comp_en              = false;
    uint8_t  cqe_comp_layout          = 0;
    uint8_t  mini_cqe_res_format      = 0;
    uint8_t  cq_timestamp_format      = 0;
    /* ---- Moderation -------------------------------------------------- */
    uint8_t  cq_period_mode           = 0;
    uint16_t cq_period                = 0;
    uint16_t cq_max_count             = 0;
    /* ---- Misc flags -------------------------------------------------- */
    bool     scqe_break_moderation_en = false;
    bool     oi                       = false;
    bool     cc                       = false;
    bool     as_notify                = false;
    uint8_t  st                       = 0;
};

class completion_queue_devx : public base_object {
    public:
        completion_queue_devx();
        ~completion_queue_devx();
        void destroy() override;
        STATUS initialize(rdma_device* rdevice, cq_hw_params& params);

        STATUS poll_cq();
        STATUS arm_cq(int solicited = 0);

        void cq_event() { _arm_sn++; }
        struct mlx5dv_devx_obj* get() const { return _cq; }
        uint32_t get_cqn() const { return _cqn; }

        STATUS initialize_cq_resources(rdma_device* rdevice, cq_hw_params& params);
        void destroy_cq_resources();

        void set_cq_hw_params(cq_hw_params& params);
        cq_hw_params get_cq_hw_params() const;
    private:
        auto_ref<user_memory> _umem;
        auto_ref<user_memory> _umem_db;
        auto_ref<uar> _uar;
        rdma_device* _rdevice;
        mlx5dv_devx_obj* _cq;
        uint32_t _cqn;
        cq_hw_params _cq_hw_params;

        __uint128_t _consumer_index;
        __uint128_t _producer_index;
        __uint128_t _arm_sn;
};

//==============================================================================
// Queue Pair
//==============================================================================

struct qp_init_creation_params {
    rdma_device* rdevice;
    ibv_context* context;
    uint32_t pdn;
    uint32_t cqn;
    uar* uar_obj;
    user_memory* umem_sq;
    user_memory* umem_db;

    uint32_t sq_size;
    uint32_t rq_size;
    uint32_t max_send_wr;
    uint32_t max_recv_wr;
    uint32_t max_send_sge;
    uint32_t max_recv_sge;
    uint32_t max_inline_data;
    uint32_t max_rd_atomic;
    uint32_t max_dest_rd_atomic;
};

struct qp_init_connection_params {
    uint8_t      mtu;
    bool         ece;
    uint8_t      port_num;
    uint8_t      retry_count;
    uint8_t      rnr_retry;
    uint8_t      min_rnr_to;
    uint8_t      sl;
    uint8_t      dscp;
    uint8_t      traffic_class;
    uint32_t     remote_qpn;
    ibv_ah_attr* remote_ah_attr;
    ibv_pd*      pd;

};

struct post_params {
    uint32_t wqe_index;
    uint32_t wqe_cnt;
    uint32_t wqe_size;
    void* wqe_addr;
};

static inline void objects_get_av(struct ibv_ah *ah, struct mlx5_wqe_av *av)
{
    struct mlx5dv_obj  dv;
    struct mlx5dv_ah   dah;

    dv.ah.in = ah;
    dv.ah.out = &dah;
    mlx5dv_init_obj(&dv, MLX5DV_OBJ_AH);

    *av = *(dah.av);
}

class queue_pair : public base_object {
public:
    queue_pair();
    ~queue_pair();
    void destroy() override;
    STATUS initialize(qp_init_creation_params& params);

    struct ibv_qp* get() const;
    uint32_t get_qpn() const;
    STATUS create_ah(ibv_pd* pd, ibv_ah_attr* rattr);
    uint32_t get_sq_buf_offset() { return _sq_buf_offset;}


    STATUS reset_to_init(qp_init_connection_params& params);
    STATUS init_to_rtr(qp_init_connection_params& params);
    STATUS rtr_to_rts(qp_init_connection_params& params);

    STATUS post_send(struct mlx5_wqe_ctrl_seg* ctrl, unsigned wqe_size);
    
    STATUS post_rdma_write(void* laddr, uint32_t lkey, 
                           void* raddr, uint32_t rkey, 
                           uint32_t length, uint32_t flags = 0);
                         
    STATUS post_rdma_read(void* laddr, uint32_t lkey, 
                        void* raddr, uint32_t rkey, 
                        uint32_t length, uint32_t flags = 0);
                        
    STATUS post_send_msg(void* laddr, uint32_t lkey, 
                       uint32_t length, uint32_t flags = 0);
                       
    STATUS post_send_imm(void* laddr, uint32_t lkey, 
                       uint32_t length, uint32_t imm_data, 
                       uint32_t flags = 0);
                       
    STATUS post_rdma_write_imm(void* laddr, uint32_t lkey, 
                             void* raddr, uint32_t rkey, 
                             uint32_t length, uint32_t imm_data, 
                             uint32_t flags = 0);

    // RDMA Write wrapper for test.cpp compatibility
    STATUS post_write(void* laddr, uint32_t lkey, void* raddr, uint32_t rkey, uint32_t length, uint32_t flags = 0) {
        return post_rdma_write(laddr, lkey, raddr, rkey, length, flags);
    }

    STATUS post_recv();

    // Query the QP state using DEVX and return as int
    int get_qp_state() const;
    static const char* qp_state_to_str(int state);

    STATUS 
    query_qp_counters(uint32_t* hw_counter,
                      uint32_t* sw_counter,
                      uint32_t* wq_sig = nullptr);

    // Return the internal DEVX object for direct modifications
    mlx5dv_devx_obj* get_devx_obj() const {
        return _qp;
    }

private:

    mlx5dv_devx_obj* _qp;
    uint32_t _qpn;

    // UAR and memory regions for doorbell and work queues
    uar* _uar;
    user_memory* _umem_sq;
    user_memory* _umem_db;
    rdma_device* _rdevice;

    // Address handle for remote communication
    ibv_ah* _ah;

    STATUS post_wqe(uint8_t opcode, void* laddr, uint32_t lkey,
                    void* raddr, uint32_t rkey, uint32_t length,
                    uint32_t imm_data = 0, uint32_t flags = 0);
    
    uint16_t _sq_pi = 0;             // SQ producer index
    uint16_t _sq_ci = 0;             // SQ consumer index
    uint16_t _sq_size = 0;           // SQ size in WQEs
    uint32_t _sq_dbr_offset;   // Offset to SQ doorbell record
    uint32_t _sq_buf_offset;     // Offset in send queue buffer

    // BlueFlame buffer tracking for doorbell
    uint32_t _bf_offset = 0;         // Current BlueFlame doorbell offset
    uint32_t _bf_buf_size = 0;       // BlueFlame buffer size in bytes (initialized after qp setup)
};

//==============================================================================
// Memory Region
//==============================================================================

class memory_region : public base_object {
    public:
        memory_region();
        ~memory_region();
        void destroy() override;
    
        STATUS
        initialize(
            rdma_device* rdevice,
            queue_pair* qp,
            protection_domain* pd,
            size_t length
        );
    
        uint32_t get_lkey() const;
        uint32_t get_rkey() const;
        void*    get_addr() const;
        size_t   get_length() const;
        uint32_t get_mr_id() const;
        uint32_t get_mr_handle() const;
        uint32_t get_mr_pd() const;
        uint32_t get_mr_access() const;
        uint32_t get_mr_flags() const;
    
    private:
        STATUS
        create_user_memory(
            rdma_device* rdevice,
            size_t length
        ) {
            _umem = new user_memory();
            STATUS res = _umem->initialize(rdevice->get_context(), length);
            if (FAILED(res)) {
                log_error("Failed to create user memory");
                return res;
            }

            _addr = _umem->addr();
            _length = length;
            return STATUS_OK;
        }

        void destroy_user_memory()
        {
            if (_umem) {
                log_debug("Destroying user memory with umem_id: %d", _umem->umem_id());
                _umem->destroy();
                delete _umem;
                _umem = nullptr;
            }
        }

        mlx5dv_devx_obj* _cross_mr;
        user_memory*     _umem;
        queue_pair*      _qp;
        rdma_device*     _rdevice;
    
        uint32_t  _lkey;
        uint32_t  _rkey;
        void*     _addr;
        size_t    _length;
        uint32_t  _mr_id;
        uint32_t  _mr_handle;
        uint32_t  _mr_pd;
        uint32_t  _mr_access;
        uint32_t  _mr_flags;
};
