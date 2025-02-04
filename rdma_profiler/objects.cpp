#include "objects.h"

using namespace std;

//============================================================================
// RDMA Device Implementation
//============================================================================
rdma_device::rdma_device()
    : _device_list(nullptr)
    , _device(nullptr)
    , _context(nullptr)
    , _device_attr(nullptr)
{}

rdma_device::~rdma_device() {
    destroy();
}

void rdma_device::destroy() {
    if (_device_attr) {
        free(_device_attr);
        _device_attr = nullptr;
    }
    if (_context) {
        ibv_close_device(_context);
        _context = nullptr;
    }
    if (_device_list) {
        ibv_free_device_list(_device_list);
        _device_list = nullptr;
    }
    _device = nullptr;
}

STATUS rdma_device::initialize(const std::string& device_name) {
    _device_list = ibv_get_device_list(nullptr);
    if (!_device_list) {
        return STATUS_ERR;
    }

    for (int i = 0; _device_list[i]; ++i) {
        if (device_name == ibv_get_device_name(_device_list[i])) {
            _device = _device_list[i];
            break;
        }
    }

    if (!_device) {
        destroy();
        return STATUS_ERR;
    }

    _context = ibv_open_device(_device);
    if (!_context) {
        destroy();
        return STATUS_ERR;
    }

    _device_attr = static_cast<ibv_device_attr*>(malloc(sizeof(ibv_device_attr)));
    if (!_device_attr) {
        destroy();
        return STATUS_ERR;
    }

    if (ibv_query_device(_context, _device_attr)) {
        destroy();
        return STATUS_ERR;
    }

    return STATUS_OK;
}

struct ibv_device* rdma_device::get() const {
    return _device;
}

struct ibv_context* rdma_device::get_context() const {
    return _context;
}

const ibv_device_attr* rdma_device::get_device_attr() const {
    return _device_attr;
}

void rdma_device::print_device_attr() const {
    if (!_device_attr) {
        fprintf(stderr, "Device attributes not available\n");
        return;
    }

    fprintf(stderr, "Device Attributes:\n");
    fprintf(stderr, "    fw_ver: %s\n",                     _device_attr->fw_ver);
    fprintf(stderr, "    node_guid: 0x%llx\n",             _device_attr->node_guid);
    fprintf(stderr, "    sys_image_guid: 0x%llx\n",        _device_attr->sys_image_guid);
    fprintf(stderr, "    max_mr_size: %lu\n",              _device_attr->max_mr_size);
    fprintf(stderr, "    page_size_cap: %lu\n",            _device_attr->page_size_cap);
    fprintf(stderr, "    vendor_id: %u\n",                 _device_attr->vendor_id);
    fprintf(stderr, "    vendor_part_id: %u\n",            _device_attr->vendor_part_id);
    fprintf(stderr, "    hw_ver: %u\n",                    _device_attr->hw_ver);
    fprintf(stderr, "    max_qp: %d\n",                    _device_attr->max_qp);
    fprintf(stderr, "    max_qp_wr: %d\n",                 _device_attr->max_qp_wr);
    fprintf(stderr, "    device_cap_flags: %u\n",          _device_attr->device_cap_flags);
    fprintf(stderr, "    max_sge: %d\n",                   _device_attr->max_sge);
    fprintf(stderr, "    max_sge_rd: %d\n",                _device_attr->max_sge_rd);
    fprintf(stderr, "    max_cq: %d\n",                    _device_attr->max_cq);
    fprintf(stderr, "    max_cqe: %d\n",                   _device_attr->max_cqe);
    fprintf(stderr, "    max_mr: %d\n",                    _device_attr->max_mr);
    fprintf(stderr, "    max_pd: %d\n",                    _device_attr->max_pd);
    fprintf(stderr, "    max_qp_rd_atom: %d\n",            _device_attr->max_qp_rd_atom);
    fprintf(stderr, "    max_ee_rd_atom: %d\n",            _device_attr->max_ee_rd_atom);
    fprintf(stderr, "    max_res_rd_atom: %d\n",           _device_attr->max_res_rd_atom);
    fprintf(stderr, "    max_qp_init_rd_atom: %d\n",       _device_attr->max_qp_init_rd_atom);
    fprintf(stderr, "    max_ee_init_rd_atom: %d\n",       _device_attr->max_ee_init_rd_atom);
    fprintf(stderr, "    atomic_cap: %d\n",                _device_attr->atomic_cap);
    fprintf(stderr, "    max_ee: %d\n",                    _device_attr->max_ee);
    fprintf(stderr, "    max_rdd: %d\n",                   _device_attr->max_rdd);
    fprintf(stderr, "    max_mw: %d\n",                    _device_attr->max_mw);
    fprintf(stderr, "    max_raw_ipv6_qp: %d\n",           _device_attr->max_raw_ipv6_qp);
    fprintf(stderr, "    max_raw_ethy_qp: %d\n",           _device_attr->max_raw_ethy_qp);
    fprintf(stderr, "    max_mcast_grp: %d\n",             _device_attr->max_mcast_grp);
    fprintf(stderr, "    max_mcast_qp_attach: %d\n",       _device_attr->max_mcast_qp_attach);
    fprintf(stderr, "    max_total_mcast_qp_attach: %d\n", _device_attr->max_total_mcast_qp_attach);
    fprintf(stderr, "    max_ah: %d\n",                    _device_attr->max_ah);
    fprintf(stderr, "    max_fmr: %d\n",                   _device_attr->max_fmr);
    fprintf(stderr, "    max_map_per_fmr: %d\n",           _device_attr->max_map_per_fmr);
    fprintf(stderr, "    max_srq: %d\n",                   _device_attr->max_srq);
    fprintf(stderr, "    max_srq_wr: %d\n",                _device_attr->max_srq_wr);
    fprintf(stderr, "    max_srq_sge: %d\n",               _device_attr->max_srq_sge);
    fprintf(stderr, "    max_pkeys: %hu\n",                _device_attr->max_pkeys);
    fprintf(stderr, "    local_ca_ack_delay: %d\n",        _device_attr->local_ca_ack_delay);
    fprintf(stderr, "    phys_port_cnt: %d\n",             _device_attr->phys_port_cnt);
}

