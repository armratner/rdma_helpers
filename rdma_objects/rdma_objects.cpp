#include "rdma_objects.h"
#include <cstring>
#include <poll.h>

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
    _umem_buf = aligned_alloc<char>(size);
    log_debug("Allocated user memory address: %p, size:%zu", _umem_buf, size);

    if (!_umem_buf || size == 0) {
        return STATUS_ERR;
    }

    uint32_t access = IBV_ACCESS_LOCAL_WRITE
                    | IBV_ACCESS_REMOTE_WRITE
                    | IBV_ACCESS_REMOTE_READ
                    | IBV_ACCESS_RELAXED_ORDERING;         

    auto* reg = mlx5dv_devx_umem_reg(context, _umem_buf, size, access);
    if (!reg) {
        free(_umem_buf);
        _umem_buf = nullptr;
        return STATUS_ERR;
    }

    _size = size;
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
    
    uint32_t access = MLX5DV_UAR_ALLOC_TYPE_BF
                    | MLX5DV_UAR_ALLOC_TYPE_NC
                    | MLX5DV_UAR_ALLOC_TYPE_NC_DEDICATED;   

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

    _cross_mr = nullptr;
}

STATUS
memory_region::initialize(
    rdma_device* rdevice,
    queue_pair* qp,
    protection_domain* pd,
    void* addr,
    size_t length
) {

    mlx5dv_devx_umem_in umem_in;
    void *aligned_address;

    if (_cross_mr) {
        return STATUS_OK;
    }

    // Print all fields for debugging
    log_debug("Registering user memory with these parameters:");
    log_debug("  addr: %p", addr);
    log_debug("  size: %zu", length);
    log_debug("  access: 0x%x", IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);

    // Store the original values
    _addr = addr;
    _length = length;

    // Use system page size for alignment
    size_t page_size = get_page_size();
    uintptr_t addr_val = (uintptr_t)addr;
    uintptr_t aligned_addr_val = addr_val & ~(page_size - 1);
    aligned_address = (void*)aligned_addr_val;

    log_debug("  page_size: %zu", page_size);
    log_debug("  aligned_address: %p", aligned_address);

    // Set appropriate page size bitmap (typical system page size is 4KB = 0x1000)
    umem_in.addr = addr;
    umem_in.size = length;
    umem_in.access = IBV_ACCESS_LOCAL_WRITE |
                     IBV_ACCESS_REMOTE_READ |
                     IBV_ACCESS_REMOTE_WRITE;
    umem_in.pgsz_bitmap = 1ULL << (ffs(page_size) - 1);  // Set bit corresponding to page size
    umem_in.comp_mask = 0;

    log_debug("  pgsz_bitmap: 0x%lx", umem_in.pgsz_bitmap);
    log_debug("  comp_mask: 0x%x", umem_in.comp_mask);

    _umem = mlx5dv_devx_umem_reg_ex(rdevice->get_context(), &umem_in);
    if (_umem == nullptr) {
        log_error("Failed to register user memory, error: %s (errno=%d)", strerror(errno), errno);
        return STATUS_ERR;
    }

    uint32_t mkey_in[DEVX_ST_SZ_DW(create_mkey_in)] = {0};
    uint32_t mkey_out[DEVX_ST_SZ_DW(create_mkey_out)] = {0};

    DEVX_SET(create_mkey_in, mkey_in, opcode, MLX5_CMD_OP_CREATE_MKEY);

    // Set opcode and sizes
    DEVX_SET(create_mkey_in, mkey_in, translations_octword_actual_size, 1);
    
    // The key fields from UCX implementation: set the UMEM ID directly
    DEVX_SET(create_mkey_in, mkey_in, mkey_umem_id, _umem->umem_id);
    DEVX_SET64(create_mkey_in, mkey_in, mkey_umem_offset, 0);
    
    // Configure the Memory Key Context
    void *mkc = DEVX_ADDR_OF(create_mkey_in, mkey_in, memory_key_mkey_entry);
    
    DEVX_SET(mkc, mkc, access_mode_1_0, MLX5_MKC_ACCESS_MODE_MTT);
    DEVX_SET(mkc, mkc, a, 1);      // Atomic operations
    DEVX_SET(mkc, mkc, rw, 1);     // Remote write
    DEVX_SET(mkc, mkc, rr, 1);     // Remote read
    DEVX_SET(mkc, mkc, lw, 1);     // Local write
    DEVX_SET(mkc, mkc, lr, 1);     // Local read
    DEVX_SET(mkc, mkc, pd, pd->get_pdn());
    DEVX_SET(mkc, mkc, qpn, qp->get_qpn());
    DEVX_SET(mkc, mkc, mkey_7_0, 0);  // Will be assigned by hardware
    
    // Set memory range
    DEVX_SET64(mkc, mkc, start_addr, (uint64_t)addr);
    DEVX_SET64(mkc, mkc, len, length);
    
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
    _lkey = (DEVX_GET(create_mkey_out, mkey_out, mkey_index) << 8);
    _rkey = _lkey;

    log_debug("Successfully created DEVX memory region");
    log_debug("MKey: 0x%x", _lkey);

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
// Completion Queue Implementation
//============================================================================

completion_queue::completion_queue() :
    _pcq(nullptr)
{}

completion_queue::~completion_queue() {
    destroy();
}

void
completion_queue::destroy() {
    if (_pcq) {
        log_debug("Destroying completion queue with cqn: %d", _cqn);
        ibv_destroy_cq(ibv_cq_ex_to_cq(_pcq));
        _pcq = nullptr;
    }

    if (_pdv_cq) {
        free(_pdv_cq);
        _pdv_cq = nullptr;
    }
}

STATUS
completion_queue::initialize(cq_creation_params& params) {
    if (_pcq) {
        return STATUS_OK;
    }
    _consumer_index = 0;
    // Create completion channel
    _comp_channel = ibv_create_comp_channel(params.context);
    if (!_comp_channel) {
        log_error("Failed to create completion channel");
        return STATUS_ERR;
    }
    // Assign channel to CQ attributes
    params.cq_attr_ex->channel = _comp_channel;
    _pcq = mlx5dv_create_cq(params.context, params.cq_attr_ex , params.dv_cq_attr);
    if (!_pcq) {
        log_error("Failed to create completion queue, error: %s", strerror(errno));
        ibv_destroy_comp_channel(_comp_channel);
        _comp_channel = nullptr;
        return STATUS_ERR;
    }
    _pdv_cq = aligned_alloc<mlx5dv_cq>(sizeof(mlx5dv_cq));
    if (!_pdv_cq) {
        log_error("Failed to allocate memory for mlx5dv_cq");
        ibv_destroy_cq(ibv_cq_ex_to_cq(_pcq));
        ibv_destroy_comp_channel(_comp_channel);
        _comp_channel = nullptr;
        return STATUS_ERR;
    }
    struct mlx5dv_obj dv_obj = {};
    dv_obj.cq.in  = ibv_cq_ex_to_cq(_pcq);
    dv_obj.cq.out = _pdv_cq;
    if (mlx5dv_init_obj(&dv_obj, MLX5DV_OBJ_CQ)) {
        log_error("Failed to initialize mlx5dv_cq");
        ibv_destroy_cq(ibv_cq_ex_to_cq(_pcq));
        ibv_destroy_comp_channel(_comp_channel);
        _comp_channel = nullptr;
        return STATUS_ERR;
    }
    _cqn = _pdv_cq->cqn;
    log_debug("Created completion queue with cqn: %d", _cqn);

    return STATUS_OK;
}

STATUS completion_queue::poll_cq() {
    if (!_pdv_cq) return STATUS_ERR;
    volatile struct mlx5_cqe64* cqe_buf = (volatile struct mlx5_cqe64*)_pdv_cq->buf;
    int cqe_cnt = _pdv_cq->cqe_cnt;
    int ci = _consumer_index % cqe_cnt;
    volatile struct mlx5_cqe64* cqe = &cqe_buf[ci];
    uint8_t owner = cqe->op_own & 0x1;
    uint8_t expected_owner = (_consumer_index / cqe_cnt) & 0x1;
    uint8_t opcode = cqe->op_own >> 4;
    log_debug("[CQ poll] ci=%d owner=%u expected_owner=%u opcode=0x%x wqe_counter=%u byte_cnt=%u", ci, owner, expected_owner, opcode, cqe->wqe_counter, cqe->byte_cnt);
    if (owner == expected_owner && opcode != 0xf) {
        log_info("DEVX CQE received: opcode=%u, wqe_counter=%u, byte_cnt=%u", opcode, cqe->wqe_counter, cqe->byte_cnt);
        _consumer_index++;
        *(_pdv_cq->dbrec) = htobe32(_consumer_index & 0xffffff);
        __sync_synchronize();
        return STATUS_OK;
    }
    return STATUS_ERR;
}

uint32_t completion_queue::get_cqn() const {
    return _cqn;
}

struct ibv_cq* completion_queue::get() const {
    return ibv_cq_ex_to_cq(_pcq);
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
    _sq_buf_offset(0)
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

STATUS
queue_pair::initialize(qp_init_creation_params& params) {
    if (_qp) {
        return STATUS_OK;
    }

    uint32_t in[DEVX_ST_SZ_DW(create_qp_in)]   = {0};
    uint32_t out[DEVX_ST_SZ_DW(create_qp_out)] = {0};

    DEVX_SET(create_qp_in, in, opcode, MLX5_CMD_OP_CREATE_QP);

    void* qpc = DEVX_ADDR_OF(create_qp_in, in, qpc);

    DEVX_SET(qpc, qpc, st, MLX5_QPC_ST_RC);

    DEVX_SET(qpc, qpc, pd, params.pdn);
    DEVX_SET(qpc, qpc, cqn_snd, params.cqn);
    DEVX_SET(qpc, qpc, cqn_rcv, params.cqn);

    DEVX_SET(qpc, qpc, log_sq_size, ilog2(params.sq_size));
    DEVX_SET(qpc, qpc, log_rq_size, ilog2(params.rq_size));
    DEVX_SET(qpc, qpc, log_rq_stride, 5);
    DEVX_SET(qpc, qpc, no_sq, 0);

    DEVX_SET(qpc, qpc, uar_page, params.uar_obj->get()->page_id);

    DEVX_SET(qpc, qpc, dbr_umem_id, params.umem_db->get()->umem_id);
    DEVX_SET(qpc, qpc, dbr_umem_valid, 1);

    DEVX_SET(create_qp_in, in, wq_umem_id, params.umem_sq->get()->umem_id);
    DEVX_SET(create_qp_in, in, wq_umem_valid, 1);

    DEVX_SET(qpc, qpc, log_page_size, get_page_size_log());
    DEVX_SET(qpc, qpc, rae, 1);
    DEVX_SET(qpc, qpc, rwe, 1);
    DEVX_SET(qpc, qpc, rre, 1);
    DEVX_SET(qpc, qpc, atomic_mode, 1);

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
    _rdevice = params.rdevice;
    
    // Initialize send queue parameters
    _sq_size = params.sq_size;
    _sq_pi = 0;
    _sq_ci = 0;
    _sq_dbr_offset = 0;  // Usually the first offset in the doorbell record
    _sq_buf_offset = 0;  // Base offset in the send queue buffer
    
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

    if (!(_rdevice->get_port_attr(1)->link_layer == IBV_LINK_LAYER_ETHERNET)) {
        DEVX_SET(qpc, qpc, primary_address_path.pkey_index, 0);
    }

    DEVX_SET(qpc, qpc, primary_address_path.vhca_port_num, params.port_num);
    DEVX_SET(qpc, qpc, pm_state, MLX5_QPC_PM_STATE_MIGRATED);
    DEVX_SET(qpc, qpc, mtu, params.mtu);

    if (mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out))) {
        log_error("Failed QP RST to INIT qpn: %d", _qpn);
        return STATUS_ERR;
    }

    log_debug("Reset QP to INIT qpn: %d", _qpn);
    
    return STATUS_OK;
}

