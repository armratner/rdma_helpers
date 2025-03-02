#pragma once

#include "../rdma_objects/rdma_objects.h"
#include <string>
#include <memory>
#include <functional>
#include <chrono>

/**
 * @brief Simplified RDMA connection manager
 *
 * Handles the exchange of connection parameters between RDMA endpoints
 * and manages connection establishment over TCP/IP sockets.
 */
class rdma_connector {
public:
    // Simple connection configuration
    struct Config {
        std::string address = "0.0.0.0";  // Server address (for client) or bind address (for server)
        uint16_t port = 18515;            // TCP port for connection exchange
        int timeout_ms = 5000;            // Connection timeout in milliseconds
        bool nonblocking = false;         // Use non-blocking sockets
    };

    // Error information structure
    struct Error {
        int error_code = 0;               // System error code (errno)
        std::string message;              // Human-readable error message
        
        // Convert to bool to check if error occurred
        operator bool() const { return error_code != 0; }
    };

    /**
     * @brief Create an RDMA connector with default configuration
     */
    rdma_connector();

    /**
     * @brief Create an RDMA connector with specific configuration
     * @param config Connection configuration
     */
    explicit rdma_connector(const Config& config);

    /**
     * @brief Destructor - closes socket if still open
     */
    ~rdma_connector();

    /**
     * @brief Initialize connector with address and port
     * @param address IP address to connect to or bind to
     * @param port Port number
     * @return Reference to this connector for method chaining
     */
    rdma_connector& initialize(const std::string& address, uint16_t port);

    /**
     * @brief Set connection timeout
     * @param timeout Timeout duration
     * @return Reference to this connector for method chaining
     */
    rdma_connector& set_timeout(std::chrono::milliseconds timeout);

    /**
     * @brief Set non-blocking mode
     * @param nonblocking Whether to use non-blocking sockets
     * @return Reference to this connector for method chaining
     */
    rdma_connector& set_nonblocking(bool nonblocking);

    /**
     * @brief Listen for incoming connections as a server
     * @return Reference to this connector for method chaining
     */
    rdma_connector& listen();

    /**
     * @brief Accept a client connection (blocking)
     * @return Reference to this connector for method chaining
     */
    rdma_connector& accept();

    /**
     * @brief Connect to a server as a client or start listening as a server
     * @param as_server True to act as server, false to act as client
     * @return Reference to this connector for method chaining
     */
    rdma_connector& connect(bool as_server = false);

    /**
     * @brief Get default QP parameters for a device
     * @param device RDMA device
     * @return QP connection parameters with default values
     */
    qp_init_connection_params get_default_qp_params(const rdma_device& device);

    /**
     * @brief Set custom QP parameters
     * @param params QP connection parameters
     * @return Reference to this connector for method chaining
     */
    rdma_connector& set_qp_params(const qp_init_connection_params& params);

    /**
     * @brief Prepare QP parameters based on device capabilities
     * @param device RDMA device
     * @return Reference to this connector for method chaining
     */
    rdma_connector& prepare_qp_params(const rdma_device& device);

    /**
     * @brief Exchange QP information with peer
     * @return Reference to this connector for method chaining
     */
    rdma_connector& exchange_qp_info();

    /**
     * @brief Configure and connect a queue pair
     * @param qp Queue pair to configure and connect
     * @param pd Protection domain associated with the queue pair
     * @return Reference to this connector for method chaining
     */
    rdma_connector& setup_remote_qp(queue_pair& qp, protection_domain& pd);

    /**
     * @brief One-shot QP connection establishment
     * @param qp Queue pair to connect
     * @param device RDMA device
     * @param pd Protection domain
     * @param as_server True to act as server, false to act as client
     * @return Error information
     */
    Error establish_qp_connection(
        queue_pair& qp,
        rdma_device& device,
        protection_domain& pd,
        bool as_server = false
    );

    /**
     * @brief Close the connection
     * @return Reference to this connector for method chaining
     */
    rdma_connector& close();

    /**
     * @brief Check if connection is established
     * @return true if connected, false otherwise
     */
    bool is_connected() const;
    
    /**
     * @brief Legacy API - Configure and connect a queue pair
     * @param qp Queue pair to configure and connect
     * @param pd Protection domain associated with the queue pair
     * @param device RDMA device associated with the queue pair
     * @return STATUS_OK on success, error code on failure
     */
    STATUS setup_connection(
        queue_pair& qp,
        protection_domain& pd,
        rdma_device& device
    );

    /**
     * @brief Check if an error has occurred
     * @return true if an error occurred, false otherwise
     */
    bool has_error() const;

    /**
     * @brief Get the last error
     * @return Error information
     */
    Error get_last_error() const;

private:
    // Connection status
    bool _connected = false;
    int _socket_fd = -1;
    int _listen_fd = -1;
    Config _config;
    Error _last_error;
    qp_init_connection_params _params;        // Current QP parameters
    qp_init_connection_params _remote_params; // Remote QP parameters

    // Set error information
    void set_error(int code, const std::string& message);

    // Clear any previous error
    void clear_error();

    // Helper methods
    STATUS exchange_qp_info(
        const qp_init_connection_params& local_info,
        qp_init_connection_params& remote_info
    );

    STATUS query_local_qp_info(
        const queue_pair& qp,
        const rdma_device& device,
        qp_init_connection_params& local_info
    );
};