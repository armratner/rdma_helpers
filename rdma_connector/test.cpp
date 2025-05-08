#include "../rdma_connector/connector.h"
#include "../rdma_objects/rdma_objects.h"
#include "../rdma_objects/auto_ref.h"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <string>
#include <cstring>
#include <csignal>

// Global variables to control test execution
std::atomic<bool> g_running{true};
std::mutex g_mutex;
std::condition_variable g_cv;
std::atomic<int> g_connected_clients{0};
const int EXPECTED_CLIENTS = 3;

// Handle signals (Ctrl+C)
void signal_handler(int sig) {
    std::cout << "Received signal " << sig << ", shutting down..." << std::endl;
    g_running = false;
    g_cv.notify_all();
}

// Message structure for RDMA operations
struct message_t {
    char data[64];
    uint32_t length;
    uint32_t msg_id;
};

// Client function - will be run in separate threads
void client_thread(int client_id, const std::string& server_ip, uint16_t server_port) {
    std::string client_name = "Client-" + std::to_string(client_id);
    std::cout << client_name << ": Starting client..." << std::endl;

    // Create RDMA components
    auto_ref<rdma_device> device;
    STATUS res = device->initialize("mlx5_0"); // Use the appropriate device name
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to initialize RDMA device" << std::endl;
        return;
    }

    auto_ref<protection_domain> pd;
    res = pd->initialize(device->get_context());
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to initialize protection domain" << std::endl;
        return;
    }

    // Setup QP-related resources
    auto_ref<user_memory> umem_sq;
    res = umem_sq->initialize(device->get_context(), 1024);
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to initialize user memory for SQ" << std::endl;
        return;
    }

    auto_ref<user_memory> umem_db;
    res = umem_db->initialize(device->get_context(), 1024);
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to initialize user memory for DB" << std::endl;
        return;
    }

    auto_ref<uar> uar_obj;
    res = uar_obj->initialize(device->get_context());
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to initialize UAR" << std::endl;
        return;
    }

    // Create completion queue
    ibv_cq_init_attr_ex cq_attr_ex{};
    cq_attr_ex.cqe = 1024;
    cq_attr_ex.comp_vector = 0;
    cq_attr_ex.flags = 0;
    cq_attr_ex.wc_flags = IBV_CREATE_CQ_SUP_WC_FLAGS;
    cq_attr_ex.comp_mask = 0;
    cq_attr_ex.parent_domain = pd->get();

    mlx5dv_cq_init_attr dv_cq_attr{};
    dv_cq_attr.comp_mask = MLX5DV_CQ_INIT_ATTR_MASK_FLAGS;
    dv_cq_attr.flags = 0;

    cq_creation_params cq_params = {
        .context = device->get_context(),
        .cq_attr_ex = &cq_attr_ex,
        .dv_cq_attr = &dv_cq_attr
    };

    auto_ref<completion_queue_devx> cq;
    cq_hw_params hw_params;
    res = cq->initialize(device.get(), hw_params);
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to initialize completion queue" << std::endl;
        return;
    }

    // Create queue pair
    qp_init_creation_params qp_params = {
        .rdevice = device.get(),
        .context = device->get_context(),
        .pdn = pd->get_pdn(),
        .cqn = cq->get_cqn(),
        .uar_obj = uar_obj,
        .umem_sq = umem_sq,
        .umem_db = umem_db,
        .sq_size = 4,
        .rq_size = 4,
        .max_send_wr = 4,
        .max_recv_wr = 4,
        .max_send_sge = 1,
        .max_recv_sge = 1,
        .max_inline_data = 64,
        .max_rd_atomic = 1,
        .max_dest_rd_atomic = 1
    };

    auto_ref<queue_pair> qp;
    res = qp->initialize(qp_params);
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to initialize queue pair" << std::endl;
        return;
    }

    // Create a connector
    rdma_connector connector;
    connector.set_timeout(std::chrono::milliseconds(5000));
    
    // Connect to the server
    std::cout << client_name << ": Connecting to server " << server_ip << ":" << server_port << std::endl;
    auto conn_id = connector.connect_to_server(server_ip, server_port);
    if (conn_id == 0) {
        std::cerr << client_name << ": Failed to connect to server" << std::endl;
        return;
    }
    
    std::cout << client_name << ": Connected to server with connection ID " << conn_id << std::endl;

    // Increment connected clients counter
    g_connected_clients++;
    if (g_connected_clients >= EXPECTED_CLIENTS) {
        g_cv.notify_all();
    }

    // Setup RDMA connection
    res = connector.setup_connection(conn_id, *qp, *device, *pd);
    std::cout << client_name << ": setup_connection returned " << res << std::endl;
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to setup RDMA connection" << std::endl;
        connector.close_connection(conn_id);
        return;
    }
    std::cout << client_name << ": QP number: " << qp->get_qpn() << ", CQ number: " << cq->get_cqn() << std::endl << std::flush;
    std::cout << client_name << ": QP pointer: " << qp->get() << ", CQ pointer: " << cq->get() << std::endl << std::flush;
    // Print QP state after setup
    struct ibv_qp_attr attr = {};
    struct ibv_qp_init_attr init_attr = {};
    int qp_state_ret = -1;
    if (qp->get()) {
        qp_state_ret = ibv_query_qp(qp->get(), &attr, IBV_QP_STATE, &init_attr);
        std::cout << client_name << ": ibv_query_qp() called on QP pointer. Return: " << qp_state_ret << std::endl;
    } else {
        std::cout << client_name << ": QP pointer is nullptr, cannot query QP state via verbs." << std::endl;
    }
    if (qp_state_ret == 0) {
        std::cout << client_name << ": QP state after setup: " << attr.qp_state << std::endl;
    }

    /* ---- obtain remote destination info the server just sent ---- */
    uint64_t raddr = 0;
    uint32_t rkey  = 0;
    connector.recv_data(conn_id, &raddr, sizeof(raddr));
    connector.recv_data(conn_id, &rkey,  sizeof(rkey));
    std::cout << client_name << ": Received raddr=0x" << std::hex << raddr << ", rkey=0x" << rkey << std::dec << std::endl << std::flush;

    // Allocate memory for RDMA operations
    void* buf = aligned_alloc(64, 4096); // 4KB aligned to 64 bytes
    if (!buf) {
        std::cerr << client_name << ": Failed to allocate memory" << std::endl;
        connector.close_connection(conn_id);
        return;
    }
    memset(buf, 0, 4096);

    // Register memory region
    auto_ref<memory_region> mr;
    res = mr->initialize(device, qp, pd, 4096);
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to register memory region" << std::endl;
        free(buf);
        connector.close_connection(conn_id);
        return;
    }

    // Prepare a message
    message_t* msg = static_cast<message_t*>(buf);
    snprintf(msg->data, sizeof(msg->data), "Hello from %s", client_name.c_str());
    msg->length = strlen(msg->data) + 1;
    msg->msg_id = client_id;

    // Allocate separate buffer for receiving
    void* recv_buf = (char*)buf + 2048; // Second half of the buffer
    message_t* recv_msg = static_cast<message_t*>(recv_buf);
    
    std::cout << client_name << ": Sending message: " << msg->data << std::endl;
    std::cout << client_name << ": post_write flags: 0x" << std::hex << (IBV_SEND_SIGNALED) << std::dec << std::endl << std::flush;
    std::cout << client_name << ": Local buffer address: " << buf << ", lkey: 0x" << std::hex << mr->get_lkey() << std::dec << ", size: 4096" << std::endl << std::flush;
    std::cout << client_name << ": Message address: " << (void*)msg << ", length: " << msg->length << ", msg_id: " << msg->msg_id << std::endl << std::flush;
    std::cout << client_name << ": Remote buffer address: 0x" << std::hex << raddr << ", rkey: 0x" << rkey << std::dec << std::endl << std::flush;
    uint64_t wr_id = 0x12345678 + client_id;
    res = qp->post_write(
        msg,
        mr->get_lkey(),
        (void*)raddr,
        rkey,
        sizeof(message_t),
        IBV_SEND_SIGNALED
    );
    if (FAILED(res)) {
        std::cerr << client_name << ": Failed to post RDMA write, res=" << res << std::endl;
    } else {
        std::cout << client_name << ": RDMA write posted successfully" << std::endl;
        // Retry loop for CQ polling
        int max_poll = 10;
        bool cqe_found = false;
        for (int poll_attempt = 0; poll_attempt < max_poll; ++poll_attempt) {
            res = cq->poll_cq();
            if (!FAILED(res)) {
                std::cout << client_name << ": RDMA operation completed (CQE) after " << poll_attempt + 1 << " attempts" << std::endl;
                cqe_found = true;
                break;
            }
            if (poll_attempt % 100 == 0) std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        if (!cqe_found) {
            std::cerr << client_name << ": Failed to poll completion queue after " << max_poll << " attempts" << std::endl;
        }
    }

    // Wait for test to complete
    {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait(lock, []{ return !g_running; });
    }

    // Cleanup
    std::cout << client_name << ": Closing connection..." << std::endl;
    connector.close_connection(conn_id);
    free(buf);
}

