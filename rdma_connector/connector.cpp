#include "connector.h"
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <array>

// Thread-safe error string helper
static std::string safe_strerror(int errnum) {
    std::array<char, 128> buf{};
#if ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE) || defined(__APPLE__)
    strerror_r(errnum, buf.data(), buf.size());
    return buf.data();
#else
    // GNU-specific strerror_r returns char*
    return std::string(strerror_r(errnum, buf.data(), buf.size()));
#endif
}

rdma_connector::rdma_connector()
    : _config()
    , _server_running(false)
    , _next_connection_id(1)
    , _listen_fd(-1)
{
}

rdma_connector::rdma_connector(const Config& config)
    : _config(config)
    , _server_running(false)
    , _next_connection_id(1)
    , _listen_fd(-1)
{
}

rdma_connector::~rdma_connector()
{
    // Stop server and close all connections
    stop_server();
    close_all_connections();
}

rdma_connector& rdma_connector::initialize(const std::string& address, uint16_t port)
{
    _config.address = address;
    _config.port = port;
    return *this;
}

rdma_connector& rdma_connector::set_timeout(std::chrono::milliseconds timeout)
{
    _config.timeout_ms = static_cast<int>(timeout.count());
    return *this;
}

rdma_connector& rdma_connector::set_max_connections(int max_connections)
{
    _config.max_connections = max_connections;
    return *this;
}

rdma_connector& rdma_connector::on_connection(connection_cb_t callback)
{
    _connection_callback = std::move(callback);
    return *this;
}

rdma_connector& rdma_connector::on_disconnection(disconnection_cb_t callback)
{
    _disconnection_callback = std::move(callback);
    return *this;
}

bool rdma_connector::start_server()
{
    // If server is already running, do nothing
    if (_server_running) {
        log_error("Server is already running");
        return false;
    }

    // Create socket
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd < 0) {
        log_error("Failed to create listen socket: %s", safe_strerror(errno).c_str());
        return false;
    }

    // Set socket options
    int enable = 1;
    if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        log_error("Failed to set SO_REUSEADDR: %s", safe_strerror(errno).c_str());
        ::close(_listen_fd);
        _listen_fd = -1;
        return false;
    }

    // Bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_config.port);

    // Convert IP address string to network format
    if (inet_pton(AF_INET, _config.address.c_str(), &addr.sin_addr) <= 0) {
        // If conversion fails or address is empty, bind to INADDR_ANY
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (bind(_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_error("Failed to bind to port %d: %s", _config.port, safe_strerror(errno).c_str());
        ::close(_listen_fd);
        _listen_fd = -1;
        return false;
    }

    // Listen
    if (::listen(_listen_fd, _config.listen_backlog) < 0) {
        log_error("Failed to listen: %s", safe_strerror(errno).c_str());
        ::close(_listen_fd);
        _listen_fd = -1;
        return false;
    }

    log_info("RDMA Connection Manager listening on %s:%d", _config.address.c_str(), _config.port);

    // Start accept thread
    _server_running = true;
    _accept_thread = std::thread(&rdma_connector::accept_connections, this);

    return true;
}

void rdma_connector::stop_server()
{
    if (!_server_running) {
        return;
    }

    // Signal the accept thread to exit
    {
        std::unique_lock<std::mutex> lock(_server_mutex);
        _server_running = false;
    }
    _server_cv.notify_one();

    // Close listen socket to unblock accept() call
    if (_listen_fd >= 0) {
        ::close(_listen_fd);
        _listen_fd = -1;
    }

    // Wait for accept thread to finish
    if (_accept_thread.joinable()) {
        _accept_thread.join();
    }

    log_info("RDMA Connection Manager server stopped");
}

void rdma_connector::accept_connections()
{
    while (_server_running) {
        // Setup file descriptor set for accept with timeout
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(_listen_fd, &readfds);

        struct timeval tv;
        tv.tv_sec = 1;  // 1 second timeout to check if server is still running
        tv.tv_usec = 0;

        // Wait for incoming connections with timeout
        int ret = select(_listen_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) {
                // Interrupted, just try again
                continue;
            }
            log_error("Select failed: %s", safe_strerror(errno).c_str());
            break;
        } else if (ret == 0) {
            // Timeout - check if server is still running
            std::unique_lock<std::mutex> lock(_server_mutex);
            if (!_server_running) {
                break;
            }
            continue;
        }

        if (!FD_ISSET(_listen_fd, &readfds)) {
            continue;
        }

        // Check if we have reached the max connections limit
        {
            std::unique_lock<std::mutex> lock(_connections_mutex);
            if (_connections.size() >= static_cast<size_t>(_config.max_connections)) {
                log_error("Maximum connections limit reached (%d)", _config.max_connections);
                std::unique_lock<std::mutex> server_lock(_server_mutex);
                if (!_server_cv.wait_for(server_lock, std::chrono::seconds(1), [this] { 
                    return !_server_running || _connections.size() < static_cast<size_t>(_config.max_connections); 
                })) {
                    continue;
                }
            }
        }

        // Accept connection
        struct sockaddr_in client_addr = {};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(_listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                // Non-fatal errors, continue
                continue;
            }
            log_error("Accept failed: %s", safe_strerror(errno).c_str());
            break;
        }

        // Get a new connection ID
        rdma_connection::connection_id_t conn_id = get_next_connection_id();

        // Create a new connection object
        auto connection = std::make_unique<rdma_connection>(conn_id, client_fd, client_addr);

        // Get client info for callback
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        uint16_t client_port = ntohs(client_addr.sin_port);

        // Add connection to the map
        {
            std::unique_lock<std::mutex> lock(_connections_mutex);
            _connections[conn_id] = std::move(connection);
        }

        // Invoke connection callback if set
        if (_connection_callback) {
            _connection_callback(conn_id, client_ip, client_port);
        }

        log_info("New connection accepted: %s:%d (ID: %lu)", client_ip, client_port, conn_id);
    }

    log_info("Accept thread exiting");
}

