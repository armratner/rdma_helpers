#include "rdma_device.h"
#include <cstring>

rdma_general_device::rdma_general_device()
    : _device_name("")
    , _initialized(false)
{
}

rdma_general_device::rdma_general_device(const std::string& device_name) 
    : _device_name(device_name)
    , _initialized(false) 
{
}

rdma_general_device::~rdma_general_device() 
{
    // Resources will be automatically cleaned up by auto_ref destructors
}

STATUS rdma_general_device::initialize() 
{
    if (_initialized) {
        return STATUS_OK;
    }
    
    // Initialize the underlying RDMA device
    STATUS status = _rdma_device->initialize(_device_name);
    if (FAILED(status)) {
        log_error("Failed to initialize RDMA device %s", _device_name.c_str());
        return status;
    }
    
    // Create the default protection domain
    status = create_default_protection_domain();
    if (FAILED(status)) {
        log_error("Failed to create default protection domain for device %s", 
                 _device_name.c_str());
        return status;
    }
    
    _initialized = true;
    return STATUS_OK;
}

std::string rdma_general_device::get_name() const 
{
    return _device_name;
}

void rdma_general_device::set_name(const std::string& device_name)
{
    _device_name = device_name;
}

::rdma_device* rdma_general_device::get_rdma_device() 
{
    return _rdma_device.get();
}

protection_domain* rdma_general_device::get_protection_domain(const std::string& pd_name) {
    // Check if PD already exists
    auto it = _protection_domains.find(pd_name);
    if (it != _protection_domains.end()) {
        return it->second.get();
    }
    
    // If not the default PD, create a new one
    if (pd_name != "default") {
        auto_ref<protection_domain> pd;
        STATUS status = pd->initialize(_rdma_device->get_context());
        
        if (FAILED(status)) {
            log_error("Failed to create protection domain '%s'", pd_name.c_str());
            return nullptr;
        }
        
        _protection_domains[pd_name] = std::move(pd);
        return _protection_domains[pd_name].get();
    }
    
    // Should never reach here if initialize() was called
    log_error("Default protection domain not found");
    return nullptr;
}

queue_pair* rdma_general_device::create_queue_pair(
    const std::string& qp_name,
    const std::string& pd_name,
    const qp_init_creation_params* params
) {
    // Check if QP with this name already exists
    if (_queue_pairs.find(qp_name) != _queue_pairs.end()) {
        log_error("Queue pair '%s' already exists", qp_name.c_str());
        return nullptr;
    }
    
    // Get the protection domain
    protection_domain* pd = get_protection_domain(pd_name);
    if (!pd) {
        log_error("Protection domain '%s' not found", pd_name.c_str());
        return nullptr;
    }
    
    // Create queue pair (auto_ref constructor creates the object)
    auto_ref<queue_pair> qp;
    
    // If params are provided, use them; otherwise create default params
    qp_init_creation_params creation_params;
    if (params) {
        creation_params = *params;
    } else {
        // Set default values
        creation_params.rdevice = _rdma_device.get();
        creation_params.context = _rdma_device->get_context();
        creation_params.pdn = pd->get_pdn();
        
        // Create default completion queue if needed
        completion_queue* cq = get_completion_queue("default");
        if (!cq) {
            cq = create_completion_queue("default");
            if (!cq) {
                log_error("Failed to create default completion queue");
                return nullptr;
            }
        }
        creation_params.cqn = cq->get_cqn();
        
        // Default QP sizes
        creation_params.sq_size = 128;
        creation_params.rq_size = 128;
        creation_params.max_send_wr = 64;
        creation_params.max_recv_wr = 64;
        creation_params.max_send_sge = 1;
        creation_params.max_recv_sge = 1;
        creation_params.max_inline_data = 64;
        creation_params.max_rd_atomic = 16;
        creation_params.max_dest_rd_atomic = 16;
    }
    
    // Initialize the queue pair
    STATUS status = qp->initialize(creation_params);
    if (FAILED(status)) {
        log_error("Failed to initialize queue pair '%s'", qp_name.c_str());
        return nullptr;
    }
    
    _queue_pairs[qp_name] = std::move(qp);
    return _queue_pairs[qp_name].get();
}