STATUS
queue_pair::init_to_rtr(qp_init_connection_params &params)
{
    if (!_qp) {
        return STATUS_ERR;
    }

    uint32_t in[DEVX_ST_SZ_DW(init2rtr_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(init2rtr_qp_out)] = {0};

    DEVX_SET(init2rtr_qp_in, in, opcode, MLX5_CMD_OP_INIT2RTR_QP);
    DEVX_SET(init2rtr_qp_in, in, qpn, _qpn);

    void* qpc = DEVX_ADDR_OF(init2rtr_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, mtu, params.mtu);
    DEVX_SET(init2rtr_qp_in, in, ece, params.ece);

    if (mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out))) {
        log_error("Failed to modify QP to RTR qpn: %d", _qpn);
        return STATUS_ERR;
    }

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

    STATUS res = create_ah(params.pd, params.remote_ah_attr);
    RETURN_IF_FAILED(res);

    auto _ah_attr = params.remote_ah_attr;

    mlx5_wqe_av mlx5_av = {};
    objects_get_av(_ah ,&mlx5_av);

    uint32_t in[DEVX_ST_SZ_DW(rtr2rts_qp_in)] = {0};
    uint32_t out[DEVX_ST_SZ_DW(rtr2rts_qp_out)] = {0};

    DEVX_SET(rtr2rts_qp_in, in, opcode, MLX5_CMD_OP_RTR2RTS_QP);
    DEVX_SET(rtr2rts_qp_in, in, qpn, _qpn);

    void* qpc = DEVX_ADDR_OF(rtr2rts_qp_in, in, qpc);
    DEVX_SET(qpc, qpc, retry_count, params.retry_count);
    DEVX_SET(qpc, qpc, rnr_retry, params.rnr_retry);

    if (_rdevice->get_port_attr(1)->link_layer == IBV_LINK_LAYER_ETHERNET) {
        memcpy(DEVX_ADDR_OF(qpc, qpc, primary_address_path.rmac_47_32),
                mlx5_av.rmac, sizeof(mlx5_av.rmac));
        memcpy(DEVX_ADDR_OF(qpc, qpc, primary_address_path.rgid_rip),
                mlx5_av.rgid, sizeof(mlx5_av.rgid));

        DEVX_SET(qpc, qpc, primary_address_path.hop_limit, mlx5_av.hop_limit);
        DEVX_SET(qpc, qpc, primary_address_path.src_addr_index, _ah_attr->grh.sgid_index);
        DEVX_SET(qpc, qpc, primary_address_path.eth_prio, params.sl);
        // For RoCE v2
        DEVX_SET(qpc, qpc, primary_address_path.udp_sport, _ah_attr->dlid);
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

    DEVX_SET(qpc, qpc, primary_address_path.vhca_port_num, _ah_attr->port_num);
    DEVX_SET(qpc, qpc, log_rra_max, ilog2(_rdevice->get_device_attr()->max_qp_rd_atom));
    DEVX_SET(qpc, qpc, min_rnr_nak, params.min_rnr_to);

        if (mlx5dv_devx_obj_modify(_qp, in, sizeof(in), out, sizeof(out))) {
        log_error("Failed to modify QP to RTS qpn: %d", _qpn);
        return STATUS_ERR;
    }

    return STATUS_OK;
}

