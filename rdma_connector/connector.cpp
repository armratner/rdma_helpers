#include "connector.h"
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/tcp.h>

namespace {
    /**
     * Set socket timeout for both send and receive operations
     */
    bool set_socket_timeout(int sock, int timeout_ms) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        return (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 &&
                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0);
    }
    
    /**
     * Enable TCP keepalive to detect disconnections
     */
    bool enable_keepalive(int sock) {
        int enable = 1;
        int idle = 60;   // Start sending keepalive after 60s of idle
        int interval = 5; // Send keepalive every 5s
        int count = 3;    // 3 failed keepalives = connection failure
        
        return (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) == 0 &&
                setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == 0 &&
                setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) == 0 &&
                setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) == 0);
    }
    
    /**
     * Set socket to non-blocking mode
     */
    bool set_non_blocking(int sock) {
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) return false;
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
    }
    
    /**
     * Wait for socket to become writable (connected) with timeout
     */
    bool wait_connected(int sock, int timeout_ms) {
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLOUT;
        
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret <= 0) return false; // Timeout or error
        
        // Check if connection was successful
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0) return false;
        return error == 0;
    }
}

rdma_connector::rdma_connector() 
    : _config() 
    , _connected(false)
    , _socket_fd(-1)
    , _listen_fd(-1)
{
}

rdma_connector::rdma_connector(const Config& config) 
    : _config(config)
    , _connected(false)
    , _socket_fd(-1)
    , _listen_fd(-1)
{
}

rdma_connector::~rdma_connector() 
{
    close();
}

rdma_connector& rdma_connector::initialize(const std::string& address, uint16_t port)
{
    _config.address = address;
    _config.port = port;
    clear_error();
    return *this;
}

rdma_connector& rdma_connector::set_timeout(std::chrono::milliseconds timeout)
{
    _config.timeout_ms = static_cast<int>(timeout.count());
    clear_error();
    return *this;
}

rdma_connector& rdma_connector::set_nonblocking(bool nonblocking)
{
    _config.nonblocking = nonblocking;
    clear_error();
    return *this;
}

void rdma_connector::set_error(int code, const std::string& message)
{
    _last_error.error_code = code;
    _last_error.message = message;
}

void rdma_connector::clear_error()
{
    _last_error.error_code = 0;
    _last_error.message.clear();
}

bool rdma_connector::has_error() const
{
    return _last_error.error_code != 0;
}

rdma_connector::Error rdma_connector::get_last_error() const
{
    return _last_error;
}

rdma_connector& rdma_connector::listen() 
{
    // Clear previous errors
    clear_error();
    
    // Close any existing sockets
    close();
    
    // Create socket
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd < 0) {
        set_error(errno, std::string("Failed to create listen socket: ") + strerror(errno));
        log_error("Failed to create listen socket: %s", strerror(errno));
        return *this;
    }
    
    // Set socket options
    int enable = 1;
    if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        set_error(errno, std::string("Failed to set SO_REUSEADDR: ") + strerror(errno));
        log_error("Failed to set SO_REUSEADDR: %s", strerror(errno));
        ::close(_listen_fd);
        _listen_fd = -1;
        return *this;
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
        set_error(errno, std::string("Failed to bind to port: ") + strerror(errno));
        log_error("Failed to bind to port %d: %s", _config.port, strerror(errno));
        ::close(_listen_fd);
        _listen_fd = -1;
        return *this;
    }
    
    // Listen
    if (::listen(_listen_fd, 1) < 0) {
        set_error(errno, std::string("Failed to listen: ") + strerror(errno));
        log_error("Failed to listen: %s", strerror(errno));
        ::close(_listen_fd);
        _listen_fd = -1;
        return *this;
    }
    
    log_info("RDMA Connector listening on %s:%d", _config.address.c_str(), _config.port);
    return *this;
}

