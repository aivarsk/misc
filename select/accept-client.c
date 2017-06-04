#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FDS_BITPERLONG (8 * sizeof(long))
#define FDS_LONGS(nr) (((nr) + FDS_BITPERLONG - 1) / FDS_BITPERLONG)
#define FDS_BYTES(nr) (FDS_LONGS(nr) * sizeof(long))

static int client(struct sockaddr_in *sin4) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  int one = 1;
  assert(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != -1);
  assert(fcntl(fd, F_SETFL, O_NONBLOCK) != -1);

  int n = connect(fd, (struct sockaddr *)sin4, sizeof(*sin4));
  assert(n != -1 || errno == EINPROGRESS);
  return fd;
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: %s <num clients> <host> <port>\n", argv[0]);
    return -1;
  }

  struct rlimit nfiles;
  assert(getrlimit(RLIMIT_NOFILE, &nfiles) != -1);
  nfiles.rlim_cur = nfiles.rlim_max;
  assert(setrlimit(RLIMIT_NOFILE, &nfiles) != -1);

  int nclients = atoi(argv[argc - 3]);

  struct sockaddr_in sin4;
  memset(&sin4, 0, sizeof(sin4));
  sin4.sin_family = AF_INET;
  sin4.sin_addr.s_addr = inet_addr(argv[argc - 2]);
  sin4.sin_port = htons(atoi(argv[argc - 1]));

  int maxfd = 0;

  fd_set *wfds_in = calloc(FDS_BYTES(nfiles.rlim_max), 1);
  fd_set *wfds_out = calloc(FDS_BYTES(nfiles.rlim_max), 1);

  for (int i = 0; i < nclients; i++) {
    int fd = client(&sin4);
    maxfd = fd > maxfd ? fd : maxfd;
    FD_SET(fd, wfds_in);
  }

  while (1) {
    memcpy(wfds_out, wfds_in, FDS_BYTES(maxfd));

    int nready = select(maxfd + 1, NULL, wfds_out, NULL, NULL);
    assert(nready != -1);

    unsigned long *wfds = (unsigned long *)wfds_out;
    for (unsigned i = 0; i < FDS_LONGS(maxfd) && nready > 0; i++, *wfds++) {
      unsigned long bits = *wfds;
      if (bits != 0) {

        unsigned long bit = 1;
        for (unsigned j = 0; j < FDS_BITPERLONG; j++, bit <<= 1) {

          if (bits & bit) {
            nready--;

            int fd = i * FDS_BITPERLONG + j;
            int err;
            socklen_t err_len = sizeof(err);
            assert(getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len) != -1);

            if (err == 0) {
              printf("%ld connected\n", time(NULL));
            } else {
              printf("%ld failed\n", time(NULL));
            }

            close(fd);
            FD_CLR(fd, wfds_in);

            int newfd = client(&sin4);
            FD_SET(newfd, wfds_in);
          }
        }
      }
    }
  }
}
