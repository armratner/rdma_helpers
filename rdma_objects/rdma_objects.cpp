#include "rdma_objects.h"
#include <cstring>
#include <poll.h>

#include "../common/mmio.h"


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

    for (const auto& [index, port_attr] : _port_attr_map) {
        if (port_attr) free(port_attr);
    }
    _port_attr_map.clear();

    for (const auto& [index, port_dv_attr] : _port_dv_attr_map) {
        if (port_dv_attr) free(port_dv_attr);
    }
    _port_dv_attr_map.clear();

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

STATUS
rdma_device::query_hca_capabilities() {
    uint32_t hca_cap_in[DEVX_ST_SZ_DW(query_hca_cap_in)] = {0};
    uint32_t hca_cap_out[DEVX_ST_SZ_DW(query_hca_cap_out)] = {0};

    DEVX_SET(query_hca_cap_in, hca_cap_in, opcode, MLX5_CMD_OP_QUERY_HCA_CAP);
    DEVX_SET(query_hca_cap_in, hca_cap_in, op_mod, MLX5_SET_HCA_CAP_OP_MOD_GENERAL_DEVICE);

    int status = mlx5dv_devx_general_cmd(_context, hca_cap_in, sizeof(hca_cap_in), hca_cap_out, sizeof(hca_cap_out));
    if (status) {
        log_error("Failed to query HCA capabilities");
        return STATUS_ERR;
    }

    void* hca_cap = DEVX_ADDR_OF(query_hca_cap_out, hca_cap_out, capability);

    _hca_cap.log_max_srq_sz               = DEVX_GET(cmd_hca_cap, hca_cap, log_max_srq_sz);
    _hca_cap.log_max_qp_sz                = DEVX_GET(cmd_hca_cap, hca_cap, log_max_qp_sz);
    _hca_cap.log_max_qp                   = DEVX_GET(cmd_hca_cap, hca_cap, log_max_qp);
    _hca_cap.log_max_srq                  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_srq);
    _hca_cap.log_max_cq_sz                = DEVX_GET(cmd_hca_cap, hca_cap, log_max_cq_sz);
    _hca_cap.log_max_cq                   = DEVX_GET(cmd_hca_cap, hca_cap, log_max_cq);
    _hca_cap.log_max_eq_sz                = DEVX_GET(cmd_hca_cap, hca_cap, log_max_eq_sz);
    _hca_cap.log_max_mkey                 = DEVX_GET(cmd_hca_cap, hca_cap, log_max_mkey);
    _hca_cap.log_max_eq                   = DEVX_GET(cmd_hca_cap, hca_cap, log_max_eq);
    _hca_cap.log_max_klm_list_size        = DEVX_GET(cmd_hca_cap, hca_cap, log_max_klm_list_size);
    _hca_cap.log_max_ra_req_qp            = DEVX_GET(cmd_hca_cap, hca_cap, log_max_ra_req_qp);
    _hca_cap.log_max_ra_res_qp            = DEVX_GET(cmd_hca_cap, hca_cap, log_max_ra_res_qp);
    _hca_cap.log_max_msg                  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_msg);
    _hca_cap.max_tc                       = DEVX_GET(cmd_hca_cap, hca_cap, max_tc);
    _hca_cap.log_max_mcg                  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_mcg);
    _hca_cap.log_max_pd                   = DEVX_GET(cmd_hca_cap, hca_cap, log_max_pd);
    _hca_cap.log_max_xrcd                 = DEVX_GET(cmd_hca_cap, hca_cap, log_max_xrcd);
    _hca_cap.log_max_rq                   = DEVX_GET(cmd_hca_cap, hca_cap, log_max_rq);
    _hca_cap.log_max_sq                   = DEVX_GET(cmd_hca_cap, hca_cap, log_max_sq);
    _hca_cap.log_max_tir                  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_tir);
    _hca_cap.log_max_tis                  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_tis);
    _hca_cap.log_max_rmp                  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_rmp);
    _hca_cap.log_max_rqt                  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_rqt);
    _hca_cap.log_max_rqt_size             = DEVX_GET(cmd_hca_cap, hca_cap, log_max_rqt_size);
    _hca_cap.log_max_tis_per_sq           = DEVX_GET(cmd_hca_cap, hca_cap, log_max_tis_per_sq);
    _hca_cap.log_max_stride_sz_rq         = DEVX_GET(cmd_hca_cap, hca_cap, log_max_stride_sz_rq);
    _hca_cap.log_min_stride_sz_rq         = DEVX_GET(cmd_hca_cap, hca_cap, log_min_stride_sz_rq);
    _hca_cap.log_max_stride_sz_sq         = DEVX_GET(cmd_hca_cap, hca_cap, log_max_stride_sz_sq);
    _hca_cap.log_min_stride_sz_sq         = DEVX_GET(cmd_hca_cap, hca_cap, log_min_stride_sz_sq);
    _hca_cap.log_max_hairpin_queues       = DEVX_GET(cmd_hca_cap, hca_cap, log_max_hairpin_queues);
    _hca_cap.log_max_hairpin_wq_data_sz   = DEVX_GET(cmd_hca_cap, hca_cap, log_max_hairpin_wq_data_sz);
    _hca_cap.log_max_hairpin_num_packets  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_hairpin_num_packets);
    _hca_cap.log_max_wq_sz                = DEVX_GET(cmd_hca_cap, hca_cap, log_max_wq_sz);
    _hca_cap.log_max_vlan_list            = DEVX_GET(cmd_hca_cap, hca_cap, log_max_vlan_list);
    _hca_cap.log_max_current_mc_list      = DEVX_GET(cmd_hca_cap, hca_cap, log_max_current_mc_list);
    _hca_cap.log_max_current_uc_list      = DEVX_GET(cmd_hca_cap, hca_cap, log_max_current_uc_list);
    _hca_cap.log_max_transport_domain     = DEVX_GET(cmd_hca_cap, hca_cap, log_max_transport_domain);
    _hca_cap.log_max_flow_counter_bulk    = DEVX_GET(cmd_hca_cap, hca_cap, log_max_flow_counter_bulk);
    _hca_cap.log_max_flow_counter_bulk    = DEVX_GET(cmd_hca_cap, hca_cap, log_max_flow_counter_bulk);
    _hca_cap.log_max_l2_table             = DEVX_GET(cmd_hca_cap, hca_cap, log_max_l2_table);
    _hca_cap.log_uar_page_sz              = DEVX_GET(cmd_hca_cap, hca_cap, log_uar_page_sz);
    _hca_cap.log_max_pasid                = DEVX_GET(cmd_hca_cap, hca_cap, log_max_pasid);
    _hca_cap.log_max_dct_connections      = DEVX_GET(cmd_hca_cap, hca_cap, log_max_dct_connections);
    _hca_cap.log_max_atomic_size_qp       = DEVX_GET(cmd_hca_cap, hca_cap, log_max_atomic_size_qp);
    _hca_cap.log_max_atomic_size_dc       = DEVX_GET(cmd_hca_cap, hca_cap, log_max_atomic_size_dc);
    _hca_cap.log_max_xrq                  = DEVX_GET(cmd_hca_cap, hca_cap, log_max_xrq);
    _hca_cap.native_port_num              = DEVX_GET(cmd_hca_cap, hca_cap, native_port_num);
    _hca_cap.num_ports                    = DEVX_GET(cmd_hca_cap, hca_cap, num_ports);

    log_debug("HCA Capabilities successfully queried, log_max_qp_sz: %u", _hca_cap.log_max_qp_sz);
    log_debug("HCA log_max_cq_sz: %u, log_max_cq: %u", _hca_cap.log_max_cq_sz, _hca_cap.log_max_cq);

    return STATUS_OK;
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

    _device_attr = aligned_alloc<ibv_device_attr>(sizeof(ibv_device_attr));
    if (!_device_attr) {
        destroy();
        return STATUS_ERR;
    }

    if (ibv_query_device(_context, _device_attr)) {
        destroy();
        return STATUS_ERR;
    }

    if(query_port_attr()) {
        destroy();
        return STATUS_ERR;
    }

    print_device_attr();

    STATUS res = query_hca_capabilities();
    RETURN_IF_FAILED(res);

    return STATUS_OK;
}