rdma_connector& rdma_connector::accept() 
{
    // Clear previous errors
    clear_error();
    
    if (_listen_fd < 0) {
        set_error(EINVAL, "Cannot accept: Not listening");
        log_error("Cannot accept: Not listening");
        return *this;
    }
    
    // Set timeout on accept
    if (!set_socket_timeout(_listen_fd, _config.timeout_ms)) {
        log_error("Failed to set accept timeout");
    }
    
    // Accept connection
    struct sockaddr_in client_addr = {};
    socklen_t client_len = sizeof(client_addr);
    
    _socket_fd = ::accept(_listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (_socket_fd < 0) {
        set_error(errno, std::string("Accept failed: ") + strerror(errno));
        log_error("Accept failed: %s", strerror(errno));
        return *this;
    }
    
    // Configure accepted socket
    if (!set_socket_timeout(_socket_fd, _config.timeout_ms)) {
        log_error("Failed to set socket timeout, continuing anyway");
    }
    
    if (!enable_keepalive(_socket_fd)) {
        log_error("Failed to enable keepalive, continuing anyway");
    }
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    log_info("Accepted connection from %s:%d", client_ip, ntohs(client_addr.sin_port));
    
    _connected = true;
    return *this;
}

rdma_connector& rdma_connector::connect(bool as_server) 
{
    // Clear previous errors
    clear_error();
    
    if (as_server) {
        return listen();
    }
    
    // Close any existing connection
    close();
    
    // Create socket
    _socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket_fd < 0) {
        set_error(errno, std::string("Failed to create socket: ") + strerror(errno));
        log_error("Failed to create socket: %s", strerror(errno));
        return *this;
    }
    
    // Set non-blocking for connection with timeout
    if (!set_non_blocking(_socket_fd)) {
        set_error(errno, std::string("Failed to set non-blocking mode: ") + strerror(errno));
        log_error("Failed to set non-blocking mode: %s", strerror(errno));
        ::close(_socket_fd);
        _socket_fd = -1;
        return *this;
    }
    
    // Prepare server address
    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_config.port);
    
    if (inet_pton(AF_INET, _config.address.c_str(), &server_addr.sin_addr) <= 0) {
        set_error(EINVAL, std::string("Invalid address: ") + _config.address);
        log_error("Invalid address: %s", _config.address.c_str());
        ::close(_socket_fd);
        _socket_fd = -1;
        return *this;
    }
    
    // Connect with timeout handling
    int ret = ::connect(_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret < 0 && errno != EINPROGRESS) {
        set_error(errno, std::string("Connect failed: ") + strerror(errno));
        log_error("Connect failed: %s", strerror(errno));
        ::close(_socket_fd);
        _socket_fd = -1;
        return *this;
    }
    
    // Wait for connection to complete or timeout
    if (!wait_connected(_socket_fd, _config.timeout_ms)) {
        set_error(ETIMEDOUT, "Connection timed out or failed");
        log_error("Connection timed out or failed");
        ::close(_socket_fd);
        _socket_fd = -1;
        return *this;
    }
    
    // Restore blocking mode and set options
    if (!_config.nonblocking) {
        int flags = fcntl(_socket_fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(_socket_fd, F_SETFL, flags & ~O_NONBLOCK);
        }
    }
    
    set_socket_timeout(_socket_fd, _config.timeout_ms);
    enable_keepalive(_socket_fd);
    
    log_info("Connected to %s:%d", _config.address.c_str(), _config.port);
    _connected = true;
    return *this;
}

