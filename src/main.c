#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#define PORT "8080"

int
main(void)
{
  int rv;
  struct addrinfo hints, *res, *p;

  memset(hints, 0, sizeof(hints));

  hints.ai_flags = ;
  hints.ai_family = ;
  hints.ai_socktype = ;
  hints.ai_protocol = ;
  hints.ai_addrlen = ;
  hints.ai_addr = ;
  hints.ai_canonname = ;
  hints.ai_next = ;

  getaddrinfo(NULL, PORT, &hints, &res);
  
  printf("hello world\n");
  return(0);
}
