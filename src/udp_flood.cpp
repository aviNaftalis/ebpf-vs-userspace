// udp_flood <dst_ip> <port> <payload_bytes> <seconds>
//
// Blast UDP packets at the destination as fast as one thread can, for <seconds>.
// We don't care if sends fail (ENOBUFS when the qdisc fills) — that's the point,
// we're trying to offer more load than the receiver can take.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

static double now_s()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	if (argc < 5) {
		fprintf(stderr, "usage: %s <dst_ip> <port> <payload_bytes> <seconds>\n", argv[0]);
		return 2;
	}
	const char *ip = argv[1];
	int port = atoi(argv[2]);
	int size = atoi(argv[3]);
	double secs = atof(argv[4]);
	if (size < 1) size = 1;
	if (size > 65000) size = 65000;

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) { perror("socket"); return 1; }

	struct sockaddr_in dst;
	memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &dst.sin_addr) != 1) {
		fprintf(stderr, "bad ip %s\n", ip);
		return 1;
	}
	// connect() lets us use send() (no per-call address copy) → faster loop.
	if (connect(fd, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
		perror("connect");
		return 1;
	}

	char *buf = (char *)calloc(size, 1);
	memcpy(buf, "FLOOD", size < 5 ? size : 5);

	double end = now_s() + secs;
	unsigned long sent = 0;
	while (1) {
		// amortize the clock_gettime over a batch of sends
		for (int i = 0; i < 2048; i++) {
			if (send(fd, buf, size, MSG_DONTWAIT) > 0)
				sent++;
		}
		if (now_s() >= end)
			break;
	}
	fprintf(stderr, "[flood] size=%d sent=%lu\n", size, sent);
	free(buf);
	close(fd);
	return 0;
}
