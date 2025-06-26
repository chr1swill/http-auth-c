#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define REQUESTBUFFERMAX 1024

#define RBUFS_HASH(connfd) ((connfd) - 3)

#define err_exit(msg) do { perror((msg)); exit(EXIT_FAILURE); } while(0);

char *get_request_buffer(char *rbufs[REQUESTBUFFERMAX], int connfd)
{
  printf("connfd=%d\n", connfd);

  if (rbufs[RBUFS_HASH(connfd)] != NULL) return rbufs[RBUFS_HASH(connfd)];
  
  rbufs[RBUFS_HASH(connfd)] = malloc(sizeof(char) * REQUESTBUFFERMAX);
  if (rbufs[RBUFS_HASH(connfd)] == NULL) err_exit("malloc");

  return rbufs[RBUFS_HASH(connfd)];
}

int main()
{
  int connfd;
  char *str, *rbuf;
  char *rbufs[REQUESTBUFFERMAX] = {0};

  connfd = 4;
  
  printf("%ld\n", sizeof(rbufs));

  str = "this is a string in the array that i just stack allocated bc i am cool";

  rbufs[RBUFS_HASH(connfd)] = (char *)malloc(sizeof(char) * strlen(str));
  if (rbufs[RBUFS_HASH(connfd)] == NULL) err_exit("malloc");
  
  strncpy(rbufs[RBUFS_HASH(connfd)], str, strlen(str));

  rbuf = rbufs[RBUFS_HASH(connfd)];
  write(STDOUT_FILENO, rbuf, strlen(str));
  return 0;
}
