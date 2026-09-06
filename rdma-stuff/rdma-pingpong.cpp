#include <infiniband/verbs.h> //in order to use ibv_get_device_list, ibv_free_device_list
#include <cstdio>

int main(argc, char *argv[]) {
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
    
    //ok now that we have a protected domain we need to create a memory region in it
    struct ibv_mr *pointer_to_memory =  
    
    int dealoc = ibv_dealloc_pd(pd);//returns 0 on succeess    fails if any other resources are affiliated with this pd

    
    //free the device list we generated maybe i should free it earlier but whatever
    ibv_free_device_list(rdma_devices); 
    return 0;
}
