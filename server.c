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
#define BACKLOG 10
#define MAX_EVENTS 10

int create_server_socket() {
  struct addrinfo hints, *res;
  int server_fd;
  int ecode;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP socket
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

  ecode = getaddrinfo(NULL, PORT, &hints, &res);
  if (ecode != 0) {
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ecode));
    exit(1);
  }

  // create socket
  server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  printf("created socket %d\n", server_fd);
  ecode = bind(server_fd, res->ai_addr, res->ai_addrlen);
  if (ecode == -1) {
    fprintf(stderr, "socket bind failed\n");
    exit(1);
  }
  printf("socket bound to port %s\n", PORT);

  int yes = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) ==
      -1) {
    fprintf(stderr, "socket cannot be reused!\n");
    exit(1);
  }

  return server_fd;
}

int main() {
  struct sockaddr_storage client_addr;
  socklen_t addr_size;
  int server_fd, client_fd;
  int ecode;

  server_fd = create_server_socket();
  listen(server_fd, BACKLOG);
  printf("socket listening to 127.0.0.1:%s\n", PORT);

  // setting up epoll instance
  int epoll_fd;
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    fprintf(stderr, "failed to create epoll instance\n");
    exit(1);
  }

  struct epoll_event ev, events[MAX_EVENTS];

  // adding server fd to epoll
  ev.events = EPOLLIN;
  ev.data.fd = server_fd;
  ecode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
  if (ecode == -1) {
    fprintf(stderr, "failed to add server fd to epoll\n");
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
      if (events[i].data.fd == server_fd) {
        addr_size = sizeof(client_addr);
        client_fd =
            accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
          fprintf(stderr, "failed to accept client socket\n");
          continue; // TODO: check if we have to exit here
        }
        printf("client connected %d\n", client_fd);

        // adding client fd to epoll
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        ecode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
        if (ecode == -1) {
          fprintf(stderr, "failed to add client fd to epoll: %d\n", client_fd);
          continue; // TODO: check if we have to exit here
        }
      } else {
        client_fd = events[i].data.fd;
        char buf[1024];
        int len, bytes_sent;

        memset(&buf, 0, sizeof(buf));
        len = recv(client_fd, &buf, 1024, 0);
        if (len == 0) { // socket closed from client side
          printf("client socket closed\n");
          close(client_fd);
          ecode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
          if (ecode == -1) {
            fprintf(stderr, "failed to delete client fd from epoll: %d\n",
                    client_fd);
          }
          continue;
        } 

        printf("<<< %s", buf);
        printf("received %d bytes from client\n", len);
        ecode = send(client_fd, &buf, strlen(buf), 0);
        if (ecode == -1) {
          fprintf(stderr, "failed to send buf\n");
          continue;
        }
        printf(">>> %s", buf);
        printf("sent: %s", buf);
      }
    }
  }
}
