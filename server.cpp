#include "myheader.h"

//stuff


//int socket(int domain, int type, int protocol);


int main(void) {
	//i want the string to the IP of this VM so i can reach it from the other VM
	struct sockaddr_in details = {0};
       	details.sin_family = AF_INET;
	details.sin_port = htons(8080);
	details.sin_addr.s_addr = htonl(INADDR_ANY);
	int my_socket = socket(AF_INET, SOCK_STREAM, 0);
	bind(my_socket, (struct sockaddr_in *)&details, sizeof(details));
	listen(my_socket, 16);
	int curr = 0;//use this variable to index into a list that holds all of the buffers so i can make sure the tensors i send over are correct
	//now that i think about it let's malloc an array of pointers to heap regions of size BUF_SIZE so we can fill them in as we go
	char **buf = malloc(sizeof(void *) * NUM_MESSAGES);	
	for (int i = 0; i < NUM_MESSAGES; i++) {
		//put them into the buffer and stuff
		buf[i] = malloc(BUF_SIZE);	
	}
	char ack[1] = 'a';	
	while (1) {
		int acc = accept(my_socket, NULL, NULL);
		if (acc < 0) {
			continue;
		}
		ssize_t n;
		size_t tot = 0;
		//read below blocks till woken
		//lowkey if we want to speedup the process of receiving the message shouldn't we 
		while ((n = recv(acc, tot + buf[curr], BUF_SIZE - tot)) > 0) {//identical to read
			tot += n;
			//not sure what to do here probably copy them into the big buffer but that's counterproductive should just be placed there to begin with . . . 
		}
		send(acc, ack, 1, 0);
		curr++;
		close(acc);
		if (curr == NUM_MESSAGES) {
			break;
		}	
	}
	for (int i = 0; i < NUM_MESSAGES; i++) {
		free(buf[i]);
	}
	free(buf);
}
