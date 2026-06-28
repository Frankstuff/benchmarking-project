#include "myheader.h"
ssize_t send_all(int fd, char *buf, int size) {
	ssize_t i = 0;
	ssize_t n;
	while (i < size) {
		n = send(fd, buf + i, size - i, 0);
		if (n < 0) {
			if (n == EINTR) {
				continue;//the eintr is there to handle the case where a signal interrupted send
			}
			return -1;
		}
		i += n;	
	}
	return i;
}
int main(void) {
	//i forgot what include statements i need on the top like #include <thing> or whatever
	
	struct addrinfo hints = {};//idk what hints to use lowkey
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;//TCP   could be changed to UDP with DGRAM
	struct addrinfo *res;
	struct addrinfo *p;
	int rv = getaddrinfo("localhost", "8080", &hints, &res);
	if (0 == rv) {//forgot to fill in the function arguments
		//we are gucci to proceed	
	} else {
		fprintf(stderr, "getaddrinfo, %s", gai_strerror(rv));
		//non-zero return code indicates error	
		return 1;//error something needs to happen here . . . i think this may mean that we missed out on getting a resolution tothe hostname
	}
	// yeah idk what type get returns above i know its a pointer . . . but a pointer to what also idk how to make the for loop
	int fd = -1;	
	for (p = res; p != NULL; p = p->ai_next) {//loop over the different connection and bam
		fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (fd == -1) {
			continue;
		}
		if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(fd);
		} else {
			break;//connection made succesfully
		}
		//connected . . . i forgot something at the end here	
	}

	if (p == NULL) {
		//no connection was made. Hence this must indicate that
		fprintf(stderr, "failed to make the connection, very sad\n");
		freeaddrinfo(res);//free the struct since it will be no longer needed
		return 1;
	}

	struct sockaddr_in thing = *((struct sockaddr_in *)(p->ai_addr));
	freeaddrinfo(res);//free the struct since it will be no longer needed
	//fd should now be gauranteed to be connected to the socket lowkey
	
	//lets print out the IP
	char ip4[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(thing.sin_addr), ip4, INET_ADDRSTRLEN);
	printf("the ip adress is: %s\n", ip4);//i hope this works lowkey	
	//now that we are connected, let's send out our stuff
	char **buf = (char **)malloc(NUM_MESSAGES * sizeof(char *));
	for (int i = 0; i < NUM_MESSAGES; i++) {
		//make the messages so we can send them on demand
		buf[i] = (char *)malloc(BUF_SIZE);
	}
	//now we must design a spinning loop that will accurately spin and send messages at the intervals we want . . .
	uint64_t start = now_ns();
	for (int i = 0; i < NUM_MESSAGES; i++) {
		char ackbuf;
		uint64_t curr = start + (i + 1) * INTERVAL_TIME;
		while (now_ns() < curr) {
			//empty spinning wasting CPU but the most accurate
		}
		//send everything
		send_all(fd, buf[i], BUF_SIZE);
		recv(fd, &ackbuf, 1, 0);
	        printf("%c\n", ackbuf);	
	}	
	for (int i = 0; i < NUM_MESSAGES; i++) {
		free(buf[i]);
	}	
	free(buf);	
	close(fd);
	return 0;	
}
