# RDMA Connector

A modern, efficient connector for RDMA applications with improved error handling and fluent API.

## Key Features

- Fluent interface for chaining operations
- Improved error handling with detailed error messages
- Timeout support for connection establishment
- Socket options for better performance (TCP_NODELAY)
- RAII pattern for automatic resource cleanup
- Simplified one-shot connection setup

## Usage Examples

### Using the Initialization Function


// Create and initialize in one chain
rdma_connector connector;
connector.initialize("192.168.1.100", 18515)
         .set_timeout(std::chrono::seconds(5))
         .set_nonblocking(false);

// Get default QP parameters
auto qp_params = connector.get_default_qp_params(device);
qp_params.mtu = IBV_MTU_2048;
qp_params.retry_count = 3;

// Complete the connection in one chain
connector.set_qp_params(qp_params)
         .connect(false)  // Connect as client
         .prepare_qp_params(device)
         .exchange_qp_info()
         .setup_remote_qp(qp, pd);

if (connector.has_error()) {
    std::cerr << "Connection failed: " << connector.get_last_error().message << std::endl;
}

// Create a server that listens on all interfaces
rdma_connector server;
auto error = server.initialize("0.0.0.0", 9999)  // Listen on all interfaces
                  .connect(true)  // true for server mode
                  .prepare_qp_params(device)
                  .exchange_qp_info()
                  .setup_remote_qp(qp, pd)
                  .get_last_error();

if (error) {
    std::cerr << "Server failed: " << error.message << std::endl;
    return 1;
}

rdma_connector connector;
connector.initialize("192.168.1.100", 18515);

// One-shot connection with error handling
auto error = connector.establish_qp_connection(qp, device, pd, false);
        
if (error) {
    if (error.error_code == ETIMEDOUT) {
        std::cerr << "Connection timed out. Check if server is running." << std::endl;
    } else if (error.error_code == ECONNREFUSED) {
        std::cerr << "Connection refused. Check port and IP address." << std::endl;
    } else {
        std::cerr << "Connection failed: " << error.message << std::endl;
    }
    return 1;
}