//============================================================================
// Protection Domain Implementation
//============================================================================
protection_domain::protection_domain() :
    _pd(nullptr) {}

protection_domain::~protection_domain() {
    if (_pd && _initialized) {
        ibv_dealloc_pd(_pd);
    }
}

STATUS
protection_domain::initialize(struct ibv_context* context) {
    _pd = ibv_alloc_pd(context);
    if (!_pd) {
        _initialized = false;
        return STATUS_ERR;
    }

    mlx5dv_obj pd_obj{};
    mlx5dv_pd dvpd{};
    pd_obj.pd.in  = _pd;
    pd_obj.pd.out = &dvpd;
    if(mlx5dv_init_obj(&pd_obj, MLX5DV_OBJ_PD)) {
        _pd = nullptr;
        return STATUS_ERR;
    }

    _pdn = dvpd.pdn;

    _initialized = true;
    return STATUS_OK;
}

void
protection_domain::destroy() {
    if (_pd) {
        ibv_dealloc_pd(_pd);
        _pd = nullptr;
    }
}

struct ibv_pd*
protection_domain::get() const {
    return _pd;
}

uint32_t
protection_domain::get_pdn() const {
    return _pdn;
}

//============================================================================
// Completion Queue Implementation
//============================================================================
completion_queue::completion_queue() 
    : _cq(nullptr)
    , _channel(nullptr) {}

completion_queue::~completion_queue() {
    if (_cq && _initialized) {
        ibv_destroy_cq(ibv_cq_ex_to_cq(_cq));
    }
    destroy_completion_channel();
}

