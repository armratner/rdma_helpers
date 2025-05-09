#include "../rdma_objects/rdma_objects.h"


enum CONNECTION_STATUS {
    CONNECTION_STATUS_OK,
    CONNECTION_STATUS_ERROR,
    CONNECTION_STATUS_INITIALIZING,
    CONNECTION_STATUS_DISCONNECTED,
    CONNECTION_STATUS_CONNECTED,
    CONNECTION_STATUS_CLOSED,
    CONNECTION_STATUS_CONNECTING,
    CONNECTION_STATUS_DISCONNECTING,
    CONNECTION_STATUS_TIMEOUT
};

struct connection_resources {
    queue_pair* _qp;
    protection_domain* _pd;
    completion_queue_devx* _cq;
    rdma_device* _rdevice;
    uar* _uar_obj;
    memory_region* _mr;

    STATUS
    create_resources(
        rdma_device* rdevice,
        qp_init_connection_params& con_params,
        qp_init_creation_params& qp_params,
        cq_hw_params& cq_hw_params,
        mr_creation_params& mr_params
    )
    {
        STATUS res = STATUS_OK;

        if (rdevice == nullptr) {
            log_error("Rdma device is null");
            return STATUS_INVALID_PARAM;
        }

        auto_ref<protection_domain> pd;
        res = pd->initialize(rdevice->get_context());
        if (FAILED(res)) {
            log_error("Failed to initialize protection domain");
            return res;
        }

        //calc qp buffer size
        auto hca_cap = rdevice->get_hca_cap();
        auto dev_attr = rdevice->get_device_attr();
        uint32_t max_inline = max_inline_from_wqesz(hca_cap.max_wqe_sz_sq, dev_attr->max_sge);
        qp_umem_layout layout = calc_qp_umem_layout(qp_params.max_recv_wr,
                                                    MLX5_RQ_STRIDE,
                                                    qp_params.max_send_wr,
                                                    dev_attr->max_sge,
                                                    max_inline);

        auto_ref<user_memory> umem_sq;
        res = umem_sq->initialize(rdevice->get_context(), layout.total_bytes);
        RETURN_IF_FAILED_MSG(res, "Failed to initialize user memory for SQ");

        auto_ref<user_memory> umem_db;
        res = umem_db->initialize(rdevice->get_context(), get_page_size());
        RETURN_IF_FAILED_MSG(res, "Failed to initialize user memory for DB");

        auto_ref<uar> uar_obj;
        res = uar_obj->initialize(rdevice->get_context());
        RETURN_IF_FAILED_MSG(res, "Failed to initialize UAR");


        cq_hw_params.log_page_size = get_page_size_log();
        cq_hw_params.cqe_sz = 0;
        cq_hw_params.cqe_comp_en = 0;
        cq_hw_params.cqe_comp_layout = 0;
        cq_hw_params.mini_cqe_res_format = 0;

        auto_ref<completion_queue_devx> cq;
        res = cq->initialize(rdevice, cq_hw_params);
        RETURN_IF_FAILED_MSG(res, "Failed to initialize completion queue");

        qp_params.rdevice = rdevice;
        qp_params.context = rdevice->get_context();
        qp_params.pdn = pd->get_pdn();
        qp_params.cqn = cq->get_cqn();
        qp_params.uar_obj = uar_obj;
        qp_params.umem_sq = umem_sq;
        qp_params.umem_db = umem_db;

        auto_ref<queue_pair> qp;
        res = qp->initialize(qp_params);
        RETURN_IF_FAILED_MSG(res, "Failed to initialize queue pair");

        auto_ref<memory_region> mr;
        res = mr->initialize(rdevice, qp, pd, mr_params.length);
        RETURN_IF_FAILED_MSG(res, "Failed to initialize memory region");

        _rdevice = rdevice;
        _qp      = qp.get();
        _pd      = pd.get();
        _cq      = cq.get();
        _uar_obj = uar_obj.get();
        _mr      = mr.get();

        return STATUS_OK;
    }
}; // <-- Added missing semicolon here

class rdma_ep {
public:
    using connection_id_t = uint64_t;

    rdma_ep(connection_id_t id);
    ~rdma_ep();

    void destroy();

    STATUS initialize(rdma_device* rdevice,
                      qp_init_connection_params& con_parmas,
                      qp_init_creation_params& qp_params,
                      cq_hw_params& cq_hw_params,
                      mr_creation_params& mr_params);
    


    connection_id_t get_id() const {
        return _id;
    }
    
    const std::string& get_address() const {
        return _address;
    }

    uint16_t get_port() const {
        return _port;
    }

    void set_status(CONNECTION_STATUS status) {
        _status = status;
    }

    CONNECTION_STATUS get_status() const {
        return _status;
    }

    void set_address(const std::string& address) {
        _address = address;
    }

    void set_port(uint16_t port) {
        _port = port;
    }

    uint32_t get_lkey() const {
        return _resources._mr->get_lkey();
    }

    uint32_t get_rkey() const {
        return _resources._mr->get_rkey();
    }

    uint32_t get_mr_id() const {
        return _resources._mr->get_mr_id();
    }

    uint32_t get_qpn() const {
        return _resources._qp->get_qpn();
    }

    uint32_t get_cqn() const {
        return _resources._cq->get_cqn();
    }

    uint32_t get_pd() const {
        return _resources._pd->get_pdn();
    }

    void* get_mr_addr() const {
        return _resources._mr->get_addr();
    }

    uint32_t get_mr_size() const {
        return _resources._mr->get_length();
    }

private:

    connection_resources _resources;

    connection_id_t   _id;
    std::string       _address;
    uint16_t          _port;
    CONNECTION_STATUS _status;
};

