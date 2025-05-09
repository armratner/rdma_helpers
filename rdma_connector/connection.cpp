#include "connection.h"

rdma_ep::rdma_ep(connection_id_t id)
    : _id(id)
    , _resources({})
    , _status(CONNECTION_STATUS_DISCONNECTED)
    , _address("")

{}

rdma_ep::~rdma_ep() {
    destroy();
}

void
rdma_ep::destroy() {
    if (_resources._qp) {
        _resources._qp->destroy();
        _resources._qp = nullptr;
    }
    if (_resources._pd) {
        _resources._pd->destroy();
        _resources._pd = nullptr;
    }
    if (_resources._cq) {
        _resources._cq->destroy();
        _resources._cq = nullptr;
    }
    if (_resources._uar_obj) {
        _resources._uar_obj->destroy();
        _resources._uar_obj = nullptr;
    }
    if (_resources._mr) {
        _resources._mr->destroy();
        _resources._mr = nullptr;
    }
}

STATUS
rdma_ep::initialize(rdma_device* rdevice,
                    qp_init_connection_params& con_params,
                    qp_init_creation_params& qp_params,
                    cq_hw_params& cq_hw_params,
                    mr_creation_params& mr_params)
{


    STATUS res  = STATUS_OK;
    
    res = _resources.create_resources(rdevice,
                                      con_params,
                                      qp_params,
                                      cq_hw_params,
                                      mr_params);
    RETURN_IF_FAILED_MSG(res, "Failed to create resources");

    _status = CONNECTION_STATUS_INITIALIZING;

    return STATUS_OK;
}