STATUS
completion_queue::initialize(struct ibv_context* context, int cqe) {
    STATUS res = create_completion_channel(context);
    RETURN_IF_FAILED(res);

    ibv_cq_init_attr_ex cq_attr_ex{};
    cq_attr_ex.cqe = cqe;
    cq_attr_ex.channel = _channel;
    cq_attr_ex.wc_flags = IBV_CREATE_CQ_SUP_WC_FLAGS;

    mlx5dv_cq_init_attr dv_cq_attr{};
    _cq = mlx5dv_create_cq(context, &cq_attr_ex, &dv_cq_attr);
    if (!_cq) {
        _initialized = false;
        return STATUS_ERR;
    }

    if (ibv_req_notify_cq(ibv_cq_ex_to_cq(_cq), 0)) {
        ibv_destroy_cq(ibv_cq_ex_to_cq(_cq));
        _cq = nullptr;
        return STATUS_ERR;
    }


    _dv_cq = static_cast<mlx5dv_cq*>(malloc(sizeof(mlx5dv_cq)));
    memset(_dv_cq, 0, sizeof(mlx5dv_cq));  // Fix: use sizeof the type, not the pointer

    struct mlx5dv_obj dv_obj = {};
    dv_obj.cq.in  = ibv_cq_ex_to_cq(_cq);
    dv_obj.cq.out = _dv_cq;
    if (mlx5dv_init_obj(&dv_obj, MLX5DV_OBJ_CQ)) {
        ibv_destroy_cq(ibv_cq_ex_to_cq(_cq));
        _cq = nullptr;
        return STATUS_ERR;
    }

    _initialized = true;
    return STATUS_OK;
}

void
completion_queue::destroy() {
    if (_cq) {
        ibv_destroy_cq(ibv_cq_ex_to_cq(_cq));
        _cq = nullptr;

        if(_dv_cq) {
            free(_dv_cq);
            _dv_cq = nullptr;
        }
    }
}

struct ibv_cq_ex*
completion_queue::get() const {
    return _cq;
}

uint32_t
completion_queue::get_cqn() const {
    return _dv_cq->cqn;
}

STATUS
completion_queue::wait_for_event_and_poll(ibv_wc* wc) {
    if (!_cq || !_channel) {
        return STATUS_ERR;
    }

    ibv_cq* ev_cq = nullptr;
    void* ev_ctx = nullptr;
    if (ibv_get_cq_event(_channel, &ev_cq, &ev_ctx)) {
        return STATUS_ERR;
    }

    ibv_ack_cq_events(ev_cq, 1);
    if (ibv_req_notify_cq(ev_cq, 0)) {
        return STATUS_ERR;
    }

    int ret = ibv_poll_cq(ev_cq, 1, wc);
    if (ret < 0) return STATUS_ERR;
    if (ret == 0) return STATUS_NO_DATA;
    return STATUS_OK;
}

STATUS
completion_queue::wait_for_event_and_poll_timestamps() {
    static constexpr int MAX_BATCH_SIZE = 128;
    
    ibv_cq* ev_cq = nullptr;
    void* ev_ctx = nullptr;

    if (!_cq || !_channel) {
        return STATUS_ERR;
    }

    if (ibv_get_cq_event(_channel, &ev_cq, &ev_ctx)) {
        return STATUS_ERR;
    }

    ibv_ack_cq_events(ev_cq, 1);
    if (ibv_req_notify_cq(ev_cq, 0)) {
        return STATUS_ERR;
    }

    ibv_cq_ex* cq_ex = _cq;
    ibv_poll_cq_attr poll_attr{};
    poll_attr.comp_mask = 0;

    int total_completions = 0;
    bool more_completions = true;

    while (more_completions) {
        int ret = ibv_start_poll(cq_ex, &poll_attr);
        if (ret == ENOENT) {
            // No more completions available
            return total_completions > 0 ? STATUS_OK : STATUS_NO_DATA;
        }
        if (ret != 0) {
            // Error occurred during start_poll, don't call end_poll
            return STATUS_ERR;
        }

        int batch_count = 0;
        do {
            ret = ibv_next_poll(cq_ex);
            if (ret == ENOENT) {
                // No more completions in this batch
                more_completions = false;
                break;
            }
            if (ret != 0) {
                // Error occurred during next_poll, still need to call end_poll
                ibv_end_poll(cq_ex);
                return total_completions > 0 ? STATUS_OK : STATUS_ERR;
            }

            uint64_t ts = ibv_wc_read_completion_wallclock_ns(cq_ex);
            batch_count++;
            total_completions++;

            if (_profiler) {
                uint32_t qp_num = ibv_wc_read_qp_num(cq_ex);
                auto timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>(std::chrono::nanoseconds(ts));
                _profiler->instance().record_cqe_timestamp(qp_num, cq_ex->wr_id, timestamp);
            }

        } while (batch_count < MAX_BATCH_SIZE);

        ibv_end_poll(cq_ex);
    }

    return total_completions > 0 ? STATUS_OK : STATUS_NO_DATA;
}

