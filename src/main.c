#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define PORT "8080"
#define BACKLOG 10

static inline
int get_listener_socket()
{
  int rv, socketfd;
  struct addrinfo hints, *result, *rp;

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_addr = NULL;
  hints.ai_canonname = NULL;
  hints.ai_next = NULL;

  rv = getaddrinfo(NULL, PORT, &hints, &result);
  if (rv != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return(-1);
  }

  rp = result;
  for (;rp != NULL; rp = rp->ai_next)
  {
    socketfd = socket(rp->ai_family , rp->ai_socktype, rp->ai_protocol);
    if (socketfd == -1)
    {
      perror("socket");
      continue;
    }

    if ((setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR,
         &(int){1}, sizeof(int)) == -1))
    {
      perror("setsockopt");
      close(socketfd);
      continue;
    }

    if ((bind(socketfd, rp->ai_addr, rp->ai_addrlen)) == -1)
    {
      perror("bind");
      close(socketfd);
      continue;
    }

    if ((listen(socketfd, BACKLOG)) == -1)
    {
      perror("listen");
      close(socketfd);
      continue;
    } 

    break;
  }

  freeaddrinfo(result);

  if (rp == NULL)
  {
    fprintf(stderr, "failed to create listener socket\n");
    return(-1);
  } 

  return socketfd;
}

int
main()
{
  int sockfd;

  sockfd = get_listener_socket();
  if (sockfd == -1)
  {
    fprintf(stderr, "get_listener_socket: failed to get socket\n");
    return(1);
  }
  
  printf("hello my socket %d\n", sockfd);
  return(0);
}
