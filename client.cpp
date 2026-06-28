#include "myheader.h"
int main(void) {
	//i forgot what include statements i need on the top like #include <thing> or whatever
	//
	struct addrinfo hints = {0};//idk what hints to use lowkey
	hints.ai_family = AF_UNSPEC;
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
		
	close(fd);
	return 0;	
}