STATUS
completion_queue::create_completion_channel(struct ibv_context* context) {
    _channel = ibv_create_comp_channel(context);
    return _channel ? STATUS_OK : STATUS_ERR;
}

void
completion_queue::destroy_completion_channel() {
    if (_channel) {
        ibv_destroy_comp_channel(_channel);
        _channel = nullptr;
    }
}

STATUS
completion_queue::poll_cq(ibv_wc* wc) {
    if (!_cq) return STATUS_ERR;
    
    int ret = ibv_poll_cq(ibv_cq_ex_to_cq(_cq), 1, wc);
    if (ret < 0) return STATUS_ERR;
    if (ret == 0) return STATUS_NO_DATA;
    return STATUS_OK;
}

//============================================================================
// User Memory Implementation
//============================================================================
user_memory::user_memory() 
    : _umem(nullptr)
    , _addr(nullptr)
    , _size(0)
    , _umem_id(0)
{}

void user_memory::destroy() {
    if (_umem) {
        mlx5dv_devx_umem_dereg(_umem);
        _umem = nullptr;
    }
    if (_addr) {
        free(_addr);
        _addr = nullptr;
    }
    _size = 0;
    _umem_id = 0;
    _initialized = false;
}

user_memory::~user_memory() {
    destroy();
}

STATUS user_memory::initialize(ibv_context* context, size_t size) {
    if (_initialized) {
        return STATUS_OK;
    }

    int page_size = getpagesize();
    size_t alloc_size = ((size + page_size - 1) / page_size) * page_size;

    void* ptr = nullptr;
    if (posix_memalign(&ptr, page_size, alloc_size) != 0) {
        return STATUS_ERR;
    }

    auto* reg = mlx5dv_devx_umem_reg(context, ptr, alloc_size, IBV_ACCESS_LOCAL_WRITE);
    if (!reg) {
        free(ptr);
        return STATUS_ERR;
    }

    _addr = ptr;
    _size = alloc_size;
    _umem = reg;
    _umem_id = reg->umem_id;
    _initialized = true;
    
    return STATUS_OK;
}

mlx5dv_devx_umem*
user_memory::get() const {
    return _umem;
}

void*
user_memory::addr() const {
    return _addr;
}

size_t
user_memory::size() const {
    return _size;
}

uint32_t
user_memory::umem_id() const {
    return _umem_id;
}

//============================================================================
// UAR Implementation
//============================================================================
uar::uar() : 
    _uar(nullptr)
{}

uar::~uar() {
    destroy();
}

void
uar::destroy() {
    if (_uar) {
        mlx5dv_devx_free_uar(_uar);
        _uar = nullptr;
    }
}

STATUS
uar::initialize(ibv_context* ctx) {
    _uar = mlx5dv_devx_alloc_uar(ctx, MLX5DV_UAR_ALLOC_TYPE_BF);
    return _uar ? STATUS_OK : STATUS_ERR;
}

mlx5dv_devx_uar*
uar::get() const {
    return _uar;
}

//============================================================================
// Queue Pair Implementation
//============================================================================
queue_pair::~queue_pair() {
    destroy();
}

void
queue_pair::destroy() {
    if (_qp) {
        mlx5dv_devx_obj_destroy(_qp);
        _qp = nullptr;
    }
    _qpn = 0;
}

STATUS
queue_pair::initialize(
    ibv_context* ctx,
    uint32_t pdn,
    uint32_t cqn,
    uint32_t umem_id_sq,
    uint32_t umem_id_db,
    const qp_params& params
) {
    auto ilog2 = [](uint32_t x) {
        uint32_t r = 0;
        while ((1U << r) < x) { ++r; }
        return r;
    };

    uint32_t in[DEVX_ST_SZ_DW(create_qp_in)]   = {0};
    uint32_t out[DEVX_ST_SZ_DW(create_qp_out)] = {0};

    DEVX_SET(create_qp_in, in, opcode, MLX5_CMD_OP_CREATE_QP);

    void* qpc = DEVX_ADDR_OF(create_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, pd, pdn);
    DEVX_SET(qpc, qpc, st, 0x0);  // RC
    DEVX_SET(qpc, qpc, cqn_snd, cqn);
    DEVX_SET(qpc, qpc, cqn_rcv, cqn);
    DEVX_SET(qpc, qpc, log_sq_size, ilog2(params.sq_size));
    DEVX_SET(qpc, qpc, log_rq_size, ilog2(params.rq_size));
    DEVX_SET(qpc, qpc, no_sq, 0);
    DEVX_SET(qpc, qpc, uar_page, 0);
    DEVX_SET(qpc, qpc, log_page_size, get_page_size_log());
    DEVX_SET(qpc, qpc, dbr_umem_id, umem_id_db);

    DEVX_SET(create_qp_in, in, wq_umem_id, umem_id_sq);
    DEVX_SET(create_qp_in, in, wq_umem_valid, 1);

    _qp = mlx5dv_devx_obj_create(ctx, in, sizeof(in), out, sizeof(out));
    if (!_qp) {
        return STATUS_ERR;
    }

    _qpn = DEVX_GET(create_qp_out, out, qpn);
    return STATUS_OK;
}

