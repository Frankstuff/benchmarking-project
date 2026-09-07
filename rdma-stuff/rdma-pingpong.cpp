/* this is what is left to be done . . .  
 1. Query this endpoint's local GID.

 2. Construct local connection information:
    - queue_pair->qp_num
    - local PSN
    - local GID

 3. Send local information through control_socket.

 4. Receive remote information through control_socket.

 5. Move local QP from INIT to RTR using remote information.

 6. Move local QP from RTR to RTS using local PSN.

 7. Post receive work requests.

 8. Synchronize both processes.

 9. Start RDMA SEND/RECV.



*/





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
constexpr uint8_t RDMA_PORT_NUMBER = 1;
constexpr int GID_INDEX = 1;


struct ConnectionData {
    uint32_t qp_number;
    uint32_t packet_sequence_number;
    ibv_gid gid;
};
struct ConnectionDataWire {
    uint32_t qp_number_network_order;
    uint32_t packet_sequence_number_network_order;
    uint8_t gid[16];
};
//let's just make sure the size is standard . . . obviously it should always be but if it isnt on a machine then we are cooked

static_assert(sizeof(ConnectionDataWire) == 24, "Unexpected ConnectionDataWire size");
//the functions below are just there to make sure that the data is sent even if send and receive fail (im pretty sure this is like 99% unnecessary because the messages i will be exchanging are extremely short

bool send_all(int socket_fd, const void *data, std::size_t length)
{
    const char *current =
        static_cast<const char *>(data);

    std::size_t bytes_sent = 0;

    while (bytes_sent < length) {
        ssize_t result = send(
            socket_fd,
            current + bytes_sent,
            length - bytes_sent,
            MSG_NOSIGNAL
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            std::perror("send");
            return false;
        }

        if (result == 0) {
            return false;
        }

        bytes_sent += static_cast<std::size_t>(result);
    }

    return true;
}

bool receive_all(int socket_fd, void *data, std::size_t length)
{
    char *current = static_cast<char *>(data);

    std::size_t bytes_received = 0;

    while (bytes_received < length) {
        ssize_t result = recv(
            socket_fd,
            current + bytes_received,
            length - bytes_received,
            0
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            std::perror("recv");
            return false;
        }

        if (result == 0) {
            std::fprintf(
                stderr,
                "Peer closed the TCP connection\n"
            );

            return false;
        }

        bytes_received += static_cast<std::size_t>(result);
    }

    return true;
}
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

bool exchange_connection_data(
    int control_socket,
    bool is_server,
    const ConnectionData &local_data,
    ConnectionData *remote_data
)
{
    /*
     * Convert our normal local representation into the exact
     * representation that we will transmit over TCP.
     */
    ConnectionDataWire local_wire_data{};

    local_wire_data.qp_number_network_order =
        htonl(local_data.qp_number);

    local_wire_data.packet_sequence_number_network_order =
        htonl(local_data.packet_sequence_number);

    std::memcpy(
        local_wire_data.gid,
        &local_data.gid,
        sizeof(local_wire_data.gid)
    );

    /*
     * This object will receive the bytes sent by the other VM.
     */
    ConnectionDataWire remote_wire_data{};

    bool exchange_succeeded;

    if (is_server) {
        /*
         * The server receives first and then responds.
         */
        exchange_succeeded =
            receive_all(
                control_socket,
                &remote_wire_data,
                sizeof(remote_wire_data)
            ) &&
            send_all(
                control_socket,
                &local_wire_data,
                sizeof(local_wire_data)
            );
    }
    else {
        /*
         * The client sends first and then waits for the response.
         */
        exchange_succeeded =
            send_all(
                control_socket,
                &local_wire_data,
                sizeof(local_wire_data)
            ) &&
            receive_all(
                control_socket,
                &remote_wire_data,
                sizeof(remote_wire_data)
            );
    }

    if (!exchange_succeeded) {
        return false;
    }

    /*
     * Convert the received wire representation into the
     * representation used by the rest of our program.
     */
    remote_data->qp_number =
        ntohl(remote_wire_data.qp_number_network_order);

    remote_data->packet_sequence_number =
        ntohl(
            remote_wire_data.packet_sequence_number_network_order
        );

    std::memcpy(
        &remote_data->gid,
        remote_wire_data.gid,
        sizeof(remote_wire_data.gid)
    );

    return true;
}

