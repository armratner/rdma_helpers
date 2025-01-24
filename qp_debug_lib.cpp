#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdio>
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#include <arpa/inet.h>

#include "mlx5_ifc.h"  // The large IFC header
#ifndef DEVX_ST_SZ_BYTES
#define DEVX_ST_SZ_BYTES(typ) (sizeof(struct mlx5_ifc_##typ##_bits) / 8)
#endif

#ifndef DEVX_SET
#define DEVX_SET(typ, p, fld, v) \
  do { \
    _devx_set((p), (v), __devx_bit_off(typ, fld), __devx_bit_sz(typ, fld)); \
  } while (0)
#endif

#ifndef DEVX_GET
#define DEVX_GET(typ, p, fld) \
  _devx_get((p), __devx_bit_off(typ, fld), __devx_bit_sz(typ, fld))
#endif

#ifndef MLX5_QP_ST_RC
#define MLX5_QP_ST_RC 0x0
#endif

#ifndef MLX5_QP_ST_UC
#define MLX5_QP_ST_UC 0x1
#endif

#ifndef MLX5_QP_ST_UD
#define MLX5_QP_ST_UD 0x2
#endif

// ------------------------------------------------------------------
static inline void print_if_error(int ret, const char *prefix)
{
    if (ret != 0) {
        std::cerr << prefix << ": failed with error = " << ret << std::endl;
    }
}

static void print_section_header(const char* section_name) {
    std::cout << "\n=== " << section_name << " ===" << std::endl;
}

static void print_hex_field(const char* field_name, uint32_t value, int width = 0) {
    std::cout << std::left << std::setw(28) << field_name 
              << "= 0x" << std::hex << std::setfill(' ') << std::setw(width) 
              << value << std::dec << std::endl;
}

static void print_dec_field(const char* field_name, uint32_t value) {
    std::cout << std::left << std::setw(28) << field_name
              << std::setfill(' ')
              << "= " << value << std::endl;
}

/**
 * queryAndPrintQpProperties
 *
 * 1. Initializes an mlx5dv_qp from the given ibv_qp.
 * 2. Prepares a DevX QUERY_QP command input.
 * 3. Issues the command via mlx5dv_devx_qp_query.
 * 4. Parses a few fields from the returned blob and prints them out.
 *
 * \param qp       [in] IB Verbs QP to query (must be DevX-bas=ed)
 */