completion_queue* rdma_general_device::create_completion_queue(
    const std::string& cq_name,
    const cq_creation_params* params
) {
    // Check if CQ with this name already exists
    if (_completion_queues.find(cq_name) != _completion_queues.end()) {
        log_error("Completion queue '%s' already exists", cq_name.c_str());
        return nullptr;
    }
    
    // Create completion queue (auto_ref constructor creates the object)
    auto_ref<completion_queue> cq;
    
    // If params are provided, use them; otherwise create default params
    cq_creation_params creation_params;
    if (params) {
        creation_params = *params;
    } else {
        // Set default values
        creation_params.context = _rdma_device->get_context();
        
        // Create default CQ attribute structures
        ibv_cq_init_attr_ex* cq_attr_ex = aligned_alloc<ibv_cq_init_attr_ex>(1);
        mlx5dv_cq_init_attr* dv_cq_attr = aligned_alloc<mlx5dv_cq_init_attr>(1);
        
        if (!cq_attr_ex || !dv_cq_attr) {
            log_error("Failed to allocate memory for CQ attributes");
            if (cq_attr_ex) free(cq_attr_ex);
            if (dv_cq_attr) free(dv_cq_attr);
            return nullptr;
        }
        
        memset(cq_attr_ex, 0, sizeof(ibv_cq_init_attr_ex));
        memset(dv_cq_attr, 0, sizeof(mlx5dv_cq_init_attr));
        
        // Default CQ size and attributes
        cq_attr_ex->cqe = 1024;
        cq_attr_ex->channel = nullptr;
        cq_attr_ex->comp_vector = 0;
        cq_attr_ex->wc_flags = IBV_WC_EX_WITH_BYTE_LEN;
        cq_attr_ex->comp_mask = IBV_CQ_INIT_ATTR_MASK_FLAGS;
        cq_attr_ex->flags = 0;
        
        dv_cq_attr->comp_mask = MLX5DV_CQ_INIT_ATTR_MASK_COMPRESSED_CQE;
        // MLX5 specific CQ attributes
        // Note: cqe_comp_en may not be defined in older versions of the library
        
        creation_params.cq_attr_ex = cq_attr_ex;
        creation_params.dv_cq_attr = dv_cq_attr;
    }
    
    // Initialize the completion queue
    STATUS status = cq->initialize(creation_params);
    
    // Free allocated memory for default parameters
    if (!params) {
        if (creation_params.cq_attr_ex) {
            free(creation_params.cq_attr_ex);
        }
        if (creation_params.dv_cq_attr) {
            free(creation_params.dv_cq_attr);
        }
    }
    
    if (FAILED(status)) {
        log_error("Failed to initialize completion queue '%s'", cq_name.c_str());
        return nullptr;
    }
    
    _completion_queues[cq_name] = std::move(cq);
    return _completion_queues[cq_name].get();
}

queue_pair* rdma_general_device::get_queue_pair(const std::string& qp_name) {
    auto it = _queue_pairs.find(qp_name);
    if (it != _queue_pairs.end()) {
        return it->second.get();
    }
    return nullptr;
}

