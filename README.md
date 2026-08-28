# Benchmarking-different-tensor-copy-mechanisms
Benchmarking different tensor copy mechanisms. I will use NIXL, io_uring, RDMA, and standard TCP techniques.

#Obviously we can just shell into the vm

launch my VM's with the command below 
multipass launch \
  --name dev \
  --cpus 4 \
  --memory 8G \
  --disk 30G \
  --cloud-init cloud-init.yaml


#Mounting it 

multipass mount \
    "$PWD" \
    dev:/home/ubuntu/project