STATUS
rdma_device::query_port_attr() {
    for (int i = 1; i <= _device_attr->phys_port_cnt; ++i) {
        struct ibv_port_attr* port_attr = aligned_alloc<ibv_port_attr>(sizeof(ibv_port_attr));
        struct mlx5dv_port* port_dv_attr = aligned_alloc<mlx5dv_port>(sizeof(mlx5dv_port));
        if (!port_attr || !port_dv_attr) {
            log_error("Failed to allocate memory for port attributes");
            return STATUS_ERR;
        }

        if (ibv_query_port(_context, i, port_attr)) {
            log_error("Failed to query port attributes for port %d", i);
            return STATUS_ERR;
        }

        if (mlx5dv_query_port(_context, i, port_dv_attr)) {
            log_error("Failed to query mlx5dv port attributes for port %d", i);
            return STATUS_ERR;
        }

        _port_dv_attr_map[i] = port_dv_attr;
        _port_attr_map[i] = port_attr;
    }

    return STATUS_OK;
}

uint8_t rdma_device::get_port_num() const {
    return _port_num;
}

static const char* ibv_mtu_str(uint8_t mtu) {
    switch (mtu) {
        case IBV_MTU_256:  return "IBV_MTU_256";
        case IBV_MTU_512:  return "IBV_MTU_512";
        case IBV_MTU_1024: return "IBV_MTU_1024";
        case IBV_MTU_2048: return "IBV_MTU_2048";
        case IBV_MTU_4096: return "IBV_MTU_4096";
        default:           return "UNKNOWN_MTU";
    }
}

static const char* ibv_active_speed_str(uint32_t speed) {
    switch (speed) {
        case 0:   return "0.0 Gbps";
        case 1:   return "2.5 Gbps";
        case 2:   return "5.0 Gbps";
        case 4:   return "5.0 Gbps";
        case 8:   return "10.0 Gbps";
        case 16:  return "14.0 Gbps";
        case 32:  return "25.0 Gbps";
        case 64:  return "50.0 Gbps";
        case 128: return "100.0 Gbps";
        case 256: return "200.0 Gbps";
        case 512: return "400.0 Gbps";
        case 1024: return "800.0 Gbps";
        default:  return "UNKNOWN_SPEED";
    }
}

static const char* ibv_phys_state_str(uint8_t state) {
    switch (state) {
        case 1: return "Sleep";
        case 2: return "Polling";
        case 3: return "Disabled";
        case 4: return "Port configuration training";
        case 5: return "Link up";
        case 6: return "Link error recovery";
        case 7: return "Phy test";
        default: return "UNKNOWN_PHYS_STATE";
    }
}

void
rdma_device::print_port_attr() const {
    for (const auto& [index, port_attr] : _port_attr_map) {
        log_debug("Port Attributes for Port %d:", index);
        log_debug("    state: %s", ibv_port_state_str(port_attr->state));
        log_debug("    max_mtu: %s", ibv_mtu_str(port_attr->max_mtu));
        log_debug("    active_mtu: %s", ibv_mtu_str(port_attr->active_mtu));
        log_debug("    active_speed: %s", ibv_active_speed_str(port_attr->active_speed_ex));
        log_debug("    phys_state: %s", ibv_phys_state_str(port_attr->phys_state));

        if (port_attr->link_layer == IBV_LINK_LAYER_ETHERNET) {
            log_debug("    link_layer: ETHERNET");
        } else if (port_attr->link_layer == IBV_LINK_LAYER_INFINIBAND) {
            log_debug("    link_layer: INFINIBAND");
            log_debug("    lid: %d", port_attr->lid);
            log_debug("    sm_lid: %d", port_attr->sm_lid);
            log_debug("    lmc: %d", port_attr->lmc);
        }
    }
}