completion_queue* rdma_general_device::get_completion_queue(const std::string& cq_name) {
    auto it = _completion_queues.find(cq_name);
    if (it != _completion_queues.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<queue_pair*> rdma_general_device::get_all_queue_pairs() {
    std::vector<queue_pair*> qps;
    for (auto& pair : _queue_pairs) {
        qps.push_back(pair.second.get());
    }
    return qps;
}

std::vector<completion_queue*> rdma_general_device::get_all_completion_queues() {
    std::vector<completion_queue*> cqs;
    for (auto& pair : _completion_queues) {
        cqs.push_back(pair.second.get());
    }
    return cqs;
}

void rdma_general_device::print_info() const {
    if (!_rdma_device) {
        log_error("Device %s not initialized", _device_name.c_str());
        return;
    }
    
    // Print device attributes
    log_info("Device Name: %s", _device_name.c_str());
    _rdma_device->print_device_attr();
    _rdma_device->print_port_attr();
    
    // Print resources
    log_info("Protection Domains: %zu", _protection_domains.size());
    log_info("Queue Pairs: %zu", _queue_pairs.size());
    log_info("Completion Queues: %zu", _completion_queues.size());
    log_info("Memory Regions: %zu", _memory_regions.size());
    log_info("User Memories: %zu", _user_memories.size());
    log_info("Memory Keys: %zu", _memory_keys.size());
    log_info("UARs: %zu", _uars.size());
}

STATUS rdma_general_device::create_default_protection_domain() 
{
    // auto_ref's constructor already creates the protection_domain
    auto_ref<protection_domain> pd;
    STATUS status = pd->initialize(_rdma_device->get_context());
    
    if (FAILED(status)) {
        log_error("Failed to create default protection domain");
        return status;
    }
    
    _protection_domains["default"] = std::move(pd);
    return STATUS_OK;
}

memory_region* rdma_general_device::create_memory_region(
    const std::string& mr_name,
    const std::string& qp_name,
    const std::string& pd_name,
    void* addr,
    size_t length
) {
    // Check if MR with this name already exists
    if (_memory_regions.find(mr_name) != _memory_regions.end()) {
        log_error("Memory region '%s' already exists", mr_name.c_str());
        return nullptr;
    }
    
    // Get the queue pair
    queue_pair* qp = get_queue_pair(qp_name);
    if (!qp) {
        log_error("Queue pair '%s' not found", qp_name.c_str());
        return nullptr;
    }
    
    // Get the protection domain
    protection_domain* pd = get_protection_domain(pd_name);
    if (!pd) {
        log_error("Protection domain '%s' not found", pd_name.c_str());
        return nullptr;
    }
    
    // Create memory region (auto_ref constructor creates the object)
    auto_ref<memory_region> mr;
    STATUS status = mr->initialize(_rdma_device.get(), qp, pd, addr, length);
    
    if (FAILED(status)) {
        log_error("Failed to initialize memory region '%s'", mr_name.c_str());
        return nullptr;
    }
    
    _memory_regions[mr_name] = std::move(mr);
    return _memory_regions[mr_name].get();
}

user_memory* rdma_general_device::create_user_memory(
    const std::string& umem_name,
    size_t size
) {
    // Check if user memory with this name already exists
    if (_user_memories.find(umem_name) != _user_memories.end()) {
        log_error("User memory '%s' already exists", umem_name.c_str());
        return nullptr;
    }
    
    // Create user memory (auto_ref constructor creates the object)
    auto_ref<user_memory> umem;
    STATUS status = umem->initialize(_rdma_device->get_context(), size);
    
    if (FAILED(status)) {
        log_error("Failed to initialize user memory '%s'", umem_name.c_str());
        return nullptr;
    }
    
    _user_memories[umem_name] = std::move(umem);
    return _user_memories[umem_name].get();
}

memory_key* rdma_general_device::create_memory_key(
    const std::string& mkey_name,
    const std::string& pd_name,
    uint32_t access,
    uint32_t num_entries
) {
    // Check if memory key with this name already exists
    if (_memory_keys.find(mkey_name) != _memory_keys.end()) {
        log_error("Memory key '%s' already exists", mkey_name.c_str());
        return nullptr;
    }
    
    // Get the protection domain
    protection_domain* pd = get_protection_domain(pd_name);
    if (!pd) {
        log_error("Protection domain '%s' not found", pd_name.c_str());
        return nullptr;
    }
    
    // Create memory key (auto_ref constructor creates the object)
    auto_ref<memory_key> mkey;
    STATUS status = mkey->initialize(pd->get(), access, num_entries);
    
    if (FAILED(status)) {
        log_error("Failed to initialize memory key '%s'", mkey_name.c_str());
        return nullptr;
    }
    
    _memory_keys[mkey_name] = std::move(mkey);
    return _memory_keys[mkey_name].get();
}

uar* rdma_general_device::create_uar(const std::string& uar_name) {
    // Check if UAR with this name already exists
    if (_uars.find(uar_name) != _uars.end()) {
        log_error("UAR '%s' already exists", uar_name.c_str());
        return nullptr;
    }
    
    // Create UAR (auto_ref constructor creates the object)
    auto_ref<uar> new_uar;
    STATUS status = new_uar->initialize(_rdma_device->get_context());
    
    if (FAILED(status)) {
        log_error("Failed to initialize UAR '%s'", uar_name.c_str());
        return nullptr;
    }
    
    _uars[uar_name] = std::move(new_uar);
    return _uars[uar_name].get();
}

memory_region* rdma_general_device::get_memory_region(const std::string& mr_name) {
    auto it = _memory_regions.find(mr_name);
    if (it != _memory_regions.end()) {
        return it->second.get();
    }
    return nullptr;
}

user_memory* rdma_general_device::get_user_memory(const std::string& umem_name) {
    auto it = _user_memories.find(umem_name);
    if (it != _user_memories.end()) {
        return it->second.get();
    }
    return nullptr;
}

memory_key* rdma_general_device::get_memory_key(const std::string& mkey_name) {
    auto it = _memory_keys.find(mkey_name);
    if (it != _memory_keys.end()) {
        return it->second.get();
    }
    return nullptr;
}

uar* rdma_general_device::get_uar(const std::string& uar_name) {
    auto it = _uars.find(uar_name);
    if (it != _uars.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<memory_region*> rdma_general_device::get_all_memory_regions() {
    std::vector<memory_region*> regions;
    for (auto& pair : _memory_regions) {
        regions.push_back(pair.second.get());
    }
    return regions;
}

std::vector<user_memory*> rdma_general_device::get_all_user_memories() {
    std::vector<user_memory*> memories;
    for (auto& pair : _user_memories) {
        memories.push_back(pair.second.get());
    }
    return memories;
}

std::vector<memory_key*> rdma_general_device::get_all_memory_keys() {
    std::vector<memory_key*> keys;
    for (auto& pair : _memory_keys) {
        keys.push_back(pair.second.get());
    }
    return keys;
}

std::vector<uar*> rdma_general_device::get_all_uars() {
    std::vector<uar*> uars;
    for (auto& pair : _uars) {
        uars.push_back(pair.second.get());
    }
    return uars;
}