STATUS
queue_pair::reset_to_init() {
    uint32_t in[DEVX_ST_SZ_DW(rst2init_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(rst2init_qp_out)] = {0};

    DEVX_SET(rst2init_qp_in, in, opcode, MLX5_CMD_OP_RST2INIT_QP);
    DEVX_SET(rst2init_qp_in, in, qpn, _qpn);

    void* qpc = DEVX_ADDR_OF(rst2init_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, primary_address_path.pkey_index, 0);
    DEVX_SET(qpc, qpc, primary_address_path.vhca_port_num, 1);
    DEVX_SET(qpc, qpc, mtu, 5);

    return mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out)) ? 
           STATUS_ERR : STATUS_OK;
}

STATUS
queue_pair::init_to_rtr(uint32_t remote_qpn, uint32_t remote_psn) {
    uint32_t in[DEVX_ST_SZ_DW(init2rtr_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(init2rtr_qp_out)] = {0};

    DEVX_SET(init2rtr_qp_in, in, opcode, MLX5_CMD_OP_INIT2RTR_QP);
    DEVX_SET(init2rtr_qp_in, in, qpn, _qpn);

    void* qpc = DEVX_ADDR_OF(init2rtr_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, primary_address_path.vhca_port_num, 1);
    DEVX_SET(qpc, qpc, mtu, 5);
    DEVX_SET(qpc, qpc, remote_qpn, remote_qpn);
    DEVX_SET(qpc, qpc, next_rcv_psn, remote_psn);

    return mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out)) ?
           STATUS_ERR : STATUS_OK;
}

STATUS
queue_pair::rtr_to_rts(uint32_t psn) {
    uint32_t in[DEVX_ST_SZ_DW(rtr2rts_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(rtr2rts_qp_out)] = {0};

    DEVX_SET(rtr2rts_qp_in, in, opcode, MLX5_CMD_OP_RTR2RTS_QP);
    DEVX_SET(rtr2rts_qp_in, in, qpn, _qpn);

    void* qpc = DEVX_ADDR_OF(rtr2rts_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, next_send_psn, psn);

    return mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out)) ?
           STATUS_ERR : STATUS_OK;
}

STATUS
queue_pair::post_op(
    wqe_op op,
    uint64_t remote_addr,
    uint32_t rkey,
    uintptr_t local_addr,
    uint32_t local_lkey,
    size_t length,
    bool signal
) {
    _profiler->instance().record_post_op(_qpn, _sq.get_idx());

    auto builder = _sq.get_wqe();
    uint32_t idx = _sq.get_idx();

    builder.build_ctrl(op, _qpn, idx, signal);
    
    if (op != wqe_op::SEND) {
        builder.build_raddr(remote_addr, rkey);
    }
    
    builder.build_data(local_addr, local_lkey, length);

    _sq.advance();

    if (_db_record) {
        *static_cast<volatile uint32_t*>(_db_record) = _sq.get_head();
    }

    if (_uar_map) {
        uint64_t db_val = pack_doorbell(_sq.get_head(), op, idx);
        *_uar_map = db_val;
    }

    _profiler->instance().record_doorbell(_qpn, idx);

    return STATUS_OK;
}

mlx5dv_devx_obj*
queue_pair::get() const {
    return _qp;
}

uint32_t 
queue_pair::get_qpn() const {
    return _qpn;
}