STATUS
rdma_connector::query_local_qp_info(
    const queue_pair& qp,
    const rdma_device& device,
    qp_init_connection_params& local_info)
{
    // Get port attributes
    const ibv_port_attr* port_attr = device.get_port_attr(1);
    if (!port_attr) {
        log_error("Failed to get port attributes");
        return STATUS_ERR;
    }

    // Initialize the connection parameters structure
    local_info.port_num = 1;
    local_info.mtu = port_attr->active_mtu;
    local_info.sl = 0;
    local_info.retry_count = 7;      // Standard retry count
    local_info.rnr_retry = 7;        // Infinite retry
    local_info.min_rnr_to = 12;      // 1.024 ms minimum timeout
    local_info.traffic_class = 0;
    local_info.dscp = 0;
    local_info.ece = false;
    
    // Allocate and initialize an address handle attribute structure
    ibv_ah_attr* ah_attr = aligned_alloc<ibv_ah_attr>(sizeof(ibv_ah_attr));
    if (!ah_attr) {
        log_error("Failed to allocate memory for ah_attr");
        return STATUS_NO_MEM;
    }
    
    memset(ah_attr, 0, sizeof(ibv_ah_attr));
    local_info.remote_ah_attr = ah_attr;
    
    // Set basic AH attributes
    ah_attr->port_num = local_info.port_num;
    ah_attr->sl = local_info.sl;
    
    bool is_roce = (port_attr->link_layer == IBV_LINK_LAYER_ETHERNET);
    
    // Configure based on network type (RoCE vs IB)
    if (is_roce) {
        // For RoCE (RDMA over Converged Ethernet)
        static constexpr uint16_t ROCE_UDP_PORT = 4791; // Standard RoCE port
        ah_attr->is_global = 1;
        ah_attr->dlid = ROCE_UDP_PORT;
        ah_attr->grh.flow_label = 0;
    } else {
        // For InfiniBand
        ah_attr->dlid = port_attr->lid;
        ah_attr->src_path_bits = 0;
    }
    
    // Query and set GID information
    union ibv_gid gid;
    uint8_t gid_index = 0;  // Typically use index 0 for default GID
    
    if (ibv_query_gid(device.get_context(), local_info.port_num, gid_index, &gid) == 0) {
        // For RoCE or when we have a valid GID, use it
        union ibv_gid empty_gid = {};
        if (is_roce || memcmp(&gid, &empty_gid, sizeof(gid)) != 0) {
            ah_attr->is_global = 1;
            ah_attr->grh.dgid = gid;  // Use local GID as destination for now
            ah_attr->grh.sgid_index = gid_index;
            ah_attr->grh.hop_limit = 64;  // Standard Internet hop limit
        }
    }
    
    log_debug("Local QP info prepared: QPN=%d, MTU=%d, Port=%d", 
              qp.get_qpn(), local_info.mtu, local_info.port_num);
    
    return STATUS_OK;
}

STATUS
rdma_connector::exchange_qp_info(
    const qp_init_connection_params& local_info,
    qp_init_connection_params& remote_info)
{
    if (!_connected || _socket_fd < 0) {
        log_error("Cannot exchange QP info: Not connected");
        return STATUS_INVALID_STATE;
    }
    
    // First allocate memory for remote AH attribute
    remote_info.remote_ah_attr = aligned_alloc<ibv_ah_attr>(sizeof(ibv_ah_attr));
    if (!remote_info.remote_ah_attr) {
        log_error("Failed to allocate memory for remote AH attribute");
        return STATUS_NO_MEM;
    }

    // Create buffer for transmitting the QP info
    // We need to serialize the structures without the pointers
    struct qp_exchange_data {
        // Connection parameters
        uint8_t mtu;
        bool ece;
        uint8_t port_num;
        uint8_t retry_count;
        uint8_t rnr_retry;
        uint8_t min_rnr_to;
        uint8_t sl;
        uint8_t dscp;
        uint8_t traffic_class;
        
        // Address handle attributes
        ibv_ah_attr ah_attr;
    } __attribute__((packed));
    
    // Prepare local data for sending
    qp_exchange_data local_data = {};
    local_data.mtu = local_info.mtu;
    local_data.ece = local_info.ece;
    local_data.port_num = local_info.port_num;
    local_data.retry_count = local_info.retry_count;
    local_data.rnr_retry = local_info.rnr_retry;
    local_data.min_rnr_to = local_info.min_rnr_to;
    local_data.sl = local_info.sl;
    local_data.dscp = local_info.dscp;
    local_data.traffic_class = local_info.traffic_class;
    
    // Copy AH attributes
    memcpy(&local_data.ah_attr, local_info.remote_ah_attr, sizeof(ibv_ah_attr));
    
    // Send local data
    ssize_t sent = send(_socket_fd, &local_data, sizeof(local_data), 0);
    if (sent != sizeof(local_data)) {
        log_error("Failed to send QP info: %s", strerror(errno));
        free(remote_info.remote_ah_attr);
        remote_info.remote_ah_attr = nullptr;
        return STATUS_ERR;
    }
    
    // Receive remote data
    qp_exchange_data remote_data = {};
    ssize_t received = recv(_socket_fd, &remote_data, sizeof(remote_data), MSG_WAITALL);
    if (received != sizeof(remote_data)) {
        log_error("Failed to receive QP info: %s", strerror(errno));
        free(remote_info.remote_ah_attr);
        remote_info.remote_ah_attr = nullptr;
        return STATUS_ERR;
    }
    
    // Copy received data to remote_info
    remote_info.mtu = remote_data.mtu;
    remote_info.ece = remote_data.ece;
    remote_info.port_num = remote_data.port_num;
    remote_info.retry_count = remote_data.retry_count;
    remote_info.rnr_retry = remote_data.rnr_retry;
    remote_info.min_rnr_to = remote_data.min_rnr_to;
    remote_info.sl = remote_data.sl;
    remote_info.dscp = remote_data.dscp;
    remote_info.traffic_class = remote_data.traffic_class;
    
    // Copy AH attributes
    memcpy(remote_info.remote_ah_attr, &remote_data.ah_attr, sizeof(ibv_ah_attr));
    
    log_debug("QP info exchanged successfully");
    return STATUS_OK;
}

