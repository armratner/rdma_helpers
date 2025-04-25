#pragma once

#include "connection.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>

/**
 * @brief RDMA Connection Manager
 *
 * Manages multiple RDMA connections and handles the exchange of connection parameters
 * between RDMA endpoints. Can operate in both client and server modes.
 */
class rdma_connector {
public:
    // Simple connection configuration
    struct Config {
        std::string address;  // Server address (for client) or bind address (for server)
        uint16_t port;        // TCP port for connection exchange
        int timeout_ms;       // Connection timeout in milliseconds
        bool nonblocking;     // Use non-blocking sockets
        int max_connections;  // Maximum number of connections to handle
        int listen_backlog;   // Listen backlog for server mode
        
        // Default constructor with initialization
        Config()
            : address("0.0.0.0")
            , port(18515)
            , timeout_ms(5000)
            , nonblocking(false)
            , max_connections(16)
            , listen_backlog(10)
        {}
    };

    // Connection callback function types
    using connection_cb_t = std::function<void(rdma_connection::connection_id_t, const std::string&, uint16_t)>;
    using disconnection_cb_t = std::function<void(rdma_connection::connection_id_t)>;
    
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
     * @brief Destructor - closes all connections and stops server
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
     * @brief Set maximum number of connections
     * @param max_connections Maximum number of connections
     * @return Reference to this connector for method chaining
     */
    rdma_connector& set_max_connections(int max_connections);
    
    /**
     * @brief Set connection established callback
     * @param callback Function to call when a new connection is established
     * @return Reference to this connector for method chaining
     */
    rdma_connector& on_connection(connection_cb_t callback);
    
    /**
     * @brief Set connection closed callback
     * @param callback Function to call when a connection is closed
     * @return Reference to this connector for method chaining
     */
    rdma_connector& on_disconnection(disconnection_cb_t callback);

    /**
     * @brief Start server mode - listen for incoming connections
     * @return true if server started successfully, false otherwise
     */
    bool start_server();

    /**
     * @brief Stop server mode
     */
    void stop_server();

    /**
     * @brief Connect to a remote server
     * @param address Remote server address
     * @param port Remote server port
     * @return Connection ID on success, 0 on failure
     */
    rdma_connection::connection_id_t connect_to_server(const std::string& address, uint16_t port);

    /**
     * @brief Get a connection by ID
     * @param id Connection ID
     * @return Pointer to the connection or nullptr if not found
     */
    rdma_connection* get_connection(rdma_connection::connection_id_t id);

    /**
     * @brief Get all active connections
     * @return Vector of connection IDs
     */
    std::vector<rdma_connection::connection_id_t> get_connections();
    
    /**
     * @brief Close a specific connection
     * @param id Connection ID
     * @return true if connection was found and closed, false otherwise
     */
    bool close_connection(rdma_connection::connection_id_t id);
    
    /**
     * @brief Close all connections
     */
    void close_all_connections();

    /**
     * @brief Get default QP parameters for a device
     * @param device RDMA device
     * @return QP connection parameters with default values
     */
    qp_init_connection_params get_default_qp_params(const rdma_device& device);

    /**
     * @brief Configure and connect a queue pair for a specific connection
     * @param id Connection ID
     * @param qp Queue pair to configure and connect
     * @param device RDMA device
     * @param pd Protection domain
     * @return STATUS_OK on success, error code on failure
     */
    STATUS setup_connection(
        rdma_connection::connection_id_t id,
        queue_pair& qp,
        rdma_device& device,
        protection_domain& pd
    );
    
    /**
     * @brief Check if server is running
     * @return true if server is running, false otherwise
     */
    bool is_server_running() const;
    
    /**
     * @brief Get number of active connections
     * @return Number of connections
     */
    size_t get_connection_count() const;

    // Data send/recv helpers
    bool send_data(rdma_connection::connection_id_t id, const void* data, size_t size);
    bool recv_data(rdma_connection::connection_id_t id, void* data, size_t size);

private:
    // Configuration and state
    Config _config;
    std::atomic<bool> _server_running{false};
    std::atomic<rdma_connection::connection_id_t> _next_connection_id{1};
    int _listen_fd = -1;
    
    // Connection management
    mutable std::mutex _connections_mutex;
    std::unordered_map<rdma_connection::connection_id_t, std::unique_ptr<rdma_connection>> _connections;
    
    // Server thread
    std::thread _accept_thread;
    std::mutex _server_mutex;
    std::condition_variable _server_cv;
    
    // Callbacks
    connection_cb_t _connection_callback;
    disconnection_cb_t _disconnection_callback;
    
    // Accept thread function
    void accept_connections();
    
    // Generate next connection ID
    rdma_connection::connection_id_t get_next_connection_id();
};