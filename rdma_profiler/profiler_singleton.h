#pragma once

#include <chrono>
#include <unordered_map>
#include <mutex>

class rdma_profiler_singleton {
public:
    static rdma_profiler_singleton& instance() {
        static rdma_profiler_singleton instance;
        return instance;
    }

    void record_post_op(uint32_t qpn, uint32_t wqe_idx) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::high_resolution_clock::now();
        post_timestamps_[qpn][wqe_idx] = now;
    }

    void record_doorbell(uint32_t qpn, uint32_t wqe_idx) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::high_resolution_clock::now();
        doorbell_timestamps_[qpn][wqe_idx] = now;
    }

    void record_cqe_timestamp(uint32_t qpn, uint64_t wr_id, 
                            std::chrono::high_resolution_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        completion_timestamps_[qpn][wr_id] = ts;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        post_timestamps_.clear();
        doorbell_timestamps_.clear();
        completion_timestamps_.clear();
    }

private:
    rdma_profiler_singleton() = default;
    ~rdma_profiler_singleton() = default;
    rdma_profiler_singleton(const rdma_profiler_singleton&) = delete;
    rdma_profiler_singleton& operator=(const rdma_profiler_singleton&) = delete;

    std::mutex mutex_;
    std::unordered_map<uint32_t, 
        std::unordered_map<uint32_t, 
            std::chrono::high_resolution_clock::time_point>> post_timestamps_;
    std::unordered_map<uint32_t, 
        std::unordered_map<uint32_t, 
            std::chrono::high_resolution_clock::time_point>> doorbell_timestamps_;
    std::unordered_map<uint32_t, 
        std::unordered_map<uint64_t, 
            std::chrono::high_resolution_clock::time_point>> completion_timestamps_;
};