void
rdma_device::print_port_dv_attr() const {
    
    for (const auto& [index, port_dv_attr] : _port_dv_attr_map) {
        log_debug("DV Port Attributes for Port %d:", index);
        log_debug("    flags: %u", port_dv_attr->flags);
        log_debug("    vport: %u", port_dv_attr->vport);
        log_debug("    vport_vhca_id: %u", port_dv_attr->vport_vhca_id);
        log_debug("    esw_owner_vhca_id: %u", port_dv_attr->esw_owner_vhca_id);
    }
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

const ibv_port_attr* rdma_device::get_port_attr(uint8_t port_num) const {
    if (port_num != 1) {
        log_error("Port number %d not supported", port_num);
        return nullptr;
    }

    auto it = _port_attr_map.find(port_num);
    if (it == _port_attr_map.end()) {
        log_error("Port attributes not found for port number %d", port_num);
        return nullptr;
    }
    return it->second;
}

void rdma_device::print_device_attr() const {
    if (!_device_attr) {
        log_error("Device attributes not available");
        return;
    }

    log_debug("Device Attributes:");
    log_debug("    fw_ver: %s", _device_attr->fw_ver);
    log_debug("    node_guid: 0x%llx", _device_attr->node_guid);
    log_debug("    sys_image_guid: 0x%llx", _device_attr->sys_image_guid);
    log_debug("    max_mr_size: %lu", _device_attr->max_mr_size);
    log_debug("    page_size_cap: %lu", _device_attr->page_size_cap);
    log_debug("    vendor_id: %u", _device_attr->vendor_id);
    log_debug("    vendor_part_id: %u", _device_attr->vendor_part_id);
    log_debug("    hw_ver: %u", _device_attr->hw_ver);
    log_debug("    max_qp: %d", _device_attr->max_qp);
    log_debug("    max_qp_wr: %d", _device_attr->max_qp_wr);
    log_debug("    device_cap_flags: %u", _device_attr->device_cap_flags);
    log_debug("    max_sge: %d", _device_attr->max_sge);
    log_debug("    max_sge_rd: %d", _device_attr->max_sge_rd);
    log_debug("    max_cq: %d", _device_attr->max_cq);
    log_debug("    max_cqe: %d", _device_attr->max_cqe);
    log_debug("    max_mr: %d", _device_attr->max_mr);
    log_debug("    max_pd: %d", _device_attr->max_pd);
    log_debug("    max_qp_rd_atom: %d", _device_attr->max_qp_rd_atom);
    log_debug("    max_ee_rd_atom: %d", _device_attr->max_ee_rd_atom);
    log_debug("    max_res_rd_atom: %d", _device_attr->max_res_rd_atom);
    log_debug("    max_qp_init_rd_atom: %d", _device_attr->max_qp_init_rd_atom);
    log_debug("    max_ee_init_rd_atom: %d", _device_attr->max_ee_init_rd_atom);
    log_debug("    atomic_cap: %d", _device_attr->atomic_cap);
    log_debug("    max_ee: %d", _device_attr->max_ee);
    log_debug("    max_rdd: %d", _device_attr->max_rdd);
    log_debug("    max_mw: %d", _device_attr->max_mw);
    log_debug("    max_raw_ipv6_qp: %d", _device_attr->max_raw_ipv6_qp);
    log_debug("    max_raw_ethy_qp: %d", _device_attr->max_raw_ethy_qp);
    log_debug("    max_mcast_grp: %d", _device_attr->max_mcast_grp);
    log_debug("    max_mcast_qp_attach: %d", _device_attr->max_mcast_qp_attach);
    log_debug("    max_total_mcast_qp_attach: %d", _device_attr->max_total_mcast_qp_attach);
    log_debug("    max_ah: %d", _device_attr->max_ah);
    log_debug("    max_fmr: %d", _device_attr->max_fmr);
    log_debug("    max_map_per_fmr: %d", _device_attr->max_map_per_fmr);
    log_debug("    max_srq: %d", _device_attr->max_srq);
    log_debug("    max_srq_wr: %d", _device_attr->max_srq_wr);
    log_debug("    max_srq_sge: %d", _device_attr->max_srq_sge);
    log_debug("    max_pkeys: %hu", _device_attr->max_pkeys);
    log_debug("    local_ca_ack_delay: %d", _device_attr->local_ca_ack_delay);
    log_debug("    phys_port_cnt: %d", _device_attr->phys_port_cnt);

    print_port_attr();
    print_port_dv_attr();
}

//============================================================================
// Protection Domain Implementation
//============================================================================
protection_domain::protection_domain() :
    _pd(nullptr) {}

protection_domain::~protection_domain() {
    if (_pd && _initialized) {
        destroy();
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

    log_debug("Protection Domain created with pdn: %d", _pdn);

    _initialized = true;
    return STATUS_OK;
}

void
protection_domain::destroy() {
    if (_pd) {
        log_debug("Destroying Protection Domain: %d", _pdn);
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
// User Memory Implementation
//============================================================================
user_memory::user_memory() 
    : _umem(nullptr)
    , _size(0)
    , _umem_id(0)
    , _umem_buf(nullptr)
{}

void user_memory::destroy() {
    if (_umem) {
        log_debug("Destroying user memory with umem_id: %d", _umem_id);
        mlx5dv_devx_umem_dereg(_umem);
        _umem = nullptr;
    }

    if (_umem_buf) {
        log_debug("Freeing user memory address: %p", _umem_buf);  // Fixed: was using _umem instead of _umem_buf
        free(_umem_buf);
        _umem_buf = nullptr;
    }

    _umem_id = 0;
    _size = 0;
    _initialized = false;
}

user_memory::~user_memory() {
    destroy();
}

STATUS
user_memory::initialize(ibv_context* context, size_t size) {
    if (_initialized) {
        return STATUS_OK;
    }

    // Fix: don't redeclare _umem_buf as a local variable, update the class member directly
    size_t allocated_size;
    _umem_buf = aligned_alloc<char>(size, &allocated_size);
    log_debug("Allocated user memory address: %p, size:%zu", _umem_buf, allocated_size);

    if (!_umem_buf || allocated_size == 0) {
        return STATUS_ERR;
    }

    uint32_t access = IBV_ACCESS_LOCAL_WRITE
                    | IBV_ACCESS_REMOTE_WRITE
                    | IBV_ACCESS_REMOTE_READ;         

    auto* reg = mlx5dv_devx_umem_reg(context, _umem_buf, allocated_size, access);
    if (!reg) {
        free(_umem_buf);
        _umem_buf = nullptr;
        return STATUS_ERR;
    }

    _size = allocated_size;
    _umem = reg;
    _umem_id = reg->umem_id;
    _initialized = true;

    log_debug("User memory initialized with umem_id: %d", _umem_id);
    return STATUS_OK;
}

mlx5dv_devx_umem*
user_memory::get() const {
    return _umem;
}

void*
user_memory::addr() const {
    return _umem_buf;
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
        log_debug("Destroying UAR %p", _uar);
        mlx5dv_devx_free_uar(_uar);
        _uar = nullptr;
    }
}

STATUS
uar::initialize(ibv_context* ctx) {
    
    uint32_t access = MLX5DV_UAR_ALLOC_TYPE_NC;

    log_debug("Using UAR access type: %u", access);
    
    _uar = mlx5dv_devx_alloc_uar(ctx, access);
    if (!_uar) {
        log_error("Failed to allocate UAR");
        return STATUS_ERR;
    }

    log_debug("Allocated UAR: %p, reg_addr:%p", _uar, _uar->reg_addr);
    
    return STATUS_OK;
}

mlx5dv_devx_uar*
uar::get() const {
    return _uar;
}

//============================================================================
// Memory Key Implementation
//============================================================================

memory_key::memory_key() :
    _mkey(0)
{}

memory_key::~memory_key() {
    destroy();
}

void memory_key::destroy() {
    if (_mkey) {
        log_debug("Destroying mkey lkey: %d, rkey: %d", _mkey->lkey, _mkey->rkey);
        mlx5dv_destroy_mkey(_mkey);
    }
    _mkey = nullptr;
}

STATUS
memory_key::initialize(ibv_pd* pd, uint32_t access, uint32_t num_entries) {
    mlx5dv_mkey_init_attr mkey_attr = {};
    mkey_attr.pd = pd;
    mkey_attr.create_flags = access;
    mkey_attr.max_entries = num_entries;

    _mkey = mlx5dv_create_mkey(&mkey_attr);
    if (!_mkey) {
        return STATUS_ERR;
    }

    log_debug("Created mkey lkey: %d, rkey: %d", _mkey->lkey, _mkey->rkey);

    return STATUS_OK;
}

mlx5dv_mkey*
memory_key::get_mkey() const {
    return _mkey;
}

uint32_t
memory_key::get_lkey() const {
    return _mkey->lkey;
}

uint32_t
memory_key::get_rkey() const {
    return _mkey->rkey;
}

//============================================================================
// Memory Region Implementation
//============================================================================
memory_region::memory_region() :
    _cross_mr(nullptr),
    _umem(nullptr),
    _qp(nullptr),
    _rdevice(nullptr),
    _lkey(0),
    _rkey(0),
    _addr(nullptr),
    _length(0),
    _mr_id(0),
    _mr_handle(0),
    _mr_pd(0),
    _mr_access(0),
    _mr_flags(0)
{}

memory_region::~memory_region() {
    destroy();
}

void
memory_region::destroy() {
    if (_cross_mr) {
        log_debug("Destroying memory region with lkey: %d, rkey: %d", _lkey, _rkey);
        mlx5dv_devx_obj_destroy(_cross_mr);
    }

    if (_umem) {
        destroy_user_memory();
    }
    _cross_mr = nullptr;
}

STATUS
memory_region::initialize(
    rdma_device* rdevice,
    queue_pair* qp,
    protection_domain* pd,
    size_t length
) {
    if (_cross_mr) {
        return STATUS_OK;
    }

    _rdevice = rdevice;
    _qp      = qp;
    _length  = length;

    STATUS res = create_user_memory(rdevice, length);
    RETURN_IF_FAILED(res);

    // Print all fields for debugging
    log_debug("Registering memory region with these parameters:");
    log_debug("  addr: %p", _addr);
    log_debug("  size: %zu", _length);


    uint32_t mkey_in[DEVX_ST_SZ_DW(create_mkey_in)] = {0};
    uint32_t mkey_out[DEVX_ST_SZ_DW(create_mkey_out)] = {0};

    DEVX_SET(create_mkey_in, mkey_in, opcode, MLX5_CMD_OP_CREATE_MKEY);
    DEVX_SET(create_mkey_in, mkey_in, mkey_umem_valid, 1);
    DEVX_SET(create_mkey_in, mkey_in, mkey_umem_id, _umem->get()->umem_id);
    DEVX_SET(create_mkey_in, mkey_in, mkey_umem_offset, 0);
    DEVX_SET(create_mkey_in, mkey_in, translations_octword_actual_size, 8);

    void *mkc = DEVX_ADDR_OF(create_mkey_in, mkey_in, memory_key_mkey_entry);
    DEVX_SET(mkc, mkc, access_mode_1_0, MLX5_MKC_ACCESS_MODE_MTT);
    DEVX_SET(mkc, mkc, a, 1);      // Atomic operations
    DEVX_SET(mkc, mkc, rw, 1);     // Remote write
    DEVX_SET(mkc, mkc, rr, 1);     // Remote read
    DEVX_SET(mkc, mkc, lw, 1);     // Local write
    DEVX_SET(mkc, mkc, lr, 1);     // Local read
    DEVX_SET(mkc, mkc, pd, pd->get_pdn());
    DEVX_SET(mkc, mkc, qpn, 0xFFFFFF);    
    DEVX_SET(mkc, mkc, mkey_7_0, 0xef);
    DEVX_SET64(mkc, mkc, start_addr, (intptr_t)_addr);
    DEVX_SET64(mkc, mkc, len, length);
    DEVX_SET(mkc, mkc, translations_octword_size, 8);
    DEVX_SET(mkc, mkc, log_page_size, get_page_size_log());

    // Create the MKEY object
    _cross_mr = mlx5dv_devx_obj_create(rdevice->get_context(),
                                       mkey_in, sizeof(mkey_in), 
                                       mkey_out, sizeof(mkey_out));
    if (!_cross_mr) {
        log_error("Failed to create memory region, error: %s, syndrome: 0x%x", 
                   strerror(errno), DEVX_GET(create_mkey_out, mkey_out, syndrome));
        return STATUS_ERR;
    }
    
    // Extract the created mkey
    uint32_t mkey_index = DEVX_GET(create_mkey_out, mkey_out, mkey_index);
    
    // For ConnectX-4/5/6, the low 8 bits should be 0xef for user-space keys
    _lkey = (mkey_index << 8) | 0xef;
    _rkey = _lkey;



    log_debug("Successfully created DEVX memory region");
    log_debug("MKey: 0x%x (index: 0x%x)", _lkey, mkey_index);

    /*
    struct ibv_mr* mr = ibv_reg_mr(pd->get(), _addr, _length,
                               IBV_ACCESS_LOCAL_WRITE |
                               IBV_ACCESS_REMOTE_WRITE |
                               IBV_ACCESS_REMOTE_READ);
    if (!mr) {
        log_error("Failed to register memory region using ibv_reg_mr: %s", strerror(errno));
        if (_umem) { destroy_user_memory(); _umem = nullptr; }
        return STATUS_ERR;
    }
    _lkey = mr->lkey;
    _rkey = mr->rkey;

    // Store the ibv_mr handle if needed for deregistration later
    // _ibv_mr_handle = mr; // Add a member to store this if needed for destroy()

    log_debug("Successfully registered memory region using ibv_reg_mr");
    log_debug("MR Keys: lkey=0x%x, rkey=0x%x", _lkey, _rkey);
    */

    return STATUS_OK;
}

uint32_t
memory_region::get_lkey() const {
    return _lkey;
}

uint32_t
memory_region::get_rkey() const {
    return _rkey;
}

void*
memory_region::get_addr() const {
    return _addr;
}

size_t
memory_region::get_length() const {
    return _length;
}

uint32_t
memory_region::get_mr_id() const {
    return _mr_id;
}

uint32_t
memory_region::get_mr_handle() const {
    return _mr_handle;
}

uint32_t
memory_region::get_mr_pd() const {
    return _mr_pd;
}

uint32_t
memory_region::get_mr_access() const {
    return _mr_access;
}

uint32_t
memory_region::get_mr_flags() const {
    return _mr_flags;
}

//============================================================================
// Completion DEVX Queue Implementation
//============================================================================

completion_queue_devx::completion_queue_devx() :
    _cq(nullptr),
    _cqn(0),
    _consumer_index(0)
{}

completion_queue_devx::~completion_queue_devx() {
    destroy();
}

void
completion_queue_devx::set_cq_hw_params(cq_hw_params& params) {
    // Store the provided parameters
    _cq_hw_params = params;
    
    // Log the parameters for debugging purposes
    log_debug("Setting CQ hardware parameters:");
    log_debug("  log_cq_size: %u", params.log_cq_size);
    log_debug("  log_page_size: %u", params.log_page_size);
    log_debug("  cqe_sz: %u", params.cqe_sz);
    log_debug("  cqe_comp_en: %s", params.cqe_comp_en ? "true" : "false");
    log_debug("  cq_period_mode: %u", params.cq_period_mode);
    log_debug("  cq_period: %u", params.cq_period);
    log_debug("  cq_max_count: %u", params.cq_max_count);
}

cq_hw_params
completion_queue_devx::get_cq_hw_params() const {
    return _cq_hw_params;
}

STATUS
completion_queue_devx::initialize_cq_resources(
    rdma_device* rdevice,
    cq_hw_params& cq_hw_params
) {
    _rdevice = rdevice;

    const hca_capabilities& caps = rdevice->get_hca_cap();
    uint8_t max_log_cq_size = caps.log_max_cq_sz;

    _uar->initialize(rdevice->get_context());
    if (_uar->get() == nullptr) {
        log_error("Failed to initialize UAR");
        return STATUS_ERR;
    }

    _umem_db->initialize(rdevice->get_context(), 1024);
    if (_umem_db->get() == nullptr) {
        log_error("Failed to initialize user memory for DB");
        return STATUS_ERR;
    }

    if (cq_hw_params.log_cq_size == 0 || 
        cq_hw_params.log_cq_size > max_log_cq_size) {
        cq_hw_params.log_cq_size = 9;
    }
    
    const size_t cqe_size = 64;
    uint32_t cq_entries = 1U << cq_hw_params.log_cq_size;

    log_debug("Allocating user memory for CQ: %u entries (%u bytes per entry),%zu bytes total (log_cq_size=%u)",
              cq_entries,
              cqe_size,
              cq_entries * cqe_size,
              cq_hw_params.log_cq_size);
    
    _umem->initialize(rdevice->get_context(), cq_entries * cqe_size);
    if (_umem->get() == nullptr) {
        log_error("Failed to initialize user memory for CQ");
        return STATUS_ERR;
    }

    void* cqe_buffer = _umem->get_umem_buf();
    if (!cqe_buffer) {
        log_error("Failed to get CQE buffer");
        return STATUS_ERR;
    }

    memset(cqe_buffer, 0, cq_entries * cqe_size);
    uint32_t cq_mask = cq_entries - 1;
    for (size_t i = 0; i < cq_entries; ++i) {
        struct mlx5_cqe64* cqe = (struct mlx5_cqe64*)((char*)cqe_buffer + i * cqe_size);
        uint8_t owner = ((i & (cq_mask + 1)) ? 1 : 0);
        cqe->op_own = (MLX5_CQE_INVALID << 4) | owner;
    }

    log_debug("CQE buffer initialized with op_own and correct owner bits");
    return STATUS_OK;
}

STATUS
completion_queue_devx::initialize(
    rdma_device* rdevice,
    cq_hw_params& cq_hw_params_list
) {
    STATUS status = initialize_cq_resources(rdevice, cq_hw_params_list);
    RETURN_IF_FAILED(status);

    uint32_t eqn = 0;
    if (mlx5dv_devx_query_eqn(rdevice->get_context(), 0, &eqn)) {
        log_error("Failed to query EQN");
        return STATUS_ERR;
    }

    // Initialize with zeros
    uint32_t in[DEVX_ST_SZ_DW(create_cq_in)]  = {0};
    uint32_t out[DEVX_ST_SZ_DW(create_cq_out)] = {0};

    DEVX_SET(create_cq_in, in, opcode, MLX5_CMD_OP_CREATE_CQ);
    void* cq_context = DEVX_ADDR_OF(create_cq_in, in, cqc);

    DEVX_SET(cqc, cq_context, c_eqn, eqn);
    DEVX_SET(cqc, cq_context, uar_page, _uar->get()->page_id);
    DEVX_SET(cqc, cq_context, log_cq_size, cq_hw_params_list.log_cq_size);

    DEVX_SET(cqc, cq_context, cqe_sz, 0);
    
    DEVX_SET(cqc, cq_context, dbr_umem_valid, 1);
    DEVX_SET(cqc, cq_context, dbr_umem_id, _umem_db->get()->umem_id);
    
    DEVX_SET(create_cq_in, in, cq_umem_valid, 1);
    DEVX_SET(create_cq_in, in, cq_umem_id, _umem->get()->umem_id);
    DEVX_SET(create_cq_in, in, cq_umem_offset, 0);
    
    log_debug("Creating CQ with parameters:");
    log_debug("  log_cq_size: %u", cq_hw_params_list.log_cq_size);
    log_debug("  cqe_sz: 0 (64 bytes)");
    log_debug("  eqn: %u", eqn);
    log_debug("  uar_page: %u", _uar->get()->page_id);
    log_debug("  umem_id: %u", _umem->get()->umem_id);
    log_debug("  dbr_umem_id: %u", _umem_db->get()->umem_id);

    _cq = mlx5dv_devx_obj_create(_rdevice->get_context(), in, sizeof(in), out, sizeof(out));
    if (!_cq) {
        log_error("Failed to create completion queue, error: %d (%s)", errno, strerror(errno));
        log_error("Syndrome: 0x%x", DEVX_GET(create_cq_out, out, syndrome));
        return STATUS_ERR;
    }

    _cqn = DEVX_GET(create_cq_out, out, cqn);
    log_debug("Created completion queue with cqn: %d", _cqn);

    return STATUS_OK;
}

STATUS
completion_queue_devx::poll_cq() {
    if (!_umem || !_umem_db) return STATUS_ERR;
    void* cqe_buf = _umem->addr();
    if (!cqe_buf) return STATUS_ERR;

    uint32_t cqe_cnt = 1U << _cq_hw_params.log_cq_size;
    uint32_t ci = _consumer_index % cqe_cnt;
    volatile struct mlx5_cqe64* cqe = 
            (volatile struct mlx5_cqe64*)((char*)cqe_buf + ci * sizeof(struct mlx5_cqe64));

    uint8_t owner = mlx5dv_get_cqe_owner((struct mlx5_cqe64*)cqe);
    uint8_t expected_owner = (_consumer_index / cqe_cnt) & 0x1;
    uint8_t opcode = mlx5dv_get_cqe_opcode((struct mlx5_cqe64*)cqe);
    uint8_t se = mlx5dv_get_cqe_se((struct mlx5_cqe64*)cqe);
    uint8_t format = mlx5dv_get_cqe_format((struct mlx5_cqe64*)cqe);

    log_debug("[DEVX CQ poll] ci=%u owner=%u expected_owner=%u opcode=0x%x se=%u format=%u wqe_counter=%u byte_cnt=%u",
              ci, owner, expected_owner, opcode, se, format, cqe->wqe_counter, cqe->byte_cnt);

    if (owner == expected_owner && (opcode != 0x0)) {
        const volatile struct mlx5_err_cqe* err_cqe = (const volatile struct mlx5_err_cqe*)cqe;
        log_error("CQE error: opcode=0x%x", opcode);
        log_error("  syndrome=0x%x", err_cqe->syndrome);
        log_error("  vendor_err_synd=0x%x", err_cqe->vendor_err_synd);
        log_error("  wqe_counter=0x%x", err_cqe->wqe_counter);
        log_error("  s_wqe_opcode_qpn=0x%x", err_cqe->s_wqe_opcode_qpn);
        log_error("  signature=0x%x", err_cqe->signature);
        log_error("  op_own=0x%x", err_cqe->op_own);
        log_error("  srqn=0x%x", err_cqe->srqn);
        _consumer_index++;
        volatile uint32_t* dbrec = (volatile uint32_t*)_umem_db->addr();
        *dbrec = htobe32(_consumer_index & 0xffffff);
        __sync_synchronize();
        return STATUS_ERR;
    }

    if (owner == expected_owner && opcode == 0x0) {
        log_debug("DEVX CQE received: opcode=%u, wqe_counter=%u, byte_cnt=%u, timestamp=%llu",
                  opcode, cqe->wqe_counter, cqe->byte_cnt, cqe->timestamp);
        _consumer_index++;
        volatile uint32_t* dbrec = (volatile uint32_t*)_umem_db->addr();
        *dbrec = htobe32(_consumer_index & 0xffffff);
        __sync_synchronize();
        return STATUS_OK;
    }

    return STATUS_ERR;
}

#define MLX5_CQ_ARM_DB 0x1

STATUS
completion_queue_devx::arm_cq(int solicited)
{
    if (!_umem_db || !_uar) return STATUS_ERR;
    volatile uint32_t* dbrec = (volatile uint32_t*)_umem_db->addr();
    if (!dbrec) return STATUS_ERR;
    void* uar_reg = _uar->get()->reg_addr;
    if (!uar_reg) return STATUS_ERR;    

    uint32_t sn = _arm_sn & 3;
    uint32_t ci = _consumer_index & 0xffffff;

    uint32_t cmd = solicited ? MLX5_CQ_DB_REQ_NOT_SOL : MLX5_CQ_DB_REQ_NOT;
    uint64_t doorbell = ((uint64_t)(sn << 28 | cmd | ci) << 32) | _cqn;

    log_debug("CQ Arming: sn=%u, ci=%u, cmd=%s, cqn=%u", 
              sn, ci, solicited ? "solicited" : "unsolicited", _cqn);

    dbrec[MLX5_CQ_ARM_DB] = htobe32(sn << 28 | cmd | ci);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    volatile uint64_t* uar_db = (volatile uint64_t*)((char*)uar_reg + MLX5_CQ_DOORBELL);
    *uar_db = htobe64(doorbell);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    
    return STATUS_OK;
}

void
completion_queue_devx::destroy() {
    if (_cq) {
        log_debug("Destroying completion queue with cqn: %d", _cqn);
        mlx5dv_devx_obj_destroy(_cq);
        _cq = nullptr;
    }

    if (_umem_db) {
        _umem_db->destroy();
    }

    if (_umem) {
        _umem->destroy();
    }

    if (_uar) {
        _uar->destroy();
    }
}

//============================================================================
// Queue Pair Implementation
//============================================================================

queue_pair::queue_pair() :
    _qp(nullptr),
    _qpn(0),
    _uar(nullptr),
    _umem_sq(nullptr),
    _umem_db(nullptr),
    _rdevice(nullptr),
    _ah(nullptr),
    _sq_size(0),
    _sq_pi(0),
    _sq_ci(0),
    _sq_dbr_offset(0),
    _sq_buf_offset(0),
    _bf_buf_size(0),
    _bf_offset(0),
    _use_bf(false)
{}

queue_pair::~queue_pair() {
    destroy();
}

void
queue_pair::destroy() {
    if (_qp) {
        log_debug("Destroying QP with qpn: %d", _qpn);
        mlx5dv_devx_obj_destroy(_qp);
        _qp = nullptr;
    }

    if (_ah) {
        log_debug("Destroying AH");
        ibv_destroy_ah(_ah);
        log_debug("Destroyed AH");
        _ah = nullptr;
    }

    _qpn = 0;
    _uar = nullptr;
    _umem_sq = nullptr;
    _umem_db = nullptr;
}

uint32_t
queue_pair::get_qpn() const {
    return _qpn;
}

#define MLX5_RQ_STRIDE          2
#define RDMA_WQE_SEG_SIZE       64   // Size of a WQE segment (one basic block)

STATUS
queue_pair::initialize(qp_init_creation_params& params) {
    if (_qp) {
        return STATUS_OK;
    }

    // uint8_t* sq_base = static_cast<uint8_t*>(params.umem_sq->addr());

    // for (size_t i = 0; i < 2048 / 64; ++i) {
    //      uint32_t* dst = reinterpret_cast<uint32_t*>(sq_base + (i * 64));

    //      dst[0] = 0xBEEF0000ULL | i;

    //      for (int q = 1; q < 8; ++q) {
    //          dst[q] = 0xDEADBEEFDEADBEEFULL;
    //     }
    // }

    if (params.rdevice) {
        _rdevice = params.rdevice;
    } else {
        log_error("Invalid device");
        return STATUS_ERR;
    }

    uint32_t in[DEVX_ST_SZ_DW(create_qp_in)]   = {0};
    uint32_t out[DEVX_ST_SZ_DW(create_qp_out)] = {0};

    DEVX_SET(create_qp_in, in, opcode, MLX5_CMD_OP_CREATE_QP);

    void* qpc = DEVX_ADDR_OF(create_qp_in, in, qpc);

    DEVX_SET(qpc, qpc, st, MLX5_QPC_ST_RC);
    DEVX_SET(qpc, qpc, pm_state, MLX5_QPC_PM_STATE_MIGRATED);

    DEVX_SET(qpc, qpc, pd, params.pdn);
    DEVX_SET(qpc, qpc, cqn_snd, params.cqn);
    DEVX_SET(qpc, qpc, cqn_rcv, params.cqn);

    DEVX_SET(qpc, qpc, log_sq_size , ilog2(params.sq_size));
    DEVX_SET(qpc, qpc, log_rq_size , ilog2(params.rq_size));
    DEVX_SET(qpc, qpc, log_rq_stride, MLX5_RQ_STRIDE);    

    DEVX_SET(qpc, qpc, no_sq, 0);
    DEVX_SET(qpc, qpc, wq_signature, 0);
    DEVX_SET(qpc, qpc, uar_page, params.uar_obj->get()->page_id);

    DEVX_SET(qpc, qpc, dbr_umem_id, params.umem_db->get()->umem_id);
    DEVX_SET(qpc, qpc, dbr_umem_valid, 1);
    DEVX_SET64(qpc, qpc, dbr_addr, 0);
    DEVX_SET(qpc, qpc, log_msg_max, _rdevice->get_hca_cap().log_max_msg);

    DEVX_SET(create_qp_in, in, wq_umem_id, params.umem_sq->get()->umem_id);
    DEVX_SET(create_qp_in, in, wq_umem_valid, 1);

    DEVX_SET(qpc, qpc, log_page_size, get_page_size_log());

    DEVX_SET(qpc, qpc, page_offset, 0);

    DEVX_SET(qpc, qpc, log_rra_max, params.max_rd_atomic);

    _qp = mlx5dv_devx_obj_create(params.context, in, sizeof(in), out, sizeof(out));
    if (!_qp) {
        log_error("Failed to initialize DEV OBJ QP");
        log_error("Errorno: %s", strerror(errno));
        log_error("Syndrome: 0x%x", DEVX_GET(create_qp_out, out, syndrome));
        return STATUS_ERR;
    }

    _qpn = DEVX_GET(create_qp_out, out, qpn);
    log_info("Created QP with qpn: %d", _qpn);
    _uar = params.uar_obj;
    _umem_sq = params.umem_sq;
    _umem_db = params.umem_db;

    _bf_buf_size = get_page_size();
    
    // Initialize send queue parameters
    _sq_size = params.sq_size;
    _sq_pi = 0;
    _sq_ci = 0;

    const size_t rq_stride_bytes = 16u << MLX5_RQ_STRIDE;
    size_t rq_bytes = params.rq_size * rq_stride_bytes;

    _sq_buf_offset = (rq_bytes + RDMA_WQE_SEG_SIZE - 1) & ~(RDMA_WQE_SEG_SIZE - 1); // Base offset in the send queue buffer
    log_debug("Send queue buffer offset: %u", _sq_buf_offset);
    
    log_debug("Queue Pair initialized with qpn: %d, sq_size: %u", _qpn, _sq_size);

    return STATUS_OK;
}

STATUS
queue_pair::reset_to_init(qp_init_connection_params& params) {
    if (!_qp) {
        return STATUS_ERR;
    }
    uint32_t in[DEVX_ST_SZ_DW(rst2init_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(rst2init_qp_out)] = {0};

    DEVX_SET(rst2init_qp_in, in, opcode, MLX5_CMD_OP_RST2INIT_QP);
    DEVX_SET(rst2init_qp_in, in, qpn, _qpn);

    void* qpc = DEVX_ADDR_OF(rst2init_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, rae, 1);
    DEVX_SET(qpc, qpc, rwe, 1);
    DEVX_SET(qpc, qpc, rre, 1);
    DEVX_SET(qpc, qpc, atomic_mode, 1);

    if (!(_rdevice->get_port_attr(1)->link_layer == IBV_LINK_LAYER_ETHERNET)) {
        DEVX_SET(qpc, qpc, primary_address_path.pkey_index, 0);
    }

    DEVX_SET(qpc, qpc, primary_address_path.vhca_port_num, params.port_num);
    DEVX_SET(qpc, qpc, pm_state, MLX5_QPC_PM_STATE_MIGRATED);

    if (mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out))) {
        log_error("Failed QP RST to INIT qpn: %d", _qpn);
        log_error("Syndrome: 0x%x", DEVX_GET(rst2init_qp_out, out, syndrome));
        return STATUS_ERR;
    }

    log_debug("Reset QP to INIT qpn: 0x%x", _qpn);
    
    return STATUS_OK;
}

STATUS
queue_pair::init_to_rtr(qp_init_connection_params &params)
{
    if (!_qp) {
        return STATUS_ERR;
    }

    STATUS res = create_ah(params.pd, params.remote_ah_attr);
    RETURN_IF_FAILED(res);

    auto _ah_attr = params.remote_ah_attr;

    mlx5_wqe_av mlx5_av = {};
    objects_get_av(_ah ,&mlx5_av);

    uint32_t in[DEVX_ST_SZ_DW(init2rtr_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(init2rtr_qp_out)] = {0};

    DEVX_SET(init2rtr_qp_in, in, opcode, MLX5_CMD_OP_INIT2RTR_QP);
    DEVX_SET(init2rtr_qp_in, in, qpn, _qpn);
    DEVX_SET(init2rtr_qp_in, in, ece, params.ece);

    void* qpc = DEVX_ADDR_OF(init2rtr_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, mtu, params.mtu);
    DEVX_SET(qpc, qpc, remote_qpn, params.remote_qpn);
    DEVX_SET(qpc, qpc, primary_address_path.vhca_port_num, _ah_attr->port_num);
    DEVX_SET(qpc, qpc, log_msg_max, _rdevice->get_hca_cap().log_max_msg);

    if (_rdevice->get_port_attr(1)->link_layer == IBV_LINK_LAYER_ETHERNET) {
        memcpy(DEVX_ADDR_OF(qpc, qpc, primary_address_path.rmac_47_32),
                mlx5_av.rmac, sizeof(mlx5_av.rmac));
        memcpy(DEVX_ADDR_OF(qpc, qpc, primary_address_path.rgid_rip),
                mlx5_av.rgid, sizeof(mlx5_av.rgid));

        DEVX_SET(qpc, qpc, primary_address_path.hop_limit, mlx5_av.hop_limit);
        DEVX_SET(qpc, qpc, primary_address_path.src_addr_index, _ah_attr->grh.sgid_index);
        DEVX_SET(qpc, qpc, primary_address_path.eth_prio, params.sl);
        DEVX_SET(qpc, qpc, primary_address_path.dscp, params.dscp);
    } else {
        DEVX_SET(qpc, qpc, primary_address_path.grh, _ah_attr->is_global);
        DEVX_SET(qpc, qpc, primary_address_path.rlid, _ah_attr->dlid);
        DEVX_SET(qpc, qpc, primary_address_path.mlid, _ah_attr->src_path_bits & 0x7f);
        DEVX_SET(qpc, qpc, primary_address_path.sl, params.sl);

        if (_ah_attr->is_global) {
            DEVX_SET(qpc, qpc, primary_address_path.src_addr_index, _ah_attr->grh.sgid_index);
            DEVX_SET(qpc, qpc, primary_address_path.hop_limit, _ah_attr->grh.hop_limit);
            memcpy(DEVX_ADDR_OF(qpc, qpc, primary_address_path.rgid_rip), &(_ah_attr->grh.dgid),
                        sizeof(_ah_attr->grh.dgid));
            DEVX_SET(qpc, qpc, primary_address_path.tclass, params.traffic_class);
        }
    }

    if (mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out))) {
        log_error("Failed to modify QP to RTR qpn: %d", _qpn);
        log_error("Syndrome: 0x%x", DEVX_GET(init2rtr_qp_out, out, syndrome));
        return STATUS_ERR;
    }

    log_debug("Modified QP to RTR qpn: %d", _qpn);

    return STATUS_OK;
}

STATUS
queue_pair::create_ah(ibv_pd* pd, ibv_ah_attr* rattr)
{
    _ah = ibv_create_ah(pd, rattr);
    if (!_ah) {
        log_error("Failed to create address handle, error: %d", errno);
        return STATUS_ERR;
    }

    log_debug("Created address handle with ah: %p", _ah);

    return STATUS_OK;
}

STATUS
queue_pair::rtr_to_rts(qp_init_connection_params &params)
{
    if (!_qp) {
        return STATUS_ERR;
    }

    uint32_t in[DEVX_ST_SZ_DW(rtr2rts_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(rtr2rts_qp_out)] = {0};

    DEVX_SET(rtr2rts_qp_in, in, opcode, MLX5_CMD_OP_RTR2RTS_QP);
    DEVX_SET(rtr2rts_qp_in, in, qpn, _qpn);

    void* qpc = DEVX_ADDR_OF(rtr2rts_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, log_ack_req_freq, 0);
    DEVX_SET(qpc, qpc, retry_count, params.retry_count);
    DEVX_SET(qpc, qpc, rnr_retry, params.rnr_retry);
    DEVX_SET(qpc, qpc, next_send_psn, 0);

    if (mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out))) {
        log_error("Failed to modify QP to RTS qpn: %d", _qpn);
        return STATUS_ERR;
    }

    log_debug("Modified QP to RTS qpn: %d", _qpn);
    return STATUS_OK;
}

