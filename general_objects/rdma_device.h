#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstring>
#include "../common/rdma_common.h"
#include "../common/auto_ref.h"
#include "../rdma_objects/rdma_objects.h"

/**
 * @brief Represents an RDMA Host Channel Adapter (HCA)
 * 
 * Encapsulates a physical RDMA device and manages its resources
 * such as protection domains, queue pairs, and completion queues.
 */
class rdma_general_device {
public:
    /**
     * @brief Default constructor
     */
    rdma_general_device();
    
    /**
     * @brief Create an RDMA device
     * @param device_name Name of the physical device
     */
    explicit rdma_general_device(const std::string& device_name);
    
    /**
     * @brief Destructor
     */
    ~rdma_general_device();
    
    /**
     * @brief Initialize the device
     * @return STATUS_OK on success, error code on failure
     */
    STATUS initialize();
    
    /**
     * @brief Get the device name
     * @return Device name
     */
    std::string get_name() const;
    
    /**
     * @brief Set the device name
     * @param device_name Name for the device
     */
    void set_name(const std::string& device_name);
    
    /**
     * @brief Get the underlying RDMA device object
     * @return Pointer to the RDMA device object
     */
    ::rdma_device* get_rdma_device();
    
    /**
     * @brief Get or create a protection domain
     * @param pd_name Name for the protection domain
     * @return Pointer to the protection domain
     */
    protection_domain* get_protection_domain(const std::string& pd_name = "default");
    
    /**
     * @brief Create a queue pair
     * @param qp_name Name for the queue pair
     * @param pd_name Name of the protection domain to use
     * @param params Queue pair creation parameters
     * @return Pointer to the queue pair or nullptr on failure
     */
    queue_pair* create_queue_pair(
        const std::string& qp_name,
        const std::string& pd_name = "default",
        const qp_init_creation_params* params = nullptr
    );
    
    /**
     * @brief Create a completion queue
     * @param cq_name Name for the completion queue
     * @param params Completion queue creation parameters
     * @return Pointer to the completion queue or nullptr on failure
     */
    completion_queue_devx* create_completion_queue(
        const std::string& cq_name,
        const cq_creation_params* params = nullptr
    );
    
    /**
     * @brief Create a memory region
     * @param mr_name Name for the memory region
     * @param qp_name Name of the queue pair to associate with
     * @param pd_name Name of the protection domain to use
     * @param addr Memory address to register
     * @param length Length of the memory region
     * @return Pointer to the memory region or nullptr on failure
     */
    memory_region* create_memory_region(
        const std::string& mr_name,
        const std::string& qp_name,
        const std::string& pd_name,
        void* addr,
        size_t length
    );
    
    /**
     * @brief Create a user memory region
     * @param umem_name Name for the user memory region
     * @param size Size of the memory region to allocate
     * @return Pointer to the user memory or nullptr on failure
     */
    user_memory* create_user_memory(
        const std::string& umem_name,
        size_t size
    );
    
    /**
     * @brief Create a memory key
     * @param mkey_name Name for the memory key
     * @param pd_name Name of the protection domain to use
     * @param access Access flags for the memory key
     * @param num_entries Number of entries for the memory key
     * @return Pointer to the memory key or nullptr on failure
     */
    memory_key* create_memory_key(
        const std::string& mkey_name,
        const std::string& pd_name,
        uint32_t access,
        uint32_t num_entries
    );
    
    /**
     * @brief Create a UAR
     * @param uar_name Name for the UAR
     * @return Pointer to the UAR or nullptr on failure
     */
    uar* create_uar(const std::string& uar_name);
    
    /**
     * @brief Get queue pair by name
     * @param qp_name Name of the queue pair
     * @return Pointer to the queue pair or nullptr if not found
     */
    queue_pair* get_queue_pair(const std::string& qp_name);
    
    /**
     * @brief Get completion queue by name
     * @param cq_name Name of the completion queue
     * @return Pointer to the completion queue or nullptr if not found
     */
    completion_queue_devx* get_completion_queue(const std::string& cq_name);
    
    /**
     * @brief Get memory region by name
     * @param mr_name Name of the memory region
     * @return Pointer to the memory region or nullptr if not found
     */
    memory_region* get_memory_region(const std::string& mr_name);
    
    /**
     * @brief Get user memory by name
     * @param umem_name Name of the user memory
     * @return Pointer to the user memory or nullptr if not found
     */
    user_memory* get_user_memory(const std::string& umem_name);
    
    /**
     * @brief Get memory key by name
     * @param mkey_name Name of the memory key
     * @return Pointer to the memory key or nullptr if not found
     */
    memory_key* get_memory_key(const std::string& mkey_name);
    
    /**
     * @brief Get UAR by name
     * @param uar_name Name of the UAR
     * @return Pointer to the UAR or nullptr if not found
     */
    uar* get_uar(const std::string& uar_name);
    
    /**
     * @brief Get all queue pairs
     * @return Vector of queue pair pointers
     */
    std::vector<queue_pair*> get_all_queue_pairs();
    
    /**
     * @brief Get all completion queues
     * @return Vector of completion queue pointers
     */
    std::vector<completion_queue_devx*> get_all_completion_queues();
    
    /**
     * @brief Get all memory regions
     * @return Vector of memory region pointers
     */
    std::vector<memory_region*> get_all_memory_regions();
    
    /**
     * @brief Get all user memories
     * @return Vector of user memory pointers
     */
    std::vector<user_memory*> get_all_user_memories();
    
    /**
     * @brief Get all memory keys
     * @return Vector of memory key pointers
     */
    std::vector<memory_key*> get_all_memory_keys();
    
    /**
     * @brief Get all UARs
     * @return Vector of UAR pointers
     */
    std::vector<uar*> get_all_uars();
    
    /**
     * @brief Print device information
     */
    void print_info() const;

private:
    std::string _device_name;
    bool _initialized = false;
    auto_ref<::rdma_device> _rdma_device;
    
    // Resource management
    std::map<std::string, auto_ref<protection_domain>> _protection_domains;
    std::map<std::string, auto_ref<queue_pair>> _queue_pairs;
    std::map<std::string, auto_ref<completion_queue_devx>> _completion_queues;
    std::map<std::string, auto_ref<memory_region>> _memory_regions;
    std::map<std::string, auto_ref<user_memory>> _user_memories;
    std::map<std::string, auto_ref<memory_key>> _memory_keys;
    std::map<std::string, auto_ref<uar>> _uars;
    
    // Helper for creating default protection domain
    STATUS create_default_protection_domain();
};