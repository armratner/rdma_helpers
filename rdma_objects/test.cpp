#include "rdma_objects.h"
#include "auto_ref.h"
#include <arpa/inet.h>

int main ()
{
    STATUS res = STATUS_OK;

    auto_ref<rdma_device> rdevice;
    res = rdevice->initialize("mlx5_0");
    if (FAILED(res)) {
        log_error("Failed to initialize RDMA device");
        return res;
    }

    auto_ref<protection_domain> pd;
    res = pd->initialize(rdevice->get_context());
    if (FAILED(res)) {
        log_error("Failed to initialize protection domain");
        return res;
    }

    auto_ref<user_memory> umem_sq;
    res = umem_sq->initialize(rdevice->get_context(), 1024);
    if (FAILED(res)) {
        log_error("Failed to initialize user memory");
        return res;
    }

    auto_ref<user_memory> umem_db;
    res = umem_db->initialize(rdevice->get_context(), 1024);
    if (FAILED(res)) {
        log_error("Failed to initialize user memory for db");
        return res;
    }

    auto_ref<uar> uar;
    res = uar->initialize(rdevice->get_context());
    if (FAILED(res)) {
        log_error("Failed to initialize UAR");
        return res;
    }

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
        .context = rdevice->get_context(),
        .cq_attr_ex = &cq_attr_ex,
        .dv_cq_attr = &dv_cq_attr
    };

    auto_ref<completion_queue> cq;
    res = cq->initialize(cq_params);
    if (FAILED(res)) {
        log_error("Failed to initialize completion queue");
        return res;
    }

    qp_init_creation_params qp_params = {};
    qp_params.rdevice = rdevice.get();
    qp_params.context = rdevice->get_context();
    qp_params.pdn = pd->get_pdn();
    qp_params.cqn = cq->get_cqn();
    qp_params.uar_obj = uar;
    qp_params.umem_sq = umem_sq;
    qp_params.umem_db = umem_db;
    qp_params.sq_size = 2;
    qp_params.rq_size = 2;
    qp_params.max_send_wr = 0;
    qp_params.max_recv_wr = 0;
    qp_params.max_send_sge = 0;
    qp_params.max_recv_sge = 0;
    qp_params.max_inline_data = 0;
    qp_params.max_rd_atomic = 0;
    qp_params.max_dest_rd_atomic = 0;

    auto_ref<queue_pair> p_qp;
    res = p_qp->initialize(qp_params);
    if (FAILED(res)) {
        log_error("Failed to initialize queue pair");
        return res;
    }

    qp_init_connection_params qp_init_conn_params {};

    qp_init_conn_params.pd = pd->get();
    qp_init_conn_params.mtu = IBV_MTU_1024;
    qp_init_conn_params.ece = 0;
    qp_init_conn_params.port_num = 1;

    // Fill in remote_ah_attr with test values
    qp_init_conn_params.remote_ah_attr = aligned_alloc<ibv_ah_attr>(sizeof(ibv_ah_attr));
    if (!qp_init_conn_params.remote_ah_attr) {
        log_error("Failed to allocate memory for remote_ah_attr");
        return STATUS_ERR;
    }

    // Setup for loopback using local GIDs
    qp_init_conn_params.remote_ah_attr->is_global = 1;
    qp_init_conn_params.remote_ah_attr->grh.sgid_index = 3; // Using same index as in show_gids
    qp_init_conn_params.remote_ah_attr->sl = 0;
    qp_init_conn_params.remote_ah_attr->src_path_bits = 0;
    qp_init_conn_params.remote_ah_attr->port_num = 1;

    // Setup destination GID using the local GID for loopback (12.11.0.2)
    memset(&qp_init_conn_params.remote_ah_attr->grh.dgid, 0, sizeof(ibv_gid));
    // Set for IPv4-mapped IPv6 address 0000:0000:0000:0000:0000:ffff:0c0b:0002
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[10] = 0xff;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[11] = 0xff;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[12] = 0x0c;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[13] = 0x0b;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[14] = 0x00;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[15] = 0x02;

    // Set additional GRH fields
    qp_init_conn_params.remote_ah_attr->grh.traffic_class = 0;
    qp_init_conn_params.remote_ah_attr->grh.flow_label = 0;
    qp_init_conn_params.remote_ah_attr->grh.hop_limit = 64;

    res = p_qp->reset_to_init(qp_init_conn_params);
    RETURN_IF_FAILED(res);

    res = p_qp->init_to_rtr(qp_init_conn_params);
    RETURN_IF_FAILED(res);

    res = p_qp->rtr_to_rts(qp_init_conn_params);
    RETURN_IF_FAILED(res);

    free(qp_init_conn_params.remote_ah_attr);
    qp_init_conn_params.remote_ah_attr = nullptr;

    void* addr = calloc(1, 262000);
    if (!addr) {
        log_error("Failed to allocate memory for address");
        return STATUS_ERR;
    }

    auto_ref<memory_region> mr;
    res = mr->initialize(rdevice, p_qp, pd, addr, 262000);
    RETURN_IF_FAILED(res);

    // Check port state before proceeding
    const ibv_port_attr* port_attr = rdevice->get_port_attr(1);
    if (!port_attr) {
        log_error("Failed to get port attributes");
        free(addr);
        return STATUS_ERR;
    }
    
    log_debug("Proceeding with transfer on port with state: %s", ibv_port_state_str(port_attr->state));
    if (port_attr->state != IBV_PORT_ACTIVE) {
        log_error("Port is not in ACTIVE state! Current state: %s", ibv_port_state_str(port_attr->state));
        log_error("RDMA operations may fail on non-active ports");
    }

    // Now we can post an RDMA Write request instead of Send
    // Prepare data to send
    const char* test_message = "Hello RDMA World!";
    
    // For loopback test, we'll use two regions in the same buffer
    // Source data at the beginning
    char* src_addr = (char*)addr;
    strncpy(src_addr, test_message, strlen(test_message) + 1);
    
    // Destination data at offset 4096 (page aligned for simplicity)
    char* dst_addr = src_addr + 4096;
    memset(dst_addr, 0, 4096); // Clear destination area
    
    log_debug("Posting RDMA WRITE request with message: %s", test_message);
    log_debug("Source address: %p, Destination address: %p", src_addr, dst_addr);
    log_debug("Source lkey: 0x%x, Destination rkey: 0x%x", mr->get_lkey(), mr->get_rkey());
    log_debug("QPN: %d", p_qp->get_qpn());
    
    // Add a direct memory copy first to verify memory is writable
    log_debug("Verifying memory access with direct write...");
    char backup[256];
    strcpy(backup, test_message); // Save a copy
    memcpy(dst_addr, test_message, strlen(test_message) + 1);
    log_debug("Direct memory write succeeded, content: '%s'", dst_addr);
    memset(dst_addr, 0, 4096); // Clear again for test
    
    // Add a memory barrier to ensure memory is visible before operation
    __sync_synchronize();
    
    // Try posting with different flags and smaller message size
    log_debug("Using smaller payload and IBV_SEND_INLINE flag");
    const char* short_message = "Hello RDMA";
    strncpy(src_addr, short_message, strlen(short_message) + 1);
    
    // Post an RDMA write operation with INLINE flag
    res = p_qp->post_rdma_write(
        src_addr,              // Local source address
        mr->get_lkey(),        // Local key
        (uint64_t)dst_addr,    // Remote destination address
        mr->get_rkey(),        // Remote key (same as local for loopback)
        strlen(short_message) + 1, // Length (shorter message)
        IBV_SEND_SIGNALED | IBV_SEND_INLINE // Add INLINE flag for very small payloads
    );
    
    if (FAILED(res)) {
        log_error("Failed to post RDMA write request");
        free(addr);
        return res;
    }
    
    log_debug("RDMA write request posted successfully");

    // Use event-driven CQ polling (standard verbs)
    res = cq->poll_cq();
    if (FAILED(res)) {
        log_error("CQ polling failed");
        free(addr);
        return res;
    }

    free(addr);
    return STATUS_OK;
}