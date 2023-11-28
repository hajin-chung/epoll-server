#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "8848"
#define MAX_EVENTS 10

int create_client_socket(char *addr, char *port) {
  struct addrinfo hints, *res, *p;
  int fd;
  int ecode;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP socket

  ecode = getaddrinfo(addr, port, &hints, &res);
  if (ecode != 0) {
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ecode));
    exit(1);
  }

  for (p = res; p != NULL; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd == -1) {
      fprintf(stderr, "failed to create socket\n");
      continue;
    }

    ecode = connect(fd, p->ai_addr, p->ai_addrlen);
    if (ecode == -1) {
      fprintf(stderr, "failed to connect socket\n");
      continue;
    }

    break;
  }
  if (p == NULL) {
    fprintf(stderr, "failed to connet\n");
    exit(1);
  }

  return fd;
}

int main() {
  int client_fd = create_client_socket("127.0.0.1", "8848");
  int ecode;
  int epoll_fd;
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    fprintf(stderr, "failed to create epoll instance\n");
    exit(1);
  }

  struct epoll_event ev, events[MAX_EVENTS];

  // add client fd to epoll
  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = client_fd;
  ecode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
  if (ecode == -1) {
    fprintf(stderr, "failed to add server fd to epoll\n");
    exit(1);
  }

  // add stdin to epoll
  ev.events = EPOLLIN;
  ev.data.fd = 0; // stdin
  ecode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &ev);
  if (ecode == -1) {
    fprintf(stderr, "failed to add stdin to epoll\n");
    exit(1);
  }

  int nfds, i;
  while (1) {
    nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); // last arg is timeout
    if (nfds == -1) {
      fprintf(stderr, "epoll failed to wait events\n");
      exit(1);
    }

    for (i = 0; i < nfds; i++) {
      if (events[i].data.fd == client_fd) {
        char buf[1024];
        int len;
        len = recv(client_fd, &buf, 1024, 0);
        if (len == 0) { // socket closed from server side
          printf("server socket closed\n");
          close(client_fd);
          ecode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
          if (ecode == -1) {
            fprintf(stderr, "failed to delete client fd from epoll: %d\n",
                    client_fd);
          }
          exit(1);
        } else {
          printf(">>> %s", buf);
        }
      } else if(events[i].data.fd == 0) {
        char buf[1024];
        int len, bytes_sent;

        memset(&buf, 0, sizeof(buf));
        len = read(0, buf, 1024);
        if (len == 0) { // stdin closed?? this is bad!
          printf("stdin closed\n");
          exit(1);
        } 

        printf("<<< %s", buf);
        ecode = send(client_fd, &buf, strlen(buf), 0);
        if (ecode == -1) {
          fprintf(stderr, "failed to send buf\n");
          continue;
        }
      }
    }
  }
  
  return 0;
}
