#pragma once

#include <string>
#include <vector>
#include <map>
#include "../common/rdma_common.h"
#include "../common/auto_ref.h"
#include "rdma_device.h"

// Forward declaration
class rdma_general_device;

/**
 * @brief Represents a physical host in the network
 * 
 * A host can have multiple RDMA devices (HCAs) and manages their lifecycle.
 */
class host {
public:
    /**
     * @brief Create a host object
     * @param hostname Name of the host (defaults to local hostname)
     */
    explicit host(const std::string& hostname = "");
    
    /**
     * @brief Destructor
     */
    ~host();
    
    /**
     * @brief Initialize the host and discover RDMA devices
     * @return STATUS_OK on success, error code on failure
     */
    STATUS initialize();
    
    /**
     * @brief Get the host's name
     * @return Host name
     */
    std::string get_hostname() const;
    
    /**
     * @brief Get device by name
     * @param device_name Name of the device
     * @return Pointer to device or nullptr if not found
     */
    rdma_general_device* get_device(const std::string& device_name);
    
    /**
     * @brief Get all devices
     * @return Vector of device pointers
     */
    std::vector<rdma_general_device*> get_all_devices();
    
    /**
     * @brief Get device count
     * @return Number of RDMA devices on this host
     */
    size_t get_device_count() const;
    
    /**
     * @brief Print information about all devices
     */
    void print_device_info() const;

private:
    std::string _hostname;
    bool _initialized = false;
    std::map<std::string, auto_ref<rdma_general_device>> _devices;
    
    /**
     * @brief Discover all RDMA devices on the host
     * @return STATUS_OK on success, error code on failure
     */
    STATUS discover_devices();
};