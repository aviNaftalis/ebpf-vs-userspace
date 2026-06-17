// udp_sink <bind_ip> <port> <seconds>
//
// The userspace way to "drop" packets: bind a UDP socket and recvmmsg() them in
// a tight loop, throwing each away. Every packet still costs an sk_buff, a trip
// up the IP stack, a socket-queue enqueue, and a copy to userspace — so this is
// the slowest of the three. Prints packets received/sec (its real throughput;
// anything the kernel drops because we couldn't keep up is NOT counted).
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

#define BATCH 64
#define MAXLEN 2048

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s <bind_ip> <port> <seconds>\n", argv[0]);
		return 2;
	}
	const char *ip = argv[1];
	int port = atoi(argv[2]);
	double secs = atof(argv[3]);

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) { perror("socket"); return 1; }

	// give userspace its best shot: a big receive buffer
	int rcvbuf = 16 * 1024 * 1024;
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
		fprintf(stderr, "bad ip %s\n", ip);
		return 1;
	}
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return 1;
	}
	// don't block forever once the flood stops
	struct timeval tv = {0, 200000};
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	static char bufs[BATCH][MAXLEN];
	struct mmsghdr msgs[BATCH];
	struct iovec iov[BATCH];
	memset(msgs, 0, sizeof(msgs));
	for (int i = 0; i < BATCH; i++) {
		iov[i].iov_base = bufs[i];
		iov[i].iov_len = MAXLEN;
		msgs[i].msg_hdr.msg_iov = &iov[i];
		msgs[i].msg_hdr.msg_iovlen = 1;
	}

	double t0 = now_s();
	double end = t0 + secs;
	unsigned long got = 0;
	while (now_s() < end) {
		int n = recvmmsg(fd, msgs, BATCH, MSG_WAITFORONE, NULL);
		if (n > 0)
			got += n;
	}
	double dt = now_s() - t0;
	double pps = dt > 0 ? got / dt : 0;

	fprintf(stderr, "[sink] received=%lu pps=%.0f\n", got, pps);
	printf("%.0f\n", pps);
	close(fd);
	return 0;
}
