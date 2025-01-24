# rdma_helpers
Libs, scripts and apps to help debug RDMA

To build the lib to trace QPC:
g++ -I/usr/include/infiniband -shared -fPIC qp_debug_lib.cpp -o libqpdebug.so

To link againt the Lib:

g++': g++ -o test_ar test_ar.cpp -L. -lqpdebug -libverbs -lmlx5