qp_init_connection_params rdma_connector::get_default_qp_params(const rdma_device& device)
{
    qp_init_connection_params params = {};
    
    // Get port attributes
    const ibv_port_attr* port_attr = device.get_port_attr(1);
    if (port_attr) {
        params.port_num = 1;
        params.mtu = port_attr->active_mtu;
        params.sl = 0;
        params.retry_count = 7;      // Standard retry count
        params.rnr_retry = 7;        // Infinite retry
        params.min_rnr_to = 12;      // 1.024 ms minimum timeout
        params.traffic_class = 0;
        params.dscp = 0;
        params.ece = false;
    }
    
    return params;
}

rdma_connector& rdma_connector::set_qp_params(const qp_init_connection_params& params)
{
    _params = params;
    clear_error();
    return *this;
}

rdma_connector& rdma_connector::prepare_qp_params(const rdma_device& device)
{
    clear_error();
    
    if (!_connected) {
        set_error(EINVAL, "Cannot prepare QP params: Not connected");
        log_error("Cannot prepare QP params: Not connected");
        return *this;
    }
    
    // Get default parameters if not already set
    if (_params.mtu == 0) {
        _params = get_default_qp_params(device);
    }
    
    return *this;
}

rdma_connector& rdma_connector::exchange_qp_info()
{
    clear_error();
    
    if (!_connected) {
        set_error(EINVAL, "Cannot exchange QP info: Not connected");
        log_error("Cannot exchange QP info: Not connected");
        return *this;
    }
    
    // Prepare local QP info if the remote_ah_attr is not set
    if (!_params.remote_ah_attr) {
        _params.remote_ah_attr = aligned_alloc<ibv_ah_attr>(1);
        if (!_params.remote_ah_attr) {
            set_error(ENOMEM, "Failed to allocate AH attributes");
            log_error("Failed to allocate AH attributes");
            return *this;
        }
        memset(_params.remote_ah_attr, 0, sizeof(ibv_ah_attr));
    }
    
    // Exchange QP info with peer
    STATUS status = exchange_qp_info(_params, _remote_params);
    if (FAILED(status)) {
        set_error(status, "Failed to exchange QP info");
        log_error("Failed to exchange QP info");
        
        // Clean up
        if (_params.remote_ah_attr) {
            free(_params.remote_ah_attr);
            _params.remote_ah_attr = nullptr;
        }
        
        if (_remote_params.remote_ah_attr) {
            free(_remote_params.remote_ah_attr);
            _remote_params.remote_ah_attr = nullptr;
        }
    }
    
    return *this;
}

