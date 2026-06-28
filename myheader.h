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
static inline uint64_t now_ns(void);
