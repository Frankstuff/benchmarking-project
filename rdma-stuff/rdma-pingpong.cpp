#include <infiniband/verbs.h> //in order to use ibv_get_device_list, ibv_free_device_list
#include <cstdio>

//headers related to exchanging info before RDMA via TCP
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

//chat gave me some functions to exchange info

constexpr uint16_t TCP_CONTROL_PORT = 18515;

int create_server_connection()
{
    int listening_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (listening_socket == -1) {
        std::perror("socket");
        return -1;
    }

    int enable = 1;

    if (setsockopt(
            listening_socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &enable,
            sizeof(enable)
        ) == -1) {

        std::perror("setsockopt");
        close(listening_socket);
        return -1;
    }

    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(TCP_CONTROL_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
            listening_socket,
            reinterpret_cast<sockaddr *>(&server_address),
            sizeof(server_address)
        ) == -1) {

        std::perror("bind");
        close(listening_socket);
        return -1;
    }

    if (listen(listening_socket, 1) == -1) {
        std::perror("listen");
        close(listening_socket);
        return -1;
    }

    std::printf(
        "Waiting for client on TCP port %u...\n",
        TCP_CONTROL_PORT
    );

    int connected_socket = accept(
        listening_socket,
        nullptr,
        nullptr
    );

    if (connected_socket == -1) {
        std::perror("accept");
        close(listening_socket);
        return -1;
    }

    /*
     * We only need the connected socket now.
     * No additional clients are required for this benchmark.
     */
    close(listening_socket);

    std::printf("Client connected\n");

    return connected_socket;
}

int create_client_connection(const char *server_ip)
{
    int connected_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (connected_socket == -1) {
        std::perror("socket");
        return -1;
    }

    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(TCP_CONTROL_PORT);

    int conversion_result = inet_pton(
        AF_INET,
        server_ip,
        &server_address.sin_addr
    );

    if (conversion_result != 1) {
        if (conversion_result == 0) {
            std::fprintf(
                stderr,
                "Invalid IPv4 address: %s\n",
                server_ip
            );
        } else {
            std::perror("inet_pton");
        }

        close(connected_socket);
        return -1;
    }

    std::printf(
        "Connecting to %s:%u...\n",
        server_ip,
        TCP_CONTROL_PORT
    );

    if (connect(
            connected_socket,
            reinterpret_cast<sockaddr *>(&server_address),
            sizeof(server_address)
        ) == -1) {

        std::perror("connect");
        close(connected_socket);
        return -1;
    }

    std::printf("Connected to server\n");

    return connected_socket;
}


int main(argc, char *argv[]) {
    int buffer_size = 1024;//I should make this a macro but whatever
    //I will be using a particular set of settings
    struct ibv_device **rdma_devices = ibv_get_device_list(NULL);//returns a null terminated array of devices 
    //there are a couple error codes that I need to handle, but I just won't for now like EPERM  Permission denied.ENOSYS No kernel support for RDMA.ENOMEM Insufficient memory to complete the operation 
    struct ibv_context *pointer_to_device_context = ibv_open_device(*rdma_devices);//device comes from the list of devices . .  I am just using the first one because I don't expect other 
    //devices for now just the rxe one that i created in software, just copied this from the man pages

    //struct to be filled in that holds the device attributes is below
    struct ibv_device_attr device_attr; //declare
    
    int query = ibv_query_device(pointer_to_device_context, &device_attr);            //should have zero return value when successful

    std::printf("the port number is: %d\n", device_attr.phys_port_cnt);
    struct ibv_port_attr port_attr;   //this is getting filled in
 
    int query_port = ibv_query_port(pointer_to_device_context, 1, &port_attr);//we are assuming here that the RDMA port number that we want on our device  

    //we are gonna create a protected domain below . . . still need to fully understand protection domain. . . 

    struct ibv_pd *pd = ibv_alloc_pd(pointer_to_device_context); 
   
    char *buffer = (char *)malloc(buffer_size);
    //forgot i'm writing c++ so actually I don't need to write struct like i do in C (unless i have a typedef)
    ibv_mr *memory_region = ibv_reg_mr(pd, buffer, buffer_size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
  
    //the queue below has 16 entries 
    ibv_cq *completion_queue = ibv_create_cq(pointer_to_device_context, 16, nullptr, nullptr, 0); //once completion queue for sending and receiving
      
    ibv_qp_init_attr qp_init_attr{};
    qp_init_attr.send_cq = completion_queue;
    qp_init_attr.recv_cq = completion_queue;
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.cap.max_send_wr  = 16;
    qp_init_attr.cap.max_recv_wr  = 16;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1; 
    ibv_qp *queue_pair = ibv_create_qp(pd, &qp_init_attr);

    ibv_qp_attr attr{};//set the queue pairs attributes to be done with the bv_modify_qp command below
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = 1;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

    int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
    int result = ibv_modify_qp(queue_pair, &attr, flags);
    //the stuff above is apparently usually done as a entire function that moves the qp into an initialized state. . .
   
    int control_socket = -1;

    if (argc == 1) {
    /*
    * No server address was provided, so this process
    * acts as the server.
    */
        control_socket = create_server_connection();
    } else if (argc == 2) {
	    /*
	     * argv[1] is the server's IPv4 address, so this
	     * process acts as the client.
	     */
        control_socket = create_client_connection(argv[1]);
    } else {
        std::fprintf(stderr, "Usage:\n"
        "  Server: %s\n"
        "  Client: %s <server-ip>\n", argv[0], argv[0]);
        return 1;
    }
    if (control_socket == -1) {
        std::fprintf(stderr, "Could not establish TCP control connection\n");
	return 1;
    }
 
    close(control_socket);
    
    ibv_destroy_qp(queue_pair);
    ibv_destroy_cq(completion_queue);
    ibv_dereg_mr(memory_region);
    free(buffer);
    ibv_dealloc_pd(pd);
    ibv_close_device(pointer_to_device_context);


    return 0;
}
