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

using namespace std;

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

private:
    struct ibv_device** _device_list;
    struct ibv_device* _device;
    struct ibv_context* _context;
    struct ibv_device_attr* _device_attr;
    map<uint8_t, struct ibv_port_attr*> _port_attr_map;
    map<uint8_t, struct mlx5dv_port*> _port_dv_attr_map;
    uint8_t _port_num;
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

class completion_queue : public base_object {
public:
    completion_queue();
    ~completion_queue();
    void destroy() override;
    STATUS initialize(cq_creation_params& params);
    struct ibv_cq* get() const;
    uint32_t get_cqn() const;
    STATUS poll_cq();
private:
    ibv_cq_ex* _pcq;
    uint32_t _cqn;
    mlx5dv_cq* _pdv_cq;
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
    uint8_t mtu;
    bool ece;
    uint8_t port_num;
    uint8_t retry_count;
    uint8_t rnr_retry;
    uint8_t min_rnr_to;
    uint8_t sl;
    uint8_t dscp;
    uint8_t traffic_class;
    ibv_ah_attr* remote_ah_attr;
    ibv_pd* pd;

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

    STATUS reset_to_init(qp_init_connection_params& params);
    STATUS init_to_rtr(qp_init_connection_params& params);
    STATUS rtr_to_rts(qp_init_connection_params& params);

    STATUS post_send(struct mlx5_wqe_ctrl_seg* ctrl, unsigned wqe_size);
    
    STATUS post_rdma_write(void* laddr, uint32_t lkey, 
                         uint64_t raddr, uint32_t rkey, 
                         uint32_t length, uint32_t flags = 0);
                         
    STATUS post_rdma_read(void* laddr, uint32_t lkey, 
                        uint64_t raddr, uint32_t rkey, 
                        uint32_t length, uint32_t flags = 0);
                        
    STATUS post_send_msg(void* laddr, uint32_t lkey, 
                       uint32_t length, uint32_t flags = 0);
                       
    STATUS post_send_imm(void* laddr, uint32_t lkey, 
                       uint32_t length, uint32_t imm_data, 
                       uint32_t flags = 0);
                       
    STATUS post_rdma_write_imm(void* laddr, uint32_t lkey, 
                             uint64_t raddr, uint32_t rkey, 
                             uint32_t length, uint32_t imm_data, 
                             uint32_t flags = 0);

    STATUS post_recv();
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
                  uint64_t raddr, uint32_t rkey, uint32_t length,
                  uint32_t imm_data = 0, uint32_t flags = 0);
    
    uint16_t _sq_pi = 0;        // SQ producer index
    uint16_t _sq_ci = 0;        // SQ consumer index
    uint16_t _sq_size = 0;      // SQ size in WQEs
    uint32_t _sq_dbr_offset = 0; // Offset to SQ doorbell record
    uint32_t _sq_buf_offset = 0; // Offset in send queue buffer
};

//==============================================================================

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
            void* addr,
            size_t length
        );
    
        uint32_t get_lkey() const;
        uint32_t get_rkey() const;
        void* get_addr() const;
        size_t get_length() const;
        uint32_t get_mr_id() const;
        uint32_t get_mr_handle() const;
        uint32_t get_mr_pd() const;
        uint32_t get_mr_access() const;
        uint32_t get_mr_flags() const;
    
    private:
        mlx5dv_devx_obj* _cross_mr;
        mlx5dv_devx_umem* _umem;
        queue_pair* _qp;
        rdma_device* _rdevice;
    
        uint32_t _lkey;
        uint32_t _rkey;
        void* _addr;
        size_t _length;
        uint32_t _mr_id;
        uint32_t _mr_handle;
        uint32_t _mr_pd;
        uint32_t _mr_access;
        uint32_t _mr_flags;
    };