int main(int argc, char *argv[]) {
    int buffer_size = 1024;//I should make this a macro but whatever
    //I will be using a particular set of settings
    struct ibv_device **rdma_devices = ibv_get_device_list(NULL);//returns a null terminated array of devices 
    //there are a couple error codes that I need to handle, but I just won't for now like EPERM  Permission denied.ENOSYS No kernel support for RDMA.ENOMEM Insufficient memory to complete the operation 
    struct ibv_context *pointer_to_device_context = ibv_open_device(*rdma_devices);//device comes from the list of devices . .  I am just using the first one because I don't expect other 
    //devices for now just the rxe one that i created in software, just copied this from the man pages
    ibv_free_device_list(rdma_devices);
    //struct to be filled in that holds the device attributes is below
    struct ibv_device_attr device_attr; //declare
    
    int query = ibv_query_device(pointer_to_device_context, &device_attr);            //should have zero return value when successful

    std::printf("the port number is: %d\n", device_attr.phys_port_cnt);
    struct ibv_port_attr port_attr;   //this is getting filled in
 
    int query_port = ibv_query_port(pointer_to_device_context, RDMA_PORT_NUMBER, &port_attr);//we are assuming here that the RDMA port number that we want on our device  

    ibv_gid local_gid{};
    int query_gid_result = ibv_query_gid(pointer_to_device_context, RDMA_PORT_NUMBER, GID_INDEX, &local_gid);
    if (query_gid_result != 0) {
        std::perror("ibv_query_gid");
        return 1;
    }
    //we now have the LOCAL RDMA ADDRESS FOR THE RESPECTIVE PROCESS

    char local_gid_string[INET6_ADDRSTRLEN]{};

    const char *conversion_result = inet_ntop(AF_INET6, &local_gid, local_gid_string, sizeof(local_gid_string));

    if (conversion_result == nullptr) {
        std::perror("inet_ntop for GID");
        return 1;
    }

    std::printf("Local GID at index %d: %s\n", GID_INDEX, local_gid_string);





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
    if (result != 0) {
        std::fprintf(stderr, "could not change the qp state to init: %s\n", std::strerror(result));
        return 1;
    } 
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

    uint32_t local_psn; //these are packet sequence numbers since we are using a RC (reliable connection) these help missing duplicate and OOO packets get detected

    if (argc == 1) {
        local_psn = 0x654321;
    } else {
        local_psn = 0x123456;
    } 

    ConnectionData local_connection_data{};

    local_connection_data.qp_number = queue_pair->qp_num;
    local_connection_data.packet_sequence_number = local_psn;
    local_connection_data.gid = local_gid;

    
    std::printf(
    "%s local connection information:\n"
    "  QP number: %u\n"
    "  PSN:       0x%06x\n"
    "  GID:       %s\n", is_server ? "Server" : "Client", local_connection_data.qp_number, local_connection_data.packet_sequence_number, local_gid_string);
    //now the client and the server have established a tcp connection, but they need to exchange GIDs 
    //Global identifiers are used to recognize network ports on rdma adapters

    ConnectionData remote_connection_data{};

    bool exchange_succeeded = exchange_connection_data(control_socket, argc == 1, local_connection_data, &remote_connection_data);
   
     
    if (!exchange_succeeded) {
        std::fprintf(stderr, "Failed to exchange RDMA connection information\n");
        return 1;
    } 

    char remote_gid_string[INET6_ADDRSTRLEN]{};

    if (inet_ntop(AF_INET6, &remote_connection_data.gid, remote_gid_string, sizeof(remote_gid_string)) == nullptr) {
        std::perror("inet_ntop for remote GID");
        return 1;
    }

    //PRINT WHAT WE RECEIVED 
    std::printf(
    "%s received remote RDMA information:\n"
    "  Remote QP number: %u\n"
    "  Remote PSN:       0x%06x\n"
    "  Remote GID:       %s\n",
    is_server ? "Server" : "Client", remote_connection_data.qp_number, remote_connection_data.packet_sequence_number, remote_gid_string);
    
    
    close(control_socket);
    
    ibv_destroy_qp(queue_pair);
    ibv_destroy_cq(completion_queue);
    ibv_dereg_mr(memory_region);
    free(buffer);
    ibv_dealloc_pd(pd);
    ibv_close_device(pointer_to_device_context);


    return 0;
}
