#include "cm.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

//----------------------
// RDMAConnector Implementation
//----------------------
rdma_connector::rdma_connector()
    : cm_id_(nullptr),
      channel_(nullptr),
      connected_(false)
{
}

rdma_connector::~rdma_connector()
{
    disconnect();
}

bool rdma_connector::connect(const std::string& _address, int _port)
{
    channel_ = rdma_create_event_channel();
    if (!channel_)
        return false;

    if (rdma_create_id(channel_, &cm_id_, nullptr, RDMA_PS_TCP)) {
        rdma_destroy_event_channel(channel_);
        channel_ = nullptr;
        return false;
    }

    struct addrinfo hints;
    struct addrinfo* result = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(_address.c_str(), std::to_string(_port).c_str(), &hints, &result) != 0) {
        disconnect();
        return false;
    }

    bool success = true;
    // For brevity, assume using the first result…
    if (rdma_resolve_addr(cm_id_, nullptr, result->ai_addr, 2000)) {
        success = false;
    }
    freeaddrinfo(result);
    if (!success) {
        disconnect();
        return false;
    }
    // (Additional route resolution and connect steps should be here)
    connected_ = true;
    return true;
}

bool rdma_connector::listen(int _port)
{
    channel_ = rdma_create_event_channel();
    if (!channel_)
        return false;

    if (rdma_create_id(channel_, &cm_id_, nullptr, RDMA_PS_TCP)) {
        rdma_destroy_event_channel(channel_);
        channel_ = nullptr;
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (rdma_bind_addr(cm_id_, (struct sockaddr*)&addr) ||
        rdma_listen(cm_id_, 1)) {
        disconnect();
        return false;
    }
    return true;
}

bool rdma_connector::accept()
{
    struct rdma_cm_event* event = nullptr;
    if (rdma_get_cm_event(channel_, &event))
        return false;
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        rdma_ack_cm_event(event);
        return false;
    }
    // (Set up connection parameters as needed)
    if (rdma_accept(cm_id_, nullptr)) {
        rdma_ack_cm_event(event);
        return false;
    }
    rdma_ack_cm_event(event);
    connected_ = true;
    return true;
}

void rdma_connector::disconnect()
{
    if (cm_id_) {
        rdma_destroy_id(cm_id_);
        cm_id_ = nullptr;
    }
    if (channel_) {
        rdma_destroy_event_channel(channel_);
        channel_ = nullptr;
    }
    connected_ = false;
}

bool rdma_connector::is_connected() const
{
    return connected_;
}

//----------------------
// TCPConnector Implementation
//----------------------
tcp_connector::tcp_connector(ip_version version)
    : ip_ver_(version),
      socket_fd_(-1),
      client_fd_(-1),
      connected_(false)
{
}

tcp_connector::~tcp_connector()
{
    disconnect();
}

bool tcp_connector::create_and_bind_socket(int port)
{
    sa_family_t family = (ip_ver_ == ip_version::v6) ? AF_INET6 : AF_INET;
    socket_fd_ = socket(family, SOCK_STREAM, 0);
    if (socket_fd_ < 0)
        return false;

    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    if (family == AF_INET6) {
        struct sockaddr_in6 addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_any;
        addr.sin6_port = htons(port);
        if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
    } else {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
    }
    return true;
}

bool tcp_connector::connect(const std::string& _address, int _port)
{
    struct addrinfo hints;
    struct addrinfo* result = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = (ip_ver_ == ip_version::v6) ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(_address.c_str(), std::to_string(_port).c_str(), &hints, &result) != 0)
        return false;

    bool success = false;
    for (auto rp = result; rp != nullptr; rp = rp->ai_next) {
        socket_fd_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd_ < 0)
            continue;
        if (::connect(socket_fd_, rp->ai_addr, rp->ai_addrlen) == 0) {
            success = true;
            break;
        }
        close(socket_fd_);
        socket_fd_ = -1;
    }
    freeaddrinfo(result);
    if (!success)
        return false;
    client_fd_ = socket_fd_;
    connected_ = true;
    return true;
}

bool tcp_connector::listen(int _port)
{
    if (!create_and_bind_socket(_port))
        return false;
    if (::listen(socket_fd_, 1) < 0)
        return false;
    connected_ = true;
    return true;
}

bool tcp_connector::accept()
{
    client_fd_ = ::accept(socket_fd_, nullptr, nullptr);
    if (client_fd_ < 0)
        return false;
    return true;
}

void tcp_connector::disconnect()
{
    if (client_fd_ >= 0) {
        close(client_fd_);
        client_fd_ = -1;
    }
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
}

bool tcp_connector::is_connected() const
{
    return connected_;
}

bool tcp_connector::receive_qp_create_msg(qp_create_msg* msg)
{
    if (!msg)
        return false;
    ssize_t bytes = read(client_fd_, msg, sizeof(qp_create_msg));
    return bytes == sizeof(qp_create_msg);
}

bool tcp_connector::send_qp_response(const qp_response_msg& response)
{
    ssize_t bytes = write(client_fd_, &response, sizeof(qp_response_msg));
    return bytes == sizeof(qp_response_msg);
}

bool tcp_connector::exchange_qp_info(const connection_info& local_info, connection_info* remote_info)
{
    if (write(client_fd_, &local_info, sizeof(local_info)) != sizeof(local_info))
        return false;
    if (read(client_fd_, remote_info, sizeof(*remote_info)) != sizeof(*remote_info))
        return false;
    return true;
}

bool tcp_connector::handle_qp_messages(qp_create_msg* qp_msg, const qp_response_msg& response)
{
    // Receive the QP creation request message from the client.
    if (!receive_qp_create_msg(qp_msg))
        return false;
    // Send the externally provided QP creation response (with rkey and address, etc.)
    return send_qp_response(response);
}

//----------------------
// Connection Manager Implementation
//----------------------
connection_manager::connection_manager(protocol proto)
{
    switch (proto) {
        case protocol::RDMA:
            connector_ = std::make_unique<rdma_connector>();
            break;
        case protocol::TCP:
            connector_ = std::make_unique<tcp_connector>();
            break;
    }
}

bool connection_manager::connect(const std::string& address, int port)
{
    return connector_->connect(address, port);
}

bool connection_manager::listen(int port)
{
    return connector_->listen(port);
}

bool connection_manager::accept()
{
    return connector_->accept();
}

void connection_manager::disconnect()
{
    connector_->disconnect();
}

bool connection_manager::is_connected() const
{
    return connector_->is_connected();
}

int connection_manager::get_socket() const
{
    return connector_->get_socket();
}

bool connection_manager::handle_qp_messages(qp_create_msg* qp_msg, const qp_response_msg& response)
{
    tcp_connector* tcp = dynamic_cast<tcp_connector*>(connector_.get());
    if (!tcp)
        return false;
    // Receive the QP creation message
    if (!tcp->receive_qp_create_msg(qp_msg))
        return false;
    // External QP initialization occurs; send the provided response.
    return tcp->send_qp_response(response);
}
