# General RDMA Objects

High-level abstractions for RDMA resources that provide a more convenient object-oriented interface for managing RDMA devices and their resources.

## Key Features

- Host abstraction for discovering and managing multiple RDMA devices
- Device abstraction for managing resources associated with a specific HCA
- Named resources (QPs, CQs, PDs) for easier management
- Automatic resource cleanup through smart pointers

## Components

- **host**: Represents a physical host with one or more RDMA devices
- **rdma_device**: Represents an RDMA device (HCA) with its resources

## Usage Example

```cpp
// Create a host and initialize it (discovers devices)
host local_host;
local_host.initialize();

// Get a specific device by name
rdma_device* device = local_host.get_device("mlx5_0");
if (device) {
    // Create a queue pair with default parameters
    queue_pair* qp = device->create_queue_pair("my_qp");
    
    // Get a protection domain
    protection_domain* pd = device->get_protection_domain();
    
    // Create custom completion queue
    cq_creation_params cq_params;
    // ... set CQ parameters ...
    completion_queue* cq = device->create_completion_queue("my_cq", &cq_params);
    
    // Print device information
    device->print_info();
}
```

## Building

To build the general_objects library and test:

```
mkdir build
cd build
cmake ..
make
```

To run the test:

```
./general_objects_test
```