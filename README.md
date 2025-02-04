## rdma_helpers
Libs, scripts and apps to help debug RDMA


# QPC lib (prints everything you need to know about the QP)
To build the lib to trace QPC:
g++ -I/usr/include/infiniband -shared -fPIC qp_debug_lib.cpp -o libqpdebug.so

To link against the Lib:
g++ -o test_app test_app.cpp -L. -lqpdebug -libverbs -lmlx5


# rdma_profiler
The RDMA profiler is an app that would profile the network for you, meaning it will take the following timestamps:
  1. post timestamp
  2. doorbell
  3. CQE timestamp
  4. poll CQ
And will create the statistics of the WQEs based on breakdown latency.

[in the initial draft only the facilities are introduced]

This is very usufull for larger clusters where you should know which of the components are introducing latency, and a very strong debugging mechanism.

To build:
cd rdma_profiler
mkdir build
cmake ..
make




