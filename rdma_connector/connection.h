#pragma once

#include "../rdma_objects/rdma_objects.h"
#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>

/**
 * @brief Represents a single RDMA connection
 *
 * Handles a single connection between RDMA endpoints and manages
 * its lifecycle and communication.
 */
class rdma_connection {
public:
    // Connection configuration
    struct Config {
        std::string address;  // Remote address or bind address
        uint16_t port;        // TCP port for connection exchange
        int timeout_ms;       // Connection timeout in milliseconds
        bool nonblocking;     // Use non-blocking sockets
        
        // Default constructor with initialization
        Config() 
            : address("0.0.0.0")
            , port(18515)
            , timeout_ms(5000)
            , nonblocking(false)
        {}
    };

    // Error information structure
    struct Error {
        int error_code = 0;               // System error code (errno)
        std::string message;              // Human-readable error message
        
        // Convert to bool to check if error occurred
        operator bool() const { return error_code != 0; }
    };

    // Connection identifiers
    using connection_id_t = uint64_t;
    
    /**
     * @brief Create a connection with specific configuration
     * @param id Unique connection identifier
     * @param config Connection configuration
     */
    explicit rdma_connection(connection_id_t id, const Config& config = Config());

    /**
     * @brief Create a connection from accepted socket
     * @param id Unique connection identifier
     * @param socket_fd Accepted socket file descriptor
     * @param remote_addr Remote address information
     */
    rdma_connection(connection_id_t id, int socket_fd, const struct sockaddr_in& remote_addr);

    /**
     * @brief Destructor - closes socket if still open
     */
    ~rdma_connection();

    /**
     * @brief Connect to remote peer
     * @return true on success, false on failure
     */
    bool connect();

    /**
     * @brief Get unique connection identifier
     * @return Connection ID
     */
    connection_id_t get_id() const { return _id; }

    /**
     * @brief Get connection configuration
     * @return Connection configuration
     */
    const Config& get_config() const { return _config; }

    /**
     * @brief Get remote IP address
     * @return Remote IP address string
     */
    std::string get_remote_ip() const { return _remote_ip; }

    /**
     * @brief Get remote port
     * @return Remote port number
     */
    uint16_t get_remote_port() const { return _remote_port; }

    /**
     * @brief Set connection timeout
     * @param timeout Timeout duration
     * @return Reference to this connection for method chaining
     */
    rdma_connection& set_timeout(std::chrono::milliseconds timeout);

    /**
     * @brief Set non-blocking mode
     * @param nonblocking Whether to use non-blocking sockets
     * @return Reference to this connection for method chaining
     */
    rdma_connection& set_nonblocking(bool nonblocking);

    /**
     * @brief Get default QP parameters for a device
     * @param device RDMA device
     * @return QP connection parameters with default values
     */
    qp_init_connection_params get_default_qp_params(const rdma_device& device);

    /**
     * @brief Set custom QP parameters
     * @param params QP connection parameters
     * @return Reference to this connection for method chaining
     */
    rdma_connection& set_qp_params(const qp_init_connection_params& params);

    /**
     * @brief Prepare QP parameters based on device capabilities
     * @param device RDMA device
     * @return Reference to this connection for method chaining
     */
    rdma_connection& prepare_qp_params(const rdma_device& device);

    /**
     * @brief Exchange QP information with peer
     * @return Reference to this connection for method chaining
     */
    rdma_connection& exchange_qp_info();

    /**
     * @brief Configure and connect a queue pair
     * @param qp Queue pair to configure and connect
     * @param pd Protection domain associated with the queue pair
     * @return Reference to this connection for method chaining
     */
    rdma_connection& setup_remote_qp(queue_pair& qp, protection_domain& pd);

    /**
     * @brief One-shot QP connection establishment
     * @param qp Queue pair to connect
     * @param device RDMA device
     * @param pd Protection domain
     * @return Error information
     */
    Error establish_qp_connection(
        queue_pair& qp,
        rdma_device& device,
        protection_domain& pd
    );

    /**
     * @brief Setup the connection for a queue pair (legacy API)
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
     * @brief Close the connection
     */
    void close();

    /**
     * @brief Check if connection is established
     * @return true if connected, false otherwise
     */
    bool is_connected() const;

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

    // Data send/recv helpers
    bool send_data(const void* data, size_t size);
    bool recv_data(void* data, size_t size);

private:
    // Connection identification
    connection_id_t _id;
    std::string _remote_ip;
    uint16_t _remote_port = 0;
    
    // Connection status
    std::atomic<bool> _connected{false};
    int _socket_fd = -1;
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