int queryAndPrintQpProperties(struct ibv_qp *qp)
{
    // 1) Translate ibv_qp -> mlx5dv_qp
    struct mlx5dv_qp dv_qp;
    memset(&dv_qp, 0, sizeof(dv_qp));

    struct mlx5dv_obj obj;
    memset(&obj, 0, sizeof(obj));
    obj.qp.in  = qp;
    obj.qp.out = &dv_qp;

    int rc = mlx5dv_init_obj(&obj, MLX5DV_OBJ_QP);
    if (rc) {
        std::cerr << "mlx5dv_init_obj(qp) failed: rc = " << rc << std::endl;
        return rc;
    }

    uint32_t qpn = dv_qp.sqn;

    uint8_t in[DEVX_ST_SZ_BYTES(query_qp_in)]   = {0};
    uint8_t out[DEVX_ST_SZ_BYTES(query_qp_out)] = {0};

    DEVX_SET(query_qp_in, in, opcode, MLX5_CMD_OP_QUERY_QP);
    DEVX_SET(query_qp_in, in, qpn, qp->qp_num);

    rc = mlx5dv_devx_qp_query(qp, in, sizeof(in), out, sizeof(out));
    if (rc != 0) {
        print_if_error(rc, "mlx5dv_devx_qp_query");
        return rc;
    }

    uint32_t qpc_state = DEVX_GET(query_qp_out, out, qpc.state);
    uint32_t qpc_st    = DEVX_GET(query_qp_out, out, qpc.st);    // QP transport type
    uint32_t qpc_pm_state = DEVX_GET(query_qp_out, out, qpc.pm_state);

    print_section_header("QP Query Results (from DevX)");
    print_hex_field("QP State (qpc.state)", qpc_state);
    print_hex_field("QP Transport (qpc.st)", qpc_st);
    print_hex_field("QP PM State (qpc.pm_state)", qpc_pm_state);

    uint32_t pd_num      = DEVX_GET(query_qp_out, out, qpc.pd);
    uint32_t uar_page    = DEVX_GET(query_qp_out, out, qpc.uar_page);
    uint32_t mtu         = DEVX_GET(query_qp_out, out, qpc.mtu);
    uint32_t retry_count = DEVX_GET(query_qp_out, out, qpc.retry_count);
    

    print_dec_field("Protection Domain (qpc.pd)", pd_num);
    print_dec_field("UAR page (qpc.uar_page)", uar_page);
    print_dec_field("MTU (qpc.mtu)", mtu);
    print_dec_field("Retry Count (qpc.retry_count)", retry_count);

    // Print Peer IP Address
    print_section_header("Remote Connection Info");
    
    // For RoCEv2, IPv4 is in the last 4 bytes of rgid_rip
    // Access each rgid_rip byte directly without loops
    uint32_t ipv4_addr = (DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[12]) << 24) |
                         (DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[13]) << 16) |
                         (DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[14]) << 8)  |
                         (DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[15]));

    print_hex_field("Raw IP value", ipv4_addr, 8);

    // Print IP address in standard format
    std::cout << std::left << std::setw(28) << "Remote IP Address"
              << "= " << ((ipv4_addr >> 24) & 0xFF) << "."
                      << ((ipv4_addr >> 16) & 0xFF) << "."
                      << ((ipv4_addr >> 8) & 0xFF) << "."
                      << (ipv4_addr & 0xFF) << std::endl;

    uint32_t gid_bytes[16] = {
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[0]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[1]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[2]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[3]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[4]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[5]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[6]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[7]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[8]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[9]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[10]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[11]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[12]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[13]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[14]),
        DEVX_GET(query_qp_out, out, qpc.primary_address_path.rgid_rip[15])
    };

    // Now we can safely print the GID using a regular loop since we have the values
    std::cout << std::left << std::setw(28) << "Remote GID" << "= ";
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << std::setfill(' ') << std::setw(2) << gid_bytes[i];
        if (i < 15) std::cout << ":";
    }
    std::cout << std::dec << std::endl;

    uint32_t rmac_47_32 = DEVX_GET(query_qp_out, out, qpc.primary_address_path.rmac_47_32);
    uint32_t rmac_31_0 = DEVX_GET(query_qp_out, out, qpc.primary_address_path.rmac_31_0);
    std::cout << std::left << std::setw(28) << "Remote MAC Address" 
              << "= " << std::hex << std::setfill(' ') << std::setw(4) 
              << rmac_47_32 << ":" << std::setw(8) << rmac_31_0 << std::dec << std::endl;

    if (qpc_st == MLX5_QP_ST_RC || qpc_st == MLX5_QP_ST_UC || qpc_st == MLX5_QP_ST_UD) {
        print_dec_field("Remote UDP Port", 
            DEVX_GET(query_qp_out, out, qpc.primary_address_path.udp_sport));
    }

    print_section_header("QoS and Flow Control");
    print_dec_field("ECN", DEVX_GET(query_qp_out, out, qpc.primary_address_path.ecn));
    print_dec_field("DSCP", DEVX_GET(query_qp_out, out, qpc.primary_address_path.dscp));
    print_dec_field("Flow Label", DEVX_GET(query_qp_out, out, qpc.primary_address_path.flow_label));
    print_dec_field("Traffic Class", DEVX_GET(query_qp_out, out, qpc.primary_address_path.tclass));
    print_dec_field("ETH Priority", DEVX_GET(query_qp_out, out, qpc.primary_address_path.eth_prio));
    print_dec_field("Hop Limit", DEVX_GET(query_qp_out, out, qpc.primary_address_path.hop_limit));

    print_section_header("Ordering and Data Path");
    print_dec_field("Data In Order", DEVX_GET(query_qp_out, out, qpc.data_in_order));
    print_dec_field("End Padding Mode", DEVX_GET(query_qp_out, out, qpc.end_padding_mode));
    print_dec_field("WQ Signature", DEVX_GET(query_qp_out, out, qpc.wq_signature));
    print_dec_field("CD Master", DEVX_GET(query_qp_out, out, qpc.cd_master));
    print_dec_field("CD Slave Send", DEVX_GET(query_qp_out, out, qpc.cd_slave_send));
    print_dec_field("CD Slave Receive", DEVX_GET(query_qp_out, out, qpc.cd_slave_receive));

    print_section_header("Error Handling");
    print_dec_field("Min RNR NAK", DEVX_GET(query_qp_out, out, qpc.min_rnr_nak));
    print_dec_field("FRE (Fast Retry Enable)", DEVX_GET(query_qp_out, out, qpc.fre));

    // Additional QPC fields
    print_section_header("State and Control");
    print_hex_field("Lag TX Port Affinity", DEVX_GET(query_qp_out, out, qpc.lag_tx_port_affinity));
    print_dec_field("Isolate VL TC", DEVX_GET(query_qp_out, out, qpc.isolate_vl_tc));
    print_dec_field("E2E Credit Mode", DEVX_GET(query_qp_out, out, qpc.req_e2e_credit_mode));
    print_dec_field("Offload Type", DEVX_GET(query_qp_out, out, qpc.offload_type));
    print_dec_field("End Padding Mode", DEVX_GET(query_qp_out, out, qpc.end_padding_mode));

    print_section_header("Queue Properties");
    print_dec_field("WQ Signature", DEVX_GET(query_qp_out, out, qpc.wq_signature));
    print_dec_field("Block LB MC", DEVX_GET(query_qp_out, out, qpc.block_lb_mc));
    print_dec_field("Atomic Like Write Enable", DEVX_GET(query_qp_out, out, qpc.atomic_like_write_en));
    print_dec_field("Latency Sensitive", DEVX_GET(query_qp_out, out, qpc.latency_sensitive));
    print_dec_field("Drain Sigerr", DEVX_GET(query_qp_out, out, qpc.drain_sigerr));
    
    print_section_header("Queue Configuration");
    print_dec_field("Log Msg Max", DEVX_GET(query_qp_out, out, qpc.log_msg_max));
    print_dec_field("Log RQ Size", DEVX_GET(query_qp_out, out, qpc.log_rq_size));
    print_dec_field("Log RQ Stride", DEVX_GET(query_qp_out, out, qpc.log_rq_stride));
    print_dec_field("No SQ", DEVX_GET(query_qp_out, out, qpc.no_sq));
    print_dec_field("Log SQ Size", DEVX_GET(query_qp_out, out, qpc.log_sq_size));
    
    print_section_header("System Information");
    print_dec_field("TS Format", DEVX_GET(query_qp_out, out, qpc.ts_format));
    print_dec_field("Data In Order", DEVX_GET(query_qp_out, out, qpc.data_in_order));
    print_dec_field("RLKY", DEVX_GET(query_qp_out, out, qpc.rlky));
    print_dec_field("Counter Set ID", DEVX_GET(query_qp_out, out, qpc.counter_set_id));
    print_dec_field("User Index", DEVX_GET(query_qp_out, out, qpc.user_index));
    print_dec_field("Log Page Size", DEVX_GET(query_qp_out, out, qpc.log_page_size));
    print_dec_field("Remote QPN", DEVX_GET(query_qp_out, out, qpc.remote_qpn));
    
    print_section_header("Retry and Timeout Parameters");
    print_dec_field("Log Ack Req Freq", DEVX_GET(query_qp_out, out, qpc.log_ack_req_freq));
    print_dec_field("Log SRA Max", DEVX_GET(query_qp_out, out, qpc.log_sra_max));
    print_dec_field("RNR Retry", DEVX_GET(query_qp_out, out, qpc.rnr_retry));
    print_dec_field("Cur RNR Retry", DEVX_GET(query_qp_out, out, qpc.cur_rnr_retry));
    print_dec_field("Cur Retry Count", DEVX_GET(query_qp_out, out, qpc.cur_retry_count));
    
    print_section_header("Sequence Numbers");
    print_hex_field("Next Send PSN", DEVX_GET(query_qp_out, out, qpc.next_send_psn));
    print_hex_field("Last Acked PSN", DEVX_GET(query_qp_out, out, qpc.last_acked_psn));
    print_hex_field("SSN", DEVX_GET(query_qp_out, out, qpc.ssn));
    print_hex_field("Next Rcv PSN", DEVX_GET(query_qp_out, out, qpc.next_rcv_psn));
    
    print_section_header("Queue Numbers");
    print_dec_field("CQN SND", DEVX_GET(query_qp_out, out, qpc.cqn_snd));
    print_dec_field("CQN RCV", DEVX_GET(query_qp_out, out, qpc.cqn_rcv));
    print_dec_field("DETH SQPN", DEVX_GET(query_qp_out, out, qpc.deth_sqpn));
    print_dec_field("SRQN RMPN XRQN", DEVX_GET(query_qp_out, out, qpc.srqn_rmpn_xrqn));
    
    print_section_header("Operation Capabilities");
    print_dec_field("Atomic Mode", DEVX_GET(query_qp_out, out, qpc.atomic_mode));
    print_dec_field("RRE", DEVX_GET(query_qp_out, out, qpc.rre));
    print_dec_field("RWE", DEVX_GET(query_qp_out, out, qpc.rwe));
    print_dec_field("RAE", DEVX_GET(query_qp_out, out, qpc.rae));
    
    print_section_header("Performance Counters");
    print_dec_field("HW SQ WQEBB Counter", DEVX_GET(query_qp_out, out, qpc.hw_sq_wqebb_counter));
    print_dec_field("SW SQ WQEBB Counter", DEVX_GET(query_qp_out, out, qpc.sw_sq_wqebb_counter));
    print_dec_field("HW RQ Counter", DEVX_GET(query_qp_out, out, qpc.hw_rq_counter));
    print_dec_field("SW RQ Counter", DEVX_GET(query_qp_out, out, qpc.sw_rq_counter));

    std::cout << std::string(47, '=') << std::endl;
    return 0;
}

/**
 * Example "library" function:
 * Takes an ibv_qp pointer, queries and prints debugging info. 
 */
extern "C" int debug_print_ibv_qp(struct ibv_qp *qp)
{
    if (!qp) {
        std::cerr << "QP pointer is NULL.\n";
        return -1;
    }
    return queryAndPrintQpProperties(qp);
}