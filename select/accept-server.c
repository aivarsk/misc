#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static long long usec() {
  struct timeval now;
  gettimeofday(&now, NULL);
  return (long long)now.tv_sec * (long long)1000000 + (long long)now.tv_usec;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    return -1;
  }

  struct sockaddr_in sin4;
  socklen_t sin4_len = sizeof(sin4);

  memset(&sin4, 0, sizeof(sin4));
  sin4.sin_family = AF_INET;
  sin4.sin_addr.s_addr = INADDR_ANY;
  sin4.sin_port = htons(atoi(argv[argc - 1]));

  int server = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(server != -1);
  int one = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));

  assert(bind(server, (struct sockaddr *)&sin4, sin4_len) != -1);
  assert(listen(server, 1024) != -1);

  int flags = fcntl(server, F_GETFL, 0);
  assert(fcntl(server, F_SETFL, flags | O_NONBLOCK) != -1);

  fd_set rfds_in, rfds_out;
  FD_ZERO(&rfds_in);
  FD_SET(server, &rfds_in);

  long long tnow = 0, tprev = 0;
  while (1) {
    tnow = usec();
    if (tnow - tprev < 1000) {
      struct timespec spec;
      spec.tv_sec = 0;
      spec.tv_nsec = 1000 - (tnow - tprev);
      nanosleep(&spec, NULL);

    } else {
      tprev = tnow;
    }

    memcpy(&rfds_out, &rfds_in, server / 8 + 1);
    int nready = select(server + 1, &rfds_out, NULL, NULL, NULL);
    assert(nready != -1);
    if (nready > 0) {
      while (1) {
        int client = accept(server, (struct sockaddr *)&sin4, &sin4_len);
        assert(client != -1 || errno == EAGAIN || errno == EWOULDBLOCK);
        if (client == -1) {
          break;
        } else {
          close(client);
          printf("%ld accepted\n", time(NULL));
        }
      }
    }
  }

  return 0;
}