rdma_connector& rdma_connector::setup_remote_qp(queue_pair& qp, protection_domain& pd)
{
    clear_error();
    
    if (!_connected) {
        set_error(EINVAL, "Cannot setup QP connection: Not connected");
        log_error("Cannot setup QP connection: Not connected");
        return *this;
    }
    
    if (!_remote_params.remote_ah_attr) {
        set_error(EINVAL, "Cannot setup QP: No remote QP information available");
        log_error("Cannot setup QP: No remote QP information available");
        return *this;
    }
    
    // Set the protection domain for using with QP transitions
    _remote_params.pd = pd.get();
    
    // Transition QP to Ready-To-Receive and then to Ready-To-Send
    STATUS status = qp.reset_to_init(_remote_params);
    if (FAILED(status)) {
        set_error(status, "Failed to transition QP to INIT state");
        log_error("Failed to transition QP to INIT state");
        free(_remote_params.remote_ah_attr);
        _remote_params.remote_ah_attr = nullptr;
        return *this;
    }
    
    status = qp.init_to_rtr(_remote_params);
    if (FAILED(status)) {
        set_error(status, "Failed to transition QP to RTR state");
        log_error("Failed to transition QP to RTR state");
        free(_remote_params.remote_ah_attr);
        _remote_params.remote_ah_attr = nullptr;
        return *this;
    }
    
    status = qp.rtr_to_rts(_remote_params);
    if (FAILED(status)) {
        set_error(status, "Failed to transition QP to RTS state");
        log_error("Failed to transition QP to RTS state");
        free(_remote_params.remote_ah_attr);
        _remote_params.remote_ah_attr = nullptr;
        return *this;
    }
    
    // Clean up
    free(_remote_params.remote_ah_attr);
    _remote_params.remote_ah_attr = nullptr;
    
    log_info("RDMA QP connection established successfully");
    return *this;
}

rdma_connector::Error rdma_connector::establish_qp_connection(
    queue_pair& qp,
    rdma_device& device,
    protection_domain& pd,
    bool as_server)
{
    // Clear any previous errors
    clear_error();
    
    // Connect as server or client
    connect(as_server);
    if (has_error()) {
        return get_last_error();
    }
    
    // For server, we need to accept a connection
    if (as_server) {
        accept();
        if (has_error()) {
            return get_last_error();
        }
    }
    
    // Prepare QP parameters
    prepare_qp_params(device);
    if (has_error()) {
        return get_last_error();
    }
    
    // Exchange QP info with the peer
    exchange_qp_info();
    if (has_error()) {
        return get_last_error();
    }
    
    // Setup remote QP
    setup_remote_qp(qp, pd);
    if (has_error()) {
        return get_last_error();
    }
    
    return _last_error;
}

// Add this traditional API method to the class for backward compatibility
STATUS rdma_connector::setup_connection(
    queue_pair& qp,
    protection_domain& pd,
    rdma_device& device)
{
    if (!_connected) {
        log_error("Cannot setup QP connection: Not connected");
        return STATUS_INVALID_STATE;
    }

    // Prepare local QP info
    qp_init_connection_params local_info = {};
    STATUS status = query_local_qp_info(qp, device, local_info);
    if (FAILED(status)) {
        log_error("Failed to query local QP info");
        return status;
    }

    // Exchange QP info with peer
    qp_init_connection_params remote_info = {};
    status = exchange_qp_info(local_info, remote_info);
    
    // Always clean up local_info
    if (local_info.remote_ah_attr) {
        free(local_info.remote_ah_attr);
        local_info.remote_ah_attr = nullptr;
    }
    
    if (FAILED(status)) {
        log_error("Failed to exchange QP info");
        if (remote_info.remote_ah_attr) {
            free(remote_info.remote_ah_attr);
        }
        return status;
    }

    // Set the protection domain for using with QP transitions
    remote_info.pd = pd.get();

    // Transition QP to Ready-To-Receive and then to Ready-To-Send
    status = qp.reset_to_init(remote_info);
    if (FAILED(status)) {
        log_error("Failed to transition QP to INIT state");
        free(remote_info.remote_ah_attr);
        return status;
    }

    status = qp.init_to_rtr(remote_info);
    if (FAILED(status)) {
        log_error("Failed to transition QP to RTR state");
        free(remote_info.remote_ah_attr);
        return status;
    }

    status = qp.rtr_to_rts(remote_info);
    if (FAILED(status)) {
        log_error("Failed to transition QP to RTS state");
        free(remote_info.remote_ah_attr);
        return status;
    }

    // Clean up
    free(remote_info.remote_ah_attr);
    
    log_info("RDMA QP connection established successfully");
    return STATUS_OK;
}

rdma_connector& rdma_connector::close() 
{
    if (_socket_fd >= 0) {
        ::close(_socket_fd);
        _socket_fd = -1;
    }
    
    if (_listen_fd >= 0) {
        ::close(_listen_fd);
        _listen_fd = -1;
    }
    
    _connected = false;
    return *this;
}

bool rdma_connector::is_connected() const 
{
    return _connected && _socket_fd >= 0;
}