#include "host.h"
#include "rdma_device.h"
#include <iostream>
#include <vector>
#include <cstring>

// Allocate a buffer of specified size
void* allocate_buffer(size_t size) {
    void* buffer = nullptr;
    const size_t page_size = get_page_size();
    posix_memalign(&buffer, page_size, size);
    if (buffer) {
        memset(buffer, 0, size);
    }
    return buffer;
}

int main() {
    // Set log level to info
    set_log_level(LOG_LEVEL_INFO);
    
    // Create a host with default hostname
    host local_host;
    
    // Initialize the host (discovers devices)
    STATUS status = local_host.initialize();
    if (FAILED(status)) {
        std::cerr << "Failed to initialize host" << std::endl;
        return 1;
    }
    
    // Print host info
    std::cout << "Host: " << local_host.get_hostname() << std::endl;
    std::cout << "Found " << local_host.get_device_count() << " RDMA device(s)" << std::endl;
    
    // Print detailed info for all devices
    local_host.print_device_info();
    
    // Get all devices
    auto devices = local_host.get_all_devices();
    
    // For each device, create resources
    for (auto device : devices) {
        std::string device_name = device->get_name();
        std::cout << "Creating resources on device: " << device_name << std::endl;
        
        // Create a UAR for doorbell
        uar* device_uar = device->create_uar("test_uar");
        if (!device_uar) {
            std::cerr << "Failed to create UAR on device " << device_name << std::endl;
            continue;
        }
        
        // Create user memory for SQ and DB
        user_memory* sq_mem = device->create_user_memory("sq_mem", 4096);
        user_memory* db_mem = device->create_user_memory("db_mem", 4096);
        
        if (!sq_mem || !db_mem) {
            std::cerr << "Failed to create user memory on device " << device_name << std::endl;
            continue;
        }
        
        // Create custom QP parameters
        qp_init_creation_params qp_params;
        memset(&qp_params, 0, sizeof(qp_params));
        
        // Set QP parameters
        qp_params.rdevice = device->get_rdma_device();
        qp_params.context = device->get_rdma_device()->get_context();
        qp_params.pdn = device->get_protection_domain()->get_pdn();
        qp_params.uar_obj = device_uar;
        qp_params.umem_sq = sq_mem;
        qp_params.umem_db = db_mem;
        qp_params.sq_size = 128;
        qp_params.rq_size = 128;
        qp_params.max_send_wr = 64;
        qp_params.max_recv_wr = 64;
        qp_params.max_send_sge = 1;
        qp_params.max_recv_sge = 1;
        qp_params.max_inline_data = 64;
        qp_params.max_rd_atomic = 16;
        qp_params.max_dest_rd_atomic = 16;
        
        // Create a completion queue
        completion_queue* cq = device->create_completion_queue("test_cq");
        if (!cq) {
            std::cerr << "Failed to create CQ on device " << device_name << std::endl;
            continue;
        }
        qp_params.cqn = cq->get_cqn();
        
        // Create a queue pair
        queue_pair* qp = device->create_queue_pair("test_qp", "default", &qp_params);
        if (!qp) {
            std::cerr << "Failed to create QP on device " << device_name << std::endl;
            continue;
        }
        
        std::cout << "Created QP with ID: " << qp->get_qpn() << std::endl;
        
        // Allocate and register memory
        const size_t mem_size = 4096;
        void* buffer = allocate_buffer(mem_size);
        if (!buffer) {
            std::cerr << "Failed to allocate memory buffer" << std::endl;
            continue;
        }
        
        // Register memory region
        memory_region* mr = device->create_memory_region("test_mr", "test_qp", "default", buffer, mem_size);
        if (!mr) {
            std::cerr << "Failed to register memory region" << std::endl;
            free(buffer);
            continue;
        }
        
        std::cout << "Created memory region with lkey: " << mr->get_lkey() << std::endl;
        
        // Create a memory key
        memory_key* mkey = device->create_memory_key("test_mkey", "default", IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ, 1);
        if (!mkey) {
            std::cerr << "Failed to create memory key" << std::endl;
            free(buffer);
            continue;
        }
        
        std::cout << "Created memory key with lkey: " << mkey->get_lkey() << std::endl;
        
        // Print summary of created resources
        std::cout << "\nResources created on device " << device_name << ":" << std::endl;
        device->print_info();
        
        // Clean up allocated buffer
        free(buffer);
    }
    
    return 0;
}