#define RDMA_MAX_WQE_BB         4    // Maximum number of basic blocks per WQE
#define MLX5_SEND_WQE_BB        64   // Basic block size for send WQEs
#define MLX5_OPCODE_RDMA_WRITE  8    // RDMA Write opcode
#define MLX5_OPCODE_RDMA_READ   12   // RDMA Read opcode
#define MLX5_OPCODE_SEND        0    // Send opcode
#define MLX5_OPCODE_SEND_IMM    1    // Send with immediate opcode
#define MLX5_OPCODE_RDMA_WRITE_IMM 9 // RDMA Write with immediate opcode

// Implementation of the post_send method in queue_pair class
STATUS queue_pair::post_send(struct mlx5_wqe_ctrl_seg* ctrl, unsigned wqe_size) {
    if ((uintptr_t)ctrl % RDMA_WQE_SEG_SIZE != 0) {
        log_error("WQE control segment not aligned to %d bytes", RDMA_WQE_SEG_SIZE);
        return STATUS_ERR;
    }

    log_debug("Posting WQE at index %u, size %zu bytes", _sq_pi, wqe_size);

    uint16_t num_bb = (wqe_size + MLX5_SEND_WQE_BB - 1) / MLX5_SEND_WQE_BB;
    uint16_t new_pi = _sq_pi + num_bb;

    void *bf_reg = static_cast<char*>(_uar->get()->reg_addr) + _bf_offset;

    unsigned bytecnt   = wqe_size; 
    void *queue_start  = _umem_sq->addr();
    void *queue_end    = static_cast<char*>(queue_start) + _sq_size * RDMA_WQE_SEG_SIZE;

    udma_to_device_barrier();
    volatile uint32_t *dbrec = static_cast<volatile uint32_t*>(_umem_db->addr());
    dbrec[MLX5_SND_DBR] = htobe32(new_pi & 0xffff); 
    mmio_flush_writes();

    if (unlikely(_use_bf)) bf_copy(bf_reg, ctrl, bytecnt, queue_start, queue_end);
    mmio_write64_be(bf_reg, ctrl);
   
    _bf_offset ^= _bf_buf_size;
    _sq_pi = new_pi;
    log_debug("Updated SQ producer index to: %u", _sq_pi);
    return STATUS_OK;
}


