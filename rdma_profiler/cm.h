#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <rdma/rdma_cma.h>
#include <infiniband/mlx5dv.h>
#include <infiniband/verbs.h>
#include <map>

#include "objects.h"
#include "auto_ref.h"

using namespace std;

// Forward declarations
class queue_pair;
class protection_domain;
class completion_queue;
class user_memory;
class uar;


// Structure used for exchanging connection information
struct connection_info {
    uint32_t qpn;
    uint32_t psn;
    uint64_t buf_addr;
    uint32_t rkey;
};

// Message structure for QP creation request
struct qp_create_msg {
    uint32_t sq_size;
    uint32_t rq_size;
    uint32_t max_send_wr;
    uint32_t max_recv_wr;
    uint32_t max_send_sge;
    uint32_t max_recv_sge;
    uint32_t max_inline_data;
};

// Message structure for QP creation response, now including rkey and addr.
struct qp_response_msg {
    uint32_t qp_num;
    uint32_t psn;
    uint32_t status;  // 0 = success, nonzero = error
    uint32_t rkey;
    uint64_t addr;
};

// Base connector interface
class connector {
public:
    virtual ~connector() = default;
    virtual bool connect(const std::string& address, int port) = 0;
    virtual bool listen(int port) = 0;
    virtual bool accept() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual int get_socket() const { return -1; }
    virtual bool receive_qp_create_msg([[maybe_unused]] qp_create_msg* msg) { return false; }
    virtual bool send_qp_response([[maybe_unused]] const qp_response_msg& response) { return false; }

protected:
    ibv_context* ctx_{nullptr};
};

// RDMA CM based connector
class rdma_connector : public connector {
public:
    rdma_connector();
    ~rdma_connector() override;

    bool connect(const std::string& address, int port) override;
    bool listen(int port) override;
    bool accept() override;
    void disconnect() override;
    bool is_connected() const override;

private:
    bool init_resources();
    
    struct rdma_cm_id* cm_id_{nullptr};
    struct rdma_event_channel* channel_{nullptr};
    bool connected_{false};
};

// TCP socket based connector
class tcp_connector : public connector {
public:
    enum class ip_version {
        v4,
        v6
    };

    explicit tcp_connector(ip_version version = ip_version::v4);
    ~tcp_connector() override;

    bool connect(const std::string& address, int port) override;
    bool listen(int port) override;
    bool accept() override;
    void disconnect() override;
    bool is_connected() const override;
    int get_socket() const override { return client_fd_; }

    // Add missing method declarations
    bool receive_qp_create_msg(qp_create_msg* msg) override;
    bool send_qp_response(const qp_response_msg& response) override;
    bool handle_qp_messages(qp_create_msg* qp_msg, const qp_response_msg& response);

private:
    bool create_and_bind_socket(int port);
    bool init_resources();
    bool exchange_qp_info(const connection_info& local_info, connection_info* remote_info);

    ip_version ip_ver_{ip_version::v4};
    int socket_fd_{-1};
    int client_fd_{-1};
    bool connected_{false};
};

struct connection_resources {
    auto_ref<queue_pair> qp;
    auto_ref<protection_domain> pd;
    auto_ref<completion_queue> cq;
    auto_ref<user_memory> umem_sq;
    auto_ref<user_memory> umem_db;
    auto_ref<uar> uar_obj;
};

// Connection manager wraps a connector (e.g. tcp_connector or rdma_connector)
// and provides a unified interface.
class connection_manager {
public:
    enum class protocol {
        RDMA,
        TCP
    };

    explicit connection_manager(protocol proto);
    ~connection_manager() = default;

    bool connect(const std::string& address, int port);
    bool listen(int port);
    bool accept();
    void disconnect();
    bool is_connected() const;
    int get_socket() const;

    // Receives a QP creation message via the underlying connector,
    // and then (after external QP initialization) sends a response.
    bool handle_qp_messages(qp_create_msg* qp_msg, const qp_response_msg& response);

private:
    unique_ptr<connector> connector_;
    map<uint32_t, connection_resources> _connection_resources;
};
