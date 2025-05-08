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
    res = umem_sq->initialize(rdevice->get_context(), 2048);
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

    auto_ref<user_memory> umem_sq1;
    res = umem_sq1->initialize(rdevice->get_context(), 2048);
    if (FAILED(res)) {
        log_error("Failed to initialize user memory");
        return res;
    }

    auto_ref<user_memory> umem_db1;
    res = umem_db1->initialize(rdevice->get_context(), 1024);
    if (FAILED(res)) {
        log_error("Failed to initialize user memory for db");
        return res;
    }

    auto_ref<uar> uar_obj;
    res = uar_obj->initialize(rdevice->get_context());
    if (FAILED(res)) {
        log_error("Failed to initialize UAR");
        return res;
    }

    auto_ref<uar> uar_obj1;
    res = uar_obj1->initialize(rdevice->get_context());
    if (FAILED(res)) {
        log_error("Failed to initialize UAR");
        return res;
    }

    log_debug("UAR reg_addr: %p", uar_obj->get()->reg_addr);
    log_debug("UAR base_addr: %p", uar_obj->get()->base_addr);
    log_debug("UAR page_id: %u", uar_obj->get()->page_id);
    log_debug("UAR mmap_off: %ld", uar_obj->get()->mmap_off);
    log_debug("UAR1 reg_addr: %p", uar_obj1->get()->reg_addr);
    log_debug("UAR1 base_addr: %p", uar_obj1->get()->base_addr);
    log_debug("UAR1 page_id: %u", uar_obj1->get()->page_id);
    log_debug("UAR1 mmap_off: %ld", uar_obj1->get()->mmap_off);

    cq_hw_params cq_hw_params = {0};
    cq_hw_params.log_cq_size = 9; // 512 entries (2^9)
    cq_hw_params.log_page_size = 12; // 4096 bytes
    cq_hw_params.cqe_sz = 0; // 64 bytes (0 = 64, 1 = 128)
    cq_hw_params.cqe_comp_en = 0;

    auto_ref<completion_queue_devx> cq_devx;
    res = cq_devx->initialize(rdevice, cq_hw_params);
    if (FAILED(res)) {
        log_error("Failed to initialize completion queue devx");
        return res;
    }

    auto_ref<completion_queue_devx> cq_devx1;
    res = cq_devx1->initialize(rdevice, cq_hw_params);
    if (FAILED(res)) {
        log_error("Failed to initialize completion queue devx");
        return res;
    }

    qp_init_creation_params qp_params = {};
    qp_params.rdevice = rdevice.get();
    qp_params.context = rdevice->get_context();
    qp_params.pdn = pd->get_pdn();
    qp_params.cqn = cq_devx->get_cqn();
    qp_params.uar_obj = uar_obj;
    qp_params.umem_sq = umem_sq;
    qp_params.umem_db = umem_db;
    qp_params.sq_size = umem_sq->size() / 64;
    qp_params.rq_size = 1;
    qp_params.max_send_wr = 16;
    qp_params.max_recv_wr = 1;
    qp_params.max_send_sge = 1;
    qp_params.max_recv_sge = 1;
    qp_params.max_inline_data = 64;
    qp_params.max_rd_atomic = 1;
    qp_params.max_dest_rd_atomic = 1;

    auto_ref<queue_pair> p_qp;
    res = p_qp->initialize(qp_params);
    if (FAILED(res)) {
        log_error("Failed to initialize queue pair");
        return res;
    }

    qp_init_creation_params qp_params1 = {};
    qp_params1.rdevice = rdevice.get();
    qp_params1.context = rdevice->get_context();
    qp_params1.pdn = pd->get_pdn();
    qp_params1.cqn = cq_devx1->get_cqn();
    qp_params1.uar_obj = uar_obj1;
    qp_params1.umem_sq = umem_sq1;
    qp_params1.umem_db = umem_db1;
    qp_params1.sq_size = 16;
    qp_params1.rq_size = 1;
    qp_params1.max_send_wr = 16;
    qp_params1.max_recv_wr = 1;
    qp_params1.max_send_sge = 1;
    qp_params1.max_recv_sge = 1;
    qp_params1.max_inline_data = 64;
    qp_params1.max_rd_atomic = 1;
    qp_params1.max_dest_rd_atomic = 1;

    auto_ref<queue_pair> p_qp1;
    res = p_qp1->initialize(qp_params1);
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

    const ibv_port_attr* port_attr = rdevice->get_port_attr(1);
    if (!port_attr) {
        log_error("Failed to get port attributes for port 1");
        return STATUS_ERR;
    }
    uint16_t port_lid = port_attr->lid;
    log_debug("Port 1 LID: 0x%x", port_lid);

    // Setup for loopback using local GIDs
    qp_init_conn_params.remote_ah_attr->is_global = 1;
    qp_init_conn_params.remote_ah_attr->grh.sgid_index = 3; // Using same index as in show_gids
    qp_init_conn_params.remote_ah_attr->sl = 0;
    qp_init_conn_params.remote_ah_attr->src_path_bits = port_lid;
    qp_init_conn_params.remote_ah_attr->port_num = 1;
    qp_init_conn_params.remote_qpn = p_qp1->get_qpn();

    // Setup destination GID using the provided GID for loopback (11.11.0.2)
    memset(&qp_init_conn_params.remote_ah_attr->grh.dgid, 0, sizeof(ibv_gid));

    if (ibv_query_gid(rdevice->get_context(), 1, 3, &qp_init_conn_params.remote_ah_attr->grh.dgid)) {
        log_error("Failed to query GID index 3 for port 1");
        // Handle error, maybe free allocated memory and return
        free(qp_init_conn_params.remote_ah_attr);
        return STATUS_ERR;
    }
    // Set for IPv4-mapped IPv6 address 0000:0000:0000:0000:0000:ffff:0b0b:0002
    /*
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[10] = 0xff;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[11] = 0xff;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[12] = 0x0b;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[13] = 0x0b;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[14] = 0x00;
    qp_init_conn_params.remote_ah_attr->grh.dgid.raw[15] = 0x02;
    */

    // Set additional GRH fields
    qp_init_conn_params.remote_ah_attr->grh.traffic_class = 0;
    qp_init_conn_params.remote_ah_attr->grh.flow_label = 0;
    qp_init_conn_params.remote_ah_attr->grh.hop_limit = 2;

    qp_init_connection_params qp_init_conn_params1 {};

    qp_init_conn_params1.pd = pd->get();
    qp_init_conn_params1.mtu = IBV_MTU_1024;
    qp_init_conn_params1.ece = 0;
    qp_init_conn_params1.port_num = 1;

    // Fill in remote_ah_attr with test values
    qp_init_conn_params1.remote_ah_attr = aligned_alloc<ibv_ah_attr>(sizeof(ibv_ah_attr));
    if (!qp_init_conn_params1.remote_ah_attr) {
        log_error("Failed to allocate memory for remote_ah_attr");
        return STATUS_ERR;
    }

    // Setup for loopback using local GIDs
    qp_init_conn_params1.remote_ah_attr->is_global = 1;
    qp_init_conn_params1.remote_ah_attr->grh.sgid_index = 3; // Using same index as in show_gids
    qp_init_conn_params1.remote_ah_attr->sl = 0;
    qp_init_conn_params1.remote_ah_attr->src_path_bits = port_lid;
    qp_init_conn_params1.remote_ah_attr->port_num = 1;
    qp_init_conn_params1.remote_qpn = p_qp->get_qpn();


    memset(&qp_init_conn_params1.remote_ah_attr->grh.dgid, 0, sizeof(ibv_gid));
    if (ibv_query_gid(rdevice->get_context(), 1, 3, &qp_init_conn_params1.remote_ah_attr->grh.dgid)) {
        log_error("Failed to query GID index 3 for port 1");
        // Handle error, maybe free allocated memory and return
        free(qp_init_conn_params.remote_ah_attr);
        free(qp_init_conn_params1.remote_ah_attr);
        return STATUS_ERR;
    }

    //print the gid 
    char gid_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &qp_init_conn_params1.remote_ah_attr->grh.dgid, gid_str, sizeof(gid_str));
    log_debug("GID: %s", gid_str);

    // Setup destination GID using the provided GID for loopback (11.11.0.2)

    // Set for IPv4-mapped IPv6 address 0000:0000:0000:0000:0000:ffff:0b0b:0002
    /*
    qp_init_conn_params1.remote_ah_attr->grh.dgid.raw[10] = 0xff;
    qp_init_conn_params1.remote_ah_attr->grh.dgid.raw[11] = 0xff;
    qp_init_conn_params1.remote_ah_attr->grh.dgid.raw[12] = 0x0b;
    qp_init_conn_params1.remote_ah_attr->grh.dgid.raw[13] = 0x0b;
    qp_init_conn_params1.remote_ah_attr->grh.dgid.raw[14] = 0x00;
    qp_init_conn_params1.remote_ah_attr->grh.dgid.raw[15] = 0x02;
    */

    // Set additional GRH fields
    qp_init_conn_params1.remote_ah_attr->grh.traffic_class = 0;
    qp_init_conn_params1.remote_ah_attr->grh.flow_label = 0;
    qp_init_conn_params1.remote_ah_attr->grh.hop_limit = 2;

    res = p_qp->reset_to_init(qp_init_conn_params);
    RETURN_IF_FAILED(res);

    res = p_qp->init_to_rtr(qp_init_conn_params);
    RETURN_IF_FAILED(res);
    
    // Add RTS transition for the first QP
    res = p_qp->rtr_to_rts(qp_init_conn_params);
    RETURN_IF_FAILED(res);

    res = p_qp1->reset_to_init(qp_init_conn_params1);
    RETURN_IF_FAILED(res);

    res = p_qp1->init_to_rtr(qp_init_conn_params1);
    RETURN_IF_FAILED(res);
    
    // Add RTS transition for the second QP
    res = p_qp1->rtr_to_rts(qp_init_conn_params1);
    RETURN_IF_FAILED(res);

    free(qp_init_conn_params.remote_ah_attr);
    qp_init_conn_params.remote_ah_attr = nullptr;

    free(qp_init_conn_params1.remote_ah_attr);
    qp_init_conn_params1.remote_ah_attr = nullptr;


    // Register memory for the first QP (sender)
    auto_ref<memory_region> mr_sender;
    res = mr_sender->initialize(rdevice, p_qp, pd, 2048);
    RETURN_IF_FAILED(res);
    
    // Register memory for the second QP (receiver)
    auto_ref<memory_region> mr_receiver;
    res = mr_receiver->initialize(rdevice, p_qp1, pd, 2048);
    RETURN_IF_FAILED(res);
    
    log_debug("MR sender - lkey: 0x%x, rkey: 0x%x", mr_sender->get_lkey(), mr_sender->get_rkey());
    log_debug("MR receiver - lkey: 0x%x, rkey: 0x%x", mr_receiver->get_lkey(), mr_receiver->get_rkey());

    port_attr = rdevice->get_port_attr(1);
    log_debug("Proceeding with transfer on port with state: %s", ibv_port_state_str(port_attr->state));
    if (port_attr->state != IBV_PORT_ACTIVE) {
        log_error("Port is not in ACTIVE state! Current state: %s", ibv_port_state_str(port_attr->state));
        log_error("RDMA operations may fail on non-active ports");
    }

    // Now we can post an RDMA Write request instead of Send
    // Prepare data to send
    const char* test_message = "Hello RDMA World!";
    
    log_debug("Posting RDMA WRITE request with message: %s", test_message);
    log_debug("Source lkey: 0x%x, Destination rkey: 0x%x", mr_sender->get_lkey(), mr_receiver->get_rkey());
    log_debug("QPN: 0x%x", p_qp->get_qpn());
    
    // Add a memory barrier to ensure memory is visible before operation
    __sync_synchronize();

    strncpy(static_cast<char*>(mr_sender->get_addr()), test_message, strlen(test_message) + 1);

    int qp_state_before = p_qp->get_qp_state();
    log_debug("QP state before post_rdma_write: %s (%d)", queue_pair::qp_state_to_str(qp_state_before), qp_state_before);

    int qp_state_before1 = p_qp1->get_qp_state();
    log_debug("QP state before post_rdma_write: %s (%d)", queue_pair::qp_state_to_str(qp_state_before1), qp_state_before1);

    sleep(1); // Sleep for a short time to avoid busy waiting

    res = p_qp->post_rdma_write(mr_sender->get_addr(),
                                mr_sender->get_lkey(),
                                mr_receiver->get_addr(),
                                mr_receiver->get_rkey(),
                                strlen(test_message) + 1,
                                IBV_SEND_SIGNALED);

    if (FAILED(res)) {
        log_error("Failed to post RDMA write request");
        return res;
    }
    
    log_debug("RDMA write request posted successfully");
    
    // Add memory fence to ensure all previous stores are visible
    __sync_synchronize();
    unsigned char* wqe_buf = (unsigned char*)umem_sq->addr() + p_qp->get_sq_buf_offset();
    log_debug("Verifying WQE buffer at %p:", wqe_buf);
    dump_wqe(wqe_buf);
    
    // Verify doorbell area 
    log_debug("Verifying doorbell area at %p:", umem_db->addr());
    uint32_t* db_area = (uint32_t*)umem_db->addr();
    log_debug("Doorbell value: 0x%08x", db_area[MLX5_SND_DBR]);

    // Dump the QP state before polling
    uint32_t hw_sq_counter = 0;
    uint32_t sw_sq_counter = 0;
    uint32_t wq_sig = 0;
    res = p_qp->query_qp_counters(&hw_sq_counter, &sw_sq_counter, &wq_sig);
    if (res == STATUS_OK) {
        log_debug("QP counters before polling - HW: %u, SW: %u, WQ_SIG: %u", hw_sq_counter, sw_sq_counter, wq_sig);
    }

    // Arm the CQ
    log_debug("Arming the completion queue");
    res = cq_devx->arm_cq(0);
    RETURN_IF_FAILED(res);

    int num_tries = 0;
    do {
        // Check for completion
        res = cq_devx->poll_cq();
        if (res == STATUS_OK) {
            log_debug("GOT CQE! RDMA write completed successfully");
            break;
        } else if (res == STATUS_ERR) {
            log_error("NO CQE! RDMA write failed");
        }
        sleep(1); // Sleep for a short time to avoid busy waiting
        num_tries++;
    } while (num_tries < 5);

    int qp_state_after = p_qp->get_qp_state();
    log_debug("QP state after post_rdma_write: %s (%d)", queue_pair::qp_state_to_str(qp_state_after), qp_state_after);

    if (FAILED(res)) {
        log_error("Failed to post RDMA write request");
        return res;
    }

    log_debug("Dest buffer contents: %s", static_cast<char*>(mr_receiver->get_addr()));
    log_debug("RDMA write request posted successfully");
    log_debug("Arming the completion queue");
    res = cq_devx->arm_cq(0);
    RETURN_IF_FAILED(res);

    return STATUS_OK;
}