STATUS queue_pair::post_wqe(uint8_t opcode, void* laddr, uint32_t lkey,
                            void* raddr, uint32_t rkey, uint32_t length,
                            uint32_t imm_data, uint32_t flags) {
    // Calculate WQE size based on segments needed
    size_t wqe_size = sizeof(mlx5_wqe_ctrl_seg);
    wqe_size += sizeof(mlx5_wqe_data_seg);
    bool need_raddr = (opcode == MLX5_OPCODE_RDMA_WRITE ||
                       opcode == MLX5_OPCODE_RDMA_WRITE_IMM ||
                       opcode == MLX5_OPCODE_RDMA_READ);
    if (need_raddr) {
        wqe_size += sizeof(mlx5_wqe_raddr_seg);
    }
    wqe_size = (wqe_size + RDMA_WQE_SEG_SIZE - 1) & ~(RDMA_WQE_SEG_SIZE - 1);

    mlx5_wqe_ctrl_seg* ctrl = (mlx5_wqe_ctrl_seg*)((char*)_umem_sq->addr() +
                              _sq_buf_offset + (_sq_pi % _sq_size) * RDMA_WQE_SEG_SIZE);
    log_debug("Posting WQE at index %u, size %zu bytes", _sq_pi, wqe_size);
    log_debug("WQE control segment at %p", ctrl);

    memset(ctrl, 0, wqe_size);

    uint8_t num_data_seg = 1;
    uint8_t ds = (need_raddr ? 2 : 1) + num_data_seg;
    uint8_t fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE;
    uint8_t signature = 0;
    uint8_t opmod = 0;
    uint32_t imm = 0;

    log_debug("Post WQE with parameters:");
    log_debug("  opcode: 0x%x", opcode);
    log_debug("  laddr: %p", laddr);
    log_debug("  lkey: 0x%x", lkey);
    log_debug("  raddr: 0x%lx", raddr);
    log_debug("  rkey: 0x%x", rkey);
    log_debug("  length: %u", length);
    log_debug("  flags: 0x%x", flags);
    log_debug("  need_raddr: %s", need_raddr ? "true" : "false");
    log_debug("  num_data_seg: %u", num_data_seg);


    log_debug("Calling mlx5dv_set_ctrl_seg with:");
    log_debug("  ctrl_addr: %p", ctrl);
    log_debug("  wqe_index: %u", _sq_pi); // Assuming _sq_pi is the correct index here
    log_debug("  opcode: 0x%x", opcode);
    log_debug("  opmod: 0x%x", opmod);
    log_debug("  qpn: 0x%x", _qpn);
    log_debug("  fm_ce_se: 0x%x", fm_ce_se); // <<< Check this value! Should be 2
    log_debug("  ds: %u", ds);
    log_debug("  signature: 0x%x", signature);
    log_debug("  imm: 0x%x", imm_data);

    if (opcode == MLX5_OPCODE_SEND_IMM || opcode == MLX5_OPCODE_RDMA_WRITE_IMM) {
        imm = htobe32(imm_data);
    }
    mlx5_set_ctrl_seg(ctrl, _sq_pi, opcode, opmod, _qpn, fm_ce_se, ds, signature, imm);

    void* segment = ctrl + 1;
    if (need_raddr) {
        mlx5_wqe_raddr_seg* raddr_seg = (mlx5_wqe_raddr_seg*)segment;
        mlx5_set_rdma_seg(raddr_seg, raddr, (uintptr_t)rkey);
        segment = (void*)((char*)segment + sizeof(struct mlx5_wqe_raddr_seg));
    }
    mlx5_wqe_data_seg* data_seg = (mlx5_wqe_data_seg*)segment;
    mlx5_set_data_seg(data_seg, length, lkey, (uintptr_t)laddr);
    dump_wqe((unsigned char*)ctrl);

    return post_send(ctrl, wqe_size);
}

