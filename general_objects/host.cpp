#include "host.h"
#include <unistd.h>
#include <cstring>

host::host(const std::string& hostname) 
    : _hostname(hostname)
    , _initialized(false) 
{
    if (_hostname.empty()) {
        // Get local hostname if not provided
        char hostname_buf[256];
        if (gethostname(hostname_buf, sizeof(hostname_buf)) == 0) {
            _hostname = hostname_buf;
        } else {
            _hostname = "localhost";
            log_error("Failed to get hostname: %s", strerror(errno));
        }
    }
}

host::~host() 
{
    // Auto-cleanup through auto_ref destructors
}

STATUS host::initialize() 
{
    if (_initialized) {
        return STATUS_OK;
    }
    
    STATUS status = discover_devices();
    if (FAILED(status)) {
        log_error("Failed to discover RDMA devices");
        return status;
    }
    
    _initialized = true;
    return STATUS_OK;
}

std::string host::get_hostname() const 
{
    return _hostname;
}

rdma_general_device* host::get_device(const std::string& device_name) 
{
    auto it = _devices.find(device_name);
    if (it != _devices.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<rdma_general_device*> host::get_all_devices() 
{
    std::vector<rdma_general_device*> devices;
    for (auto& pair : _devices) {
        devices.push_back(pair.second.get());
    }
    return devices;
}

size_t host::get_device_count() const 
{
    return _devices.size();
}

void host::print_device_info() const 
{
    log_info("Host: %s", _hostname.c_str());
    log_info("RDMA Devices: %zu", _devices.size());
    
    for (const auto& pair : _devices) {
        log_info("Device: %s", pair.first.c_str());
        pair.second->print_info();
    }
}

STATUS host::discover_devices() 
{
    // Get the list of available RDMA devices from driver
    struct ibv_device** device_list = ibv_get_device_list(nullptr);
    if (!device_list) {
        log_error("Failed to get RDMA device list: %s", strerror(errno));
        return STATUS_ERR;
    }
    
    // Count the number of devices
    int num_devices = 0;
    struct ibv_device* device;
    
    while ((device = device_list[num_devices]) != nullptr) {
        std::string device_name = ibv_get_device_name(device);
        
        // Create the device 
        auto_ref<rdma_general_device> new_device;
        
        // Set the device name
        new_device->set_name(device_name);
        
        // Initialize the device (opens it, etc.)
        STATUS status = new_device->initialize();
        
        if (FAILED(status)) {
            log_error("Failed to initialize device %s", device_name.c_str());
        } else {
            _devices[device_name] = std::move(new_device);
            log_info("Discovered RDMA device: %s", device_name.c_str());
        }
        
        num_devices++;
    }
    
    // Free the device list
    ibv_free_device_list(device_list);
    
    if (_devices.empty()) {
        log_error("No RDMA devices found or initialized");
        return STATUS_ERR;
    }
    
    log_info("Discovered %zu RDMA devices", _devices.size());
    return STATUS_OK;
}