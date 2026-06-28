#include <stdio.h>
#include <string.h> //memset
#include <netdb.h>//getaddrinfo
#include <sys/socket.h>//socket
#include <unistd.h>//close
#include <arpa/inet.h> //what is th
#include <cstdint>
#include <time.h>
#include <errno.h>
#include <cstdlib>
#define NUM_MESSAGES 1
#define BUF_SIZE 65536
#define INTERVAL_TIME 100

static inline uint64_t now_ns(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
