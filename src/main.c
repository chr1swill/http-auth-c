#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "picohttpparser.h"

#define PORT "8080"
#define BACKLOG 10
#define PFDSMAX 1024
#define REQUESTBUFFERMAX 1024
// since this for a server we know that we will never need a
// request buffer for fd 0, 1, 2 (STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO)
// so why not make a function that will translate the connfd to make
// the first connfd (#3) fill spot number 0 or at least 1
// JUST A THOUGHT cause if we really have PFDSMAX fds in our array we will
// run out of space by 3 which is not ideal
#define PHP_HASHIDX(connfd, sockfd) (((connfd) - (sockfd)) - 1)
#define client_idx() PHP_HASHIDX(connfd, sockfd)

#define err_exit(msg) \
do { perror((msg)); exit(EXIT_FAILURE); } while(0); \

static char php_buf[PFDSMAX][REQUESTBUFFERMAX] = {0};
static size_t php_buflen[PFDSMAX] = {0};
static const char *php_method[PFDSMAX] = {0};
static size_t php_methodlen[PFDSMAX] = {0};
static const char *php_path[PFDSMAX] = {0};
static size_t php_pathlen[PFDSMAX] = {0};

#define PHP_NUM_HEADERS  8
static size_t php_num_headers[PFDSMAX] = {PHP_NUM_HEADERS};
static struct phr_header php_headers[PFDSMAX][PHP_NUM_HEADERS] = {0};

static int php_minor_version[PFDSMAX] = {0};
static size_t php_prevbuflen[PFDSMAX] = {0};

static inline
int get_non_blocking_listener()
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

  if ((fcntl(socketfd, F_SETFL, O_NONBLOCK)) == -1)
  {
    perror("fcntl: O_NONBLOCK");
    close(socketfd);
    return(-1);
  }

  return socketfd;
}

static inline
int pollfd_add(nfds_t *nfds, struct pollfd *pfds, int connfd, int events)
{
  /* returns the idx of new member or -1 if error */
  int j;

  if ((*nfds) >= PFDSMAX) return(-1);

  j = 0;
  for (; j < PFDSMAX; ++j)
  {
    if (pfds[j].fd != -1) continue;

    pfds[j].fd = connfd;
    pfds[j].events = events;
    pfds[j].revents = -1;
    
    ++(*nfds);

    return(j);
  }
  
  return(-1);
}

int main()
{
  ssize_t n;
  nfds_t nfds, i;
  int sockfd, connfd, ready;
  struct pollfd pfds[PFDSMAX];

  memset(pfds, -1, sizeof(struct pollfd) * PFDSMAX);

  sockfd = get_non_blocking_listener();
  if (sockfd == -1)
  {
    fprintf(stderr, "get_listener_socket: failed to get socket\n");
    return(1);
  }

  pfds[0].fd = sockfd;
  pfds[0].events = POLLIN;
  nfds = 1;

  printf("server listening on port :%s\n", PORT);
  for (;;)
  {
    ready = poll(pfds, nfds, -1);
    if (ready == -1) err_exit("poll");
    
    i = 0;
    for (; i < PFDSMAX && ready != 0; ++i)
    {
      if (pfds[i].revents == 0) continue;
      --ready;

      // TODO: add the rest or the events
      // that could be in revents (POLLHUP, etc...)
      switch ((pfds[i].revents & (POLLIN | POLLOUT | POLLERR)))
      {
        case POLLIN:
          if (pfds[i].fd == sockfd)
          {
            printf("sockfd got event\n");

            // TODO: right now I don't care about the
            // of the connected peer,
            // when I do this will need to change
            connfd = accept(sockfd, NULL, NULL); 
            if (connfd == -1)
            {
              if (errno == EAGAIN || errno == EWOULDBLOCK)
              {
                pfds[i].events = POLLIN;
                pfds[i].revents = -1;
                continue;
              }
              else
              {
                // TODO: do something better than
                // crashing after single failed accept
                err_exit("accept");
              }
            }

            if ((fcntl(connfd, F_SETFL, O_NONBLOCK)) == -1)
            {
              err_exit("fcntl: O_NONBLOCK");
            }
            
            if ((pollfd_add(&nfds, pfds, connfd, POLLIN)) == -1)
            {
              fprintf(stderr,
               "cannot accept new connection, pfds array at capacity\n");
              continue;
            }
          }
          else
          {
            printf("conn socket fired POLLIN event\n");

            while((n = read(pfds[i].fd,
                    php_buf[client_idx()] + php_buflen[client_idx()],
                    sizeof(php_buf[client_idx()]) - php_buflen[client_idx()])
                  ) == -1 && errno == EINTR);

            if (n == -1)
            {
              if (errno == EAGAIN || errno == EWOULDBLOCK)
              {
                pfds[i].events = POLLIN;
                pfds[i].revents = -1;
                continue;
              } else {
                err_exit("read ==>> connfd");
              }
            }

            php_prevbuflen[client_idx()] = php_buflen[client_idx()];
            php_buflen[client_idx()] += n;

            /* returns number of bytes consumed if successful, -2 if request is partial,
             * -1 if failed */
            n = phr_parse_request(php_buf[client_idx()], php_buflen[client_idx()],
                &php_method[client_idx()], &php_methodlen[client_idx()],
                &php_path[client_idx()], &php_pathlen[client_idx()],
                &php_minor_version[client_idx()],
                php_headers[client_idx()], &php_num_headers[client_idx()],
                php_prevbuflen[client_idx()]);

            if (n > 0) {
              puts("php_parse_request: success\n\n");
            } else if (n == -1) {
              err_exit("phr_parse_request: failed, ignore the ->");
            } else {
              assert(n == -2);
              puts("php_parse_request: partial");

              if (php_buflen[client_idx()] == sizeof(php_buf[client_idx()]))
                err_exit("php_parse_request: request_to_long");

              pfds[i].events = POLLIN;
              pfds[i].revents = -1;
              continue;
            }
              
            fputs("request: ", stdout);
            fflush(stdout);
            write(STDOUT_FILENO, php_buf[client_idx()], php_buflen[client_idx()]);
            putchar('\n');

            fputs("method: ", stdout);
            fflush(stdout);
            write(STDOUT_FILENO, php_method[client_idx()], php_methodlen[client_idx()]);
            putchar('\n');

            fputs("path: ", stdout);
            fflush(stdout);
            write(STDOUT_FILENO, php_path[client_idx()], php_pathlen[client_idx()]);
            putchar('\n');

            printf("version: HTTP/1.%d\n", php_minor_version[client_idx()]);
            fflush(stdout);

            fputs("headers:\n", stdout);
            fflush(stdout);
            for (size_t it = 0; it < php_num_headers[client_idx()]; ++it)
            {
              fputs("\theader name: ", stdout);
              fflush(stdout);
              write(STDOUT_FILENO, php_headers[client_idx()][it].name, php_headers[client_idx()][it].name_len);
              putchar('\n');

              fputs("\theader value: ", stdout);
              fflush(stdout);
              write(STDOUT_FILENO, php_headers[client_idx()][it].value, php_headers[client_idx()][it].value_len);
              putchar('\n');
            }

            return(0);
          }
        break;
        case POLLOUT: printf("POLLOUT\n");
        break;
        case POLLERR: printf("POLLERR\n"); 
        break;
        default: printf("that is not how that works brother\n"); 
      }
    }
  }

  close(sockfd);
  return(0);
}