// Implementations of the convenience methods
STATUS
queue_pair::post_rdma_write(void* laddr, uint32_t lkey, 
                            void* raddr, uint32_t rkey, 
                            uint32_t length, uint32_t flags) {
    return post_wqe(MLX5_OPCODE_RDMA_WRITE, laddr, lkey, raddr, rkey, length, 0, flags);
}

STATUS
queue_pair::post_rdma_read(void* laddr, uint32_t lkey, 
                           void* raddr, uint32_t rkey, 
                           uint32_t length, uint32_t flags) {
    return post_wqe(MLX5_OPCODE_RDMA_READ, laddr, lkey, raddr, rkey, length, 0, flags);
}

STATUS
queue_pair::post_send_msg(void* laddr, uint32_t lkey, 
                          uint32_t length, uint32_t flags) {
    return post_wqe(MLX5_OPCODE_SEND, laddr, lkey, 0, 0, length, 0, flags);
}

STATUS
queue_pair::post_send_imm(void* laddr, uint32_t lkey, 
                          uint32_t length, uint32_t imm_data, 
                          uint32_t flags) {
    return post_wqe(MLX5_OPCODE_SEND_IMM, laddr, lkey, 0, 0, length, imm_data, flags);
}

STATUS
queue_pair::post_rdma_write_imm(void* laddr, uint32_t lkey, 
                                void* raddr, uint32_t rkey, 
                                uint32_t length, uint32_t imm_data, 
                                uint32_t flags) {
    return post_wqe(MLX5_OPCODE_RDMA_WRITE_IMM, laddr, lkey, raddr, rkey, length, imm_data, flags);
}