#define RDMA_WQE_SEG_SIZE       64   // Size of a WQE segment (one basic block)
#define RDMA_MAX_WQE_BB         4    // Maximum number of basic blocks per WQE
#define MLX5_SEND_WQE_BB        64   // Basic block size for send WQEs
#define MLX5_OPCODE_RDMA_WRITE  8    // RDMA Write opcode
#define MLX5_OPCODE_RDMA_READ   12   // RDMA Read opcode
#define MLX5_OPCODE_SEND        0    // Send opcode
#define MLX5_OPCODE_SEND_IMM    1    // Send with immediate opcode
#define MLX5_OPCODE_RDMA_WRITE_IMM 9 // RDMA Write with immediate opcode

// Implementation of the post_send method in queue_pair class
STATUS queue_pair::post_send(struct mlx5_wqe_ctrl_seg* ctrl, unsigned wqe_size) {
    // Make sure control segment is aligned
    if ((uintptr_t)ctrl % RDMA_WQE_SEG_SIZE != 0) {
        log_error("WQE control segment not aligned to %d bytes", RDMA_WQE_SEG_SIZE);
        return STATUS_ERR;
    }
    
    // Calculate the number of basic blocks (BB) this WQE will consume
    uint16_t num_bb = (wqe_size + MLX5_SEND_WQE_BB - 1) / MLX5_SEND_WQE_BB;
    
    // Ensure we have enough space in the queue
    uint16_t available = _sq_size - ((_sq_pi - _sq_ci) % _sq_size);
    if (available < num_bb) {
        log_error("Send queue full, not enough space for WQE");
        return STATUS_ERR;
    }
    
    // Calculate the DB value: new producer index after posting this WQE
    uint16_t new_pi = (_sq_pi + num_bb) % _sq_size;
    
    // Ensure all WQE stores are visible before writing doorbell record
    __sync_synchronize(); // Full memory barrier
    
    // Write control+data to the BlueFlame register using 64-bit writes
    void* bf_reg = _uar->get()->reg_addr;
    void* src = ctrl;
    __be64* dst64 = (__be64*)bf_reg;
    __be64* src64 = (__be64*)src;
    for (size_t i = 0; i < (num_bb * MLX5_SEND_WQE_BB / sizeof(__be64)); ++i) {
        dst64[i] = src64[i];
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    
    // Write doorbell record - this is where HW reads the current queue position
    volatile uint32_t* dbrec = (volatile uint32_t*)((char*)_umem_db->addr() + _sq_dbr_offset);
    *dbrec = htobe32(new_pi & 0xFFFF);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    
    // Update the queue pointer
    _sq_pi = new_pi;
    
    return STATUS_OK;
}

// Implementation of the post_wqe helper method
STATUS queue_pair::post_wqe(uint8_t opcode, void* laddr, uint32_t lkey,
                          uint64_t raddr, uint32_t rkey, uint32_t length,
                          uint32_t imm_data, uint32_t flags) {
    // Calculate WQE size based on segments needed
    size_t wqe_size = sizeof(mlx5_wqe_ctrl_seg);  // Control segment always present
    
    // Add data segments size
    wqe_size += sizeof(mlx5_wqe_data_seg);
    
    // Add remote address segment for RDMA operations
    bool need_raddr = (opcode == MLX5_OPCODE_RDMA_WRITE ||
                       opcode == MLX5_OPCODE_RDMA_WRITE_IMM ||
                       opcode == MLX5_OPCODE_RDMA_READ);
    if (need_raddr) {
        wqe_size += sizeof(mlx5_wqe_raddr_seg);
    }
    
    // Round up to nearest segment boundary
    wqe_size = (wqe_size + RDMA_WQE_SEG_SIZE - 1) & ~(RDMA_WQE_SEG_SIZE - 1);
    
    // Get pointer to the WQE in the send queue
    mlx5_wqe_ctrl_seg* ctrl = (mlx5_wqe_ctrl_seg*)((char*)_umem_sq->addr() + (_sq_pi % _sq_size) * RDMA_WQE_SEG_SIZE);
    
    // Set up control segment
    memset(ctrl, 0, wqe_size);
    ctrl->opmod_idx_opcode = htobe32((_sq_pi & 0xffff) << 8 | opcode);
    ctrl->qpn_ds = htobe32((_qpn & 0xFFFFFF) << 8 | (wqe_size / 16));
    ctrl->fm_ce_se = (flags & 0x7); // FM, CE, SE flags
    
    // Initialize segment pointer for next segment
    void* segment = ctrl + 1;
    
    // Add remote address segment for RDMA operations
    if (need_raddr) {
        mlx5_wqe_raddr_seg* raddr_seg = (mlx5_wqe_raddr_seg*)segment;
        raddr_seg->raddr = htobe64(raddr);
        raddr_seg->rkey = htobe32(rkey);
        segment = raddr_seg + 1;
    }
    
    // Add data segment
    mlx5_wqe_data_seg* data_seg = (mlx5_wqe_data_seg*)segment;
    data_seg->byte_count = htobe32(length);
    data_seg->lkey = htobe32(lkey);
    data_seg->addr = htobe64((uint64_t)laddr);
    
    // For operations with immediate data
    if (opcode == MLX5_OPCODE_SEND_IMM || opcode == MLX5_OPCODE_RDMA_WRITE_IMM) {
        ctrl->imm = htobe32(imm_data);
    }
    
    // Post the WQE to the hardware
    return post_send(ctrl, wqe_size);
}

// Implementations of the convenience methods
STATUS queue_pair::post_rdma_write(void* laddr, uint32_t lkey, 
                                uint64_t raddr, uint32_t rkey, 
                                uint32_t length, uint32_t flags) {
    return post_wqe(MLX5_OPCODE_RDMA_WRITE, laddr, lkey, raddr, rkey, length, 0, flags);
}

STATUS queue_pair::post_rdma_read(void* laddr, uint32_t lkey, 
                               uint64_t raddr, uint32_t rkey, 
                               uint32_t length, uint32_t flags) {
    return post_wqe(MLX5_OPCODE_RDMA_READ, laddr, lkey, raddr, rkey, length, 0, flags);
}

STATUS queue_pair::post_send_msg(void* laddr, uint32_t lkey, 
                              uint32_t length, uint32_t flags) {
    return post_wqe(MLX5_OPCODE_SEND, laddr, lkey, 0, 0, length, 0, flags);
}

STATUS queue_pair::post_send_imm(void* laddr, uint32_t lkey, 
                              uint32_t length, uint32_t imm_data, 
                              uint32_t flags) {
    return post_wqe(MLX5_OPCODE_SEND_IMM, laddr, lkey, 0, 0, length, imm_data, flags);
}

STATUS queue_pair::post_rdma_write_imm(void* laddr, uint32_t lkey, 
                                    uint64_t raddr, uint32_t rkey, 
                                    uint32_t length, uint32_t imm_data, 
                                    uint32_t flags) {
    return post_wqe(MLX5_OPCODE_RDMA_WRITE_IMM, laddr, lkey, raddr, rkey, length, imm_data, flags);
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