rdma_connection::connection_id_t rdma_connector::connect_to_server(const std::string& address, uint16_t port)
{
    // Get a new connection ID
    rdma_connection::connection_id_t conn_id = get_next_connection_id();

    // Create connection config
    rdma_connection::Config config;
    config.address = address;
    config.port = port;
    config.timeout_ms = _config.timeout_ms;
    config.nonblocking = _config.nonblocking;

    // Create a new connection object
    auto connection = std::make_unique<rdma_connection>(conn_id, config);

    // Try to connect
    if (!connection->connect()) {
        log_error("Failed to connect to %s:%d: %s", 
                 address.c_str(), port, connection->get_last_error().message.c_str());
        return 0; // Return 0 to indicate failure
    }

    // Add connection to the map
    {
        std::unique_lock<std::mutex> lock(_connections_mutex);
        _connections[conn_id] = std::move(connection);
    }

    // Invoke connection callback if set
    if (_connection_callback) {
        _connection_callback(conn_id, address, port);
    }

    log_info("Connected to server: %s:%d (ID: %lu)", address.c_str(), port, conn_id);
    return conn_id;
}

rdma_connection* rdma_connector::get_connection(rdma_connection::connection_id_t id)
{
    std::unique_lock<std::mutex> lock(_connections_mutex);
    auto it = _connections.find(id);
    if (it == _connections.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<rdma_connection::connection_id_t> rdma_connector::get_connections()
{
    std::vector<rdma_connection::connection_id_t> result;
    std::unique_lock<std::mutex> lock(_connections_mutex);
    result.reserve(_connections.size());
    for (const auto& pair : _connections) {
        result.push_back(pair.first);
    }
    return result;
}

bool rdma_connector::close_connection(rdma_connection::connection_id_t id)
{
    std::unique_lock<std::mutex> lock(_connections_mutex);
    auto it = _connections.find(id);
    if (it == _connections.end()) {
        return false;
    }

    // Close the connection
    it->second->close();

    // Remove it from the map
    _connections.erase(it);

    // Signal that a connection slot is available
    _server_cv.notify_one();

    // Invoke disconnection callback if set
    if (_disconnection_callback) {
        _disconnection_callback(id);
    }

    log_info("Connection closed (ID: %lu)", id);
    return true;
}

void rdma_connector::close_all_connections()
{
    std::vector<rdma_connection::connection_id_t> conn_ids;
    {
        std::unique_lock<std::mutex> lock(_connections_mutex);
        conn_ids.reserve(_connections.size());
        for (const auto& pair : _connections) {
            conn_ids.push_back(pair.first);
        }
    }

    for (auto id : conn_ids) {
        close_connection(id);
    }

    log_info("All connections closed");
}

qp_init_connection_params rdma_connector::get_default_qp_params(const rdma_device& device)
{
    // Delegate to connection class
    rdma_connection::Config config;
    rdma_connection temp(0, config);
    return temp.get_default_qp_params(device);
}

STATUS rdma_connector::setup_connection(
    rdma_connection::connection_id_t id,
    queue_pair& qp,
    rdma_device& device,
    protection_domain& pd)
{
    // Get the connection
    rdma_connection* conn = get_connection(id);
    if (!conn) {
        log_error("Connection not found (ID: %lu)", id);
        return STATUS_INVALID_PARAM;
    }

    // Setup the connection
    return conn->setup_connection(qp, pd, device);
}

bool rdma_connector::is_server_running() const
{
    return _server_running;
}

size_t rdma_connector::get_connection_count() const
{
    std::unique_lock<std::mutex> lock(_connections_mutex);
    return _connections.size();
}

rdma_connection::connection_id_t rdma_connector::get_next_connection_id()
{
    return _next_connection_id++;
}

bool rdma_connector::send_data(rdma_connection::connection_id_t id, const void* data, size_t size) {
    rdma_connection* conn = get_connection(id);
    if (!conn) return false;
    return conn->send_data(data, size);
}

bool rdma_connector::recv_data(rdma_connection::connection_id_t id, void* data, size_t size) {
    rdma_connection* conn = get_connection(id);
    if (!conn) return false;
    return conn->recv_data(data, size);
}