STATUS 
queue_pair::query_qp_counters(uint32_t* hw_counter,
                              uint32_t* sw_counter,
                              uint32_t* wq_sig) {
    if (!_qp) {
        log_error("QP object not initialized");
        return STATUS_ERR;
    }

    uint32_t out[DEVX_ST_SZ_DW(query_qp_out)] = {};
    uint32_t in[DEVX_ST_SZ_DW(query_qp_in)] = {};
    
    DEVX_SET(query_qp_in, in, opcode, MLX5_CMD_OP_QUERY_QP);
    DEVX_SET(query_qp_in, in, qpn, _qpn);

    int ret = mlx5dv_devx_obj_query(_qp, in, sizeof(in), out, sizeof(out));
    if (ret) {
        log_error("Failed to query QP counters: %s", strerror(errno));
        return STATUS_ERR;
    }
    
    if (hw_counter) {
        *hw_counter = DEVX_GET(query_qp_out, out, qpc.hw_sq_wqebb_counter);
    }
    
    if (sw_counter) {
        *sw_counter = DEVX_GET(query_qp_out, out, qpc.sw_sq_wqebb_counter);
    }

    if (wq_sig) {
        *wq_sig = DEVX_GET(query_qp_out, out, qpc.wq_signature);
    }

    log_debug("QP MAX MSG: %d", DEVX_GET(query_qp_out, out, qpc.log_msg_max));
    log_debug("QP access rae: %d", DEVX_GET(query_qp_out, out, qpc.rae));
    
    return STATUS_OK;
}

