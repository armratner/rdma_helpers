#include "cm.h"
#include "objects.h"
#include "common.h"
#include "auto_ref.h"
#include "config_parser.h"
#include <iostream>

struct host_config {
    std::string address;
    int port;
    bool is_server;
    rdma_config rdma_cfg;
};

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <is_server>" << std::endl;
        return 1;
    }

    host_config cfg;
    cfg.rdma_cfg = config_parser::parse_config(argv[1]);
    cfg.is_server = std::string(argv[2]) == "server";

    STATUS res = STATUS_OK;
    
    try {
        rdma_config config = cfg.rdma_cfg;
        
        // Use the first host configuration
        const auto& host_config = config.hosts[0];
        
        auto_ref<rdma_device> dev;
        res = dev->initialize(host_config.device_name.c_str());
        RETURN_IF_FAILED(res);

        printf("Device: %s\n", ibv_get_device_name(dev->get()));
        dev->print_device_attr();

        auto_ref<protection_domain> pd;
        res = pd->initialize(dev->get_context());
        RETURN_IF_FAILED(res);

        printf("Protection Domain Number: %d\n", pd->get_pdn());

        auto_ref<completion_queue> cq;
        res = cq->initialize(dev->get_context(), config.cq_size);
        RETURN_IF_FAILED(res);

        printf("Completion Queue Number: %d\n", cq->get_cqn());

        auto_ref<uar> uar;
        res = uar->initialize(dev->get_context());
        RETURN_IF_FAILED(res);

        printf("UAR: %p\n", uar->get());

        auto_ref<user_memory> umem;
        res = umem->initialize(dev->get_context(), config.buffer_size * WQE_STRIDE); 
        RETURN_IF_FAILED(res);

        auto_ref<user_memory> umem_db;
        res = umem_db->initialize(dev->get_context(), config.buffer_size * WQE_STRIDE); 
        RETURN_IF_FAILED(res);

        printf("UMEM: %p\n", umem->addr());

        // Initialize QPs based on configuration
        std::vector<auto_ref<queue_pair>> qps;
        for (uint32_t i = 0; i < config.qp_config.num_qps; i++) {
            auto_ref<queue_pair> qp;

            queue_pair::qp_params params = queue_pair::qp_params()
                                         .set_sq_size(config.qp_config.sq_size)
                                         .set_rq_size(config.qp_config.rq_size)
                                         .set_max_send_wr(config.qp_config.max_send_wr)
                                         .set_max_recv_wr(config.qp_config.max_recv_wr);

            res = qp->initialize(dev->get_context(),
                                 pd->get_pdn(),
                                 cq->get_cqn(),
                                 umem->umem_id(),
                                 umem_db->umem_id(),
                                 params);
            RETURN_IF_FAILED(res);
            printf("Queue Pair %d Number: %d\n", i, qp->get_qpn());
            qps.push_back(std::move(qp));
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
