#pragma once
#include <chrono>
#include <map>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

template<typename time_point = std::chrono::high_resolution_clock::time_point>
struct rdma_op_timestamps {
    time_point post_op;       // When post operation was called
    time_point doorbell;      // After doorbell ring
    time_point cqe_timestamp; // CQE timestamp
    time_point poll_cq;       // When event was polled
    uint64_t wr_id;          // Work request ID
};

template<typename clock = std::chrono::high_resolution_clock>
class rdma_profiler {
public:
    using time_point = typename clock::time_point;
    using op_timestamps = rdma_op_timestamps<time_point>;

    void record_post_op(uint32_t qp_num, uint64_t wr_id) {
        auto& timestamps = get_or_create_timestamp(qp_num, wr_id);
        timestamps.post_op = clock::now();
    }

    void record_doorbell(uint32_t qp_num, uint64_t wr_id) {
        auto& timestamps = get_or_create_timestamp(qp_num, wr_id);
        timestamps.doorbell = clock::now();
    }

    void record_cqe_timestamp(uint32_t qp_num, uint64_t wr_id, time_point timestamp) {
        auto& timestamps = get_or_create_timestamp(qp_num, wr_id);
        timestamps.cqe_timestamp = timestamp;
    }

    void record_poll_cq(uint32_t qp_num, uint64_t wr_id) {
        auto& timestamps = get_or_create_timestamp(qp_num, wr_id);
        timestamps.poll_cq = clock::now();
    }

    struct latency_stats {
        double post_to_doorbell;  // Time between post and doorbell
        double doorbell_to_cqe;   // Time between doorbell and CQE
        double cqe_to_poll;       // Time between CQE and poll
        double total_latency;     // Total operation latency
    };

    latency_stats analyze_latency(uint32_t qp_num, uint64_t wr_id) {
        const auto& ts = m_timestamps[qp_num][wr_id];
        latency_stats stats;
        
        stats.post_to_doorbell = std::chrono::duration<double, std::micro>(
            ts.doorbell - ts.post_op).count();
        stats.doorbell_to_cqe = std::chrono::duration<double, std::micro>(
            ts.cqe_timestamp - ts.doorbell).count();
        stats.cqe_to_poll = std::chrono::duration<double, std::micro>(
            ts.poll_cq - ts.cqe_timestamp).count();
        stats.total_latency = std::chrono::duration<double, std::micro>(
            ts.poll_cq - ts.post_op).count();
        
        return stats;
    }

    struct aggregate_stats {
        double avg_post_to_doorbell;
        double avg_doorbell_to_cqe;
        double avg_cqe_to_poll;
        double avg_total_latency;
        double min_total_latency;
        double max_total_latency;
        double std_dev_latency;
        size_t sample_count;
    };

    aggregate_stats analyze_qp_stats(uint32_t qp_num) {
        aggregate_stats stats = {};
        auto qp_iter = m_timestamps.find(qp_num);
        if (qp_iter == m_timestamps.end() || qp_iter->second.empty()) {
            return stats;
        }

        std::vector<double> latencies;
        latencies.reserve(qp_iter->second.size());

        for (const auto& [wr_id, ts] : qp_iter->second) {
            auto single_stats = analyze_latency(qp_num, wr_id);
            stats.avg_post_to_doorbell += single_stats.post_to_doorbell;
            stats.avg_doorbell_to_cqe += single_stats.doorbell_to_cqe;
            stats.avg_cqe_to_poll += single_stats.cqe_to_poll;
            stats.avg_total_latency += single_stats.total_latency;
            latencies.push_back(single_stats.total_latency);
        }

        stats.sample_count = qp_iter->second.size();
        double n = static_cast<double>(stats.sample_count);
        
        // Calculate averages
        stats.avg_post_to_doorbell /= n;
        stats.avg_doorbell_to_cqe /= n;
        stats.avg_cqe_to_poll /= n;
        stats.avg_total_latency /= n;

        // Calculate min/max and standard deviation
        stats.min_total_latency = *std::min_element(latencies.begin(), latencies.end());
        stats.max_total_latency = *std::max_element(latencies.begin(), latencies.end());

        // Calculate standard deviation
        double variance = 0.0;
        for (double latency : latencies) {
            double diff = latency - stats.avg_total_latency;
            variance += diff * diff;
        }
        stats.std_dev_latency = std::sqrt(variance / n);

        return stats;
    }

private:
    op_timestamps& get_or_create_timestamp(uint32_t qp_num, uint64_t wr_id) {
        return m_timestamps[qp_num][wr_id];
    }

    std::map<uint32_t, std::map<uint64_t, op_timestamps>> m_timestamps;
};