// Implement the empty post_recv method from the header
STATUS queue_pair::post_recv() {
    // Implementation for posting receive WQEs
    // This would follow a similar pattern but use the receive queue
    log_error("post_recv not yet implemented");
    return STATUS_NOT_IMPLEMENTED;
}

// Implementation for queue_pair::get()
struct ibv_qp* queue_pair::get() const {
    // If you have a direct ibv_qp* member, return it. If not, return nullptr or the correct pointer.
    // If using DEVX, you may need to cast or extract the ibv_qp* from your internal structure.
    // For now, return nullptr if not available.
    return nullptr; // TODO: return actual ibv_qp* if available
}

int queue_pair::get_qp_state() const {
    if (!_qp || !_rdevice) return STATUS_ERR;

    uint32_t in[DEVX_ST_SZ_DW(query_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(query_qp_out)] = {0};

    DEVX_SET(query_qp_in, in, opcode, MLX5_CMD_OP_QUERY_QP);
    DEVX_SET(query_qp_in, in, qpn, _qpn);

    if (mlx5dv_devx_obj_query(_qp, in, sizeof(in), out, sizeof(out))) {
        log_error("Failed to query QP state for qpn: %d", _qpn);
        return -1;
    }

    void* qpc = DEVX_ADDR_OF(query_qp_out, out, qpc);
    int state = DEVX_GET(qpc, qpc, state);
    return state;
}

const char* queue_pair::qp_state_to_str(int state) {
    switch (state) {
        case 0: return "RESET";
        case 1: return "INIT";
        case 2: return "RTR";
        case 3: return "RTS";
        case 4: return "SQD";
        case 5: return "SQE";
        case 6: return "ERR";
        default: return "UNKNOWN";
    }
}