// Server function
int main(int argc, char** argv) {
    // Register signal handler
    signal(SIGINT, signal_handler);

    std::cout << "Starting RDMA Connector Test..." << std::endl;
    
    // Define server parameters
    std::string server_ip = "0.0.0.0"; // Listen on all interfaces
    uint16_t server_port = 18515;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--port" && i + 1 < argc) {
            server_port = std::stoi(argv[i + 1]);
            i++;
        } else if (std::string(argv[i]) == "--ip" && i + 1 < argc) {
            server_ip = argv[i + 1];
            i++;
        }
    }

    // Create RDMA device for server
    auto_ref<rdma_device> device;
    STATUS res = device->initialize("mlx5_0"); // Use the appropriate device name
    if (FAILED(res)) {
        std::cerr << "Server: Failed to initialize RDMA device" << std::endl;
        return 1;
    }

    // Create protection domain
    auto_ref<protection_domain> pd;
    res = pd->initialize(device->get_context());
    if (FAILED(res)) {
        std::cerr << "Server: Failed to initialize protection domain" << std::endl;
        return 1;
    }

    // Create connection manager (server)
    rdma_connector server;
    server.initialize(server_ip, server_port)
          .set_max_connections(10)
          .on_connection([&](auto id, auto ip, auto port) {
              std::cout << "Server: New connection from " << ip << ":" << port
                        << " (ID: " << id << ")" << std::endl;

              STATUS rc;

              /* ---------- per‑connection RDMA resources ---------- */
              auto_ref<user_memory> umem_sq;
              rc = umem_sq->initialize(device->get_context(), 1024);
              if (FAILED(rc)) {
                  std::cerr << "Server: Failed to init SQ umem for ID " << id << '\n';
                  server.close_connection(id);
                  return;
              }

              auto_ref<user_memory> umem_db;
              rc = umem_db->initialize(device->get_context(), 1024);
              if (FAILED(rc)) {
                  std::cerr << "Server: Failed to init DB umem for ID " << id << '\n';
                  server.close_connection(id);
                  return;
              }

              auto_ref<uar> uar_obj;
              rc = uar_obj->initialize(device->get_context());
              if (FAILED(rc)) {
                  std::cerr << "Server: Failed to init UAR for ID " << id << '\n';
                  server.close_connection(id);
                  return;
              }

              /* ---------- completion queue ---------- */
              ibv_cq_init_attr_ex cq_attr_ex{};
              cq_attr_ex.cqe            = 1024;
              cq_attr_ex.comp_vector    = 0;
              cq_attr_ex.flags          = 0;
              cq_attr_ex.wc_flags       = IBV_CREATE_CQ_SUP_WC_FLAGS;
              cq_attr_ex.comp_mask      = 0;
              cq_attr_ex.parent_domain  = pd->get();

              mlx5dv_cq_init_attr dv_cq_attr{};
              dv_cq_attr.comp_mask = MLX5DV_CQ_INIT_ATTR_MASK_FLAGS;
              dv_cq_attr.flags     = 0;

              cq_creation_params cq_params = {
                  .context     = device->get_context(),
                  .cq_attr_ex  = &cq_attr_ex,
                  .dv_cq_attr  = &dv_cq_attr
              };

              auto_ref<completion_queue_devx> cq;
              cq_hw_params hw_params;
              rc = cq->initialize(device.get(), hw_params);
              if (FAILED(rc)) {
                  std::cerr << "Server: Failed to init CQ for ID " << id << '\n';
                  server.close_connection(id);
                  return;
              }

              /* ---------- queue pair ---------- */
              qp_init_creation_params qp_params = {
                  .rdevice            = device.get(),
                  .context            = device->get_context(),
                  .pdn                = pd->get_pdn(),
                  .cqn                = cq->get_cqn(),
                  .uar_obj            = uar_obj,
                  .umem_sq            = umem_sq,
                  .umem_db            = umem_db,
                  .sq_size            = 4,
                  .rq_size            = 4,
                  .max_send_wr        = 4,
                  .max_recv_wr        = 4,
                  .max_send_sge       = 1,
                  .max_recv_sge       = 1,
                  .max_inline_data    = 64,
                  .max_rd_atomic      = 1,
                  .max_dest_rd_atomic = 1
              };

              auto_ref<queue_pair> qp;
              rc = qp->initialize(qp_params);
              if (FAILED(rc)) {
                  std::cerr << "Server: Failed to init QP for ID " << id << '\n';
                  server.close_connection(id);
                  return;
              }

              /* ---------- exchange QP info with peer ---------- */
              rc = server.setup_connection(id, *qp, *device, *pd);
              if (FAILED(rc)) {
                  std::cerr << "Server: Failed to setup RDMA conn for ID " << id << '\n';
                  server.close_connection(id);
                  return;
              }

              /* ---------- prepare and post a receive buffer ---------- */
              void* recv_buf = aligned_alloc(64, 4096);   // 4 KiB, 64‑byte aligned
              if (!recv_buf) {
                  std::cerr << "Server: Failed to allocate recv buffer for ID " << id << '\n';
                  server.close_connection(id);
                  return;
              }
              memset(recv_buf, 0, 4096);

              auto_ref<memory_region> mr_recv;
              rc = mr_recv->initialize(device, qp, pd, 4096);
              if (FAILED(rc)) {
                  std::cerr << "Server: Failed to register recv MR for ID " << id << '\n';
                  server.close_connection(id);
                  return;
              }

              /* ---- publish raddr + rkey to the client ---- */
              uint64_t raddr = reinterpret_cast<uint64_t>(recv_buf);
              uint32_t rkey  = mr_recv->get_rkey();
              server.send_data(id, &raddr, sizeof(raddr));
              server.send_data(id, &rkey,  sizeof(rkey));

              std::cout << "Server: RDMA connection established (ID " << id << ")\n";
          })
          .on_disconnection([](auto id) {
              std::cout << "Server: Connection " << id << " closed" << std::endl;
          });

    // Start server
    if (!server.start_server()) {
        std::cerr << "Server: Failed to start RDMA connection server" << std::endl;
        return 1;
    }

    std::cout << "Server: Listening on " << server_ip << ":" << server_port << std::endl;

    // Start client threads
    std::vector<std::thread> client_threads;
    const char* client_target_ip = "127.0.0.1"; // Loopback for local testing
    
    std::cout << "Server: Starting " << EXPECTED_CLIENTS << " client threads..." << std::endl;
    for (int i = 0; i < EXPECTED_CLIENTS; i++) {
        client_threads.emplace_back(client_thread, i + 1, client_target_ip, server_port);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Stagger client starts
    }

    // Wait for all clients to connect
    {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait_for(lock, std::chrono::seconds(10), []{ 
            return g_connected_clients >= EXPECTED_CLIENTS; 
        });
    }

    if (g_connected_clients >= EXPECTED_CLIENTS) {
        std::cout << "Server: All clients connected successfully!" << std::endl;
    } else {
        std::cout << "Server: Only " << g_connected_clients << "/" << EXPECTED_CLIENTS 
                  << " clients connected within timeout" << std::endl;
    }

    // Run the test for a few seconds
    std::cout << "Server: Running test for 5 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Signal client threads to terminate
    g_running = false;
    g_cv.notify_all();

    // Wait for client threads to join
    std::cout << "Server: Waiting for client threads to terminate..." << std::endl;
    for (auto& thread : client_threads) {
        thread.join();
    }

    // Stop server
    std::cout << "Server: Stopping server..." << std::endl;
    server.stop_server();

    std::cout << "Test completed!" << std::endl;
    return 0;
}