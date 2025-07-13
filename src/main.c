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
#include "login.html.h"
#include "index.html.h"
#include "signup.html.h"
#define URLPARAMPARSER_IMPLEMENTATION
#include "urlparamparser.h"

#define PORT "8080"
#define BACKLOG 10
#define PFDSMAX 1024
#define REQUESTBUFFERMAX 1024
#define HTTP_HELPERS_IMPLEMENTATION
#include "http_helpers.h"

#define PHP_HASHIDX(connfd, sockfd) (((connfd) - (sockfd)) - 1)
#define client_idx() PHP_HASHIDX(connfd, sockfd)
#define PHP_NUM_HEADERS 64 
#define QUERY_PARAM_MAX 4

#define err_exit(msg) \
do { perror((msg)); exit(EXIT_FAILURE); } while(0); \

static char             php_buf[PFDSMAX][REQUESTBUFFERMAX]               = {0};
static size_t           php_buflen[PFDSMAX]                              = {0};
static const char *     php_method[PFDSMAX]                              = {0};
static size_t           php_methodlen[PFDSMAX]                           = {0};
static const char *     php_path[PFDSMAX]                                = {0};
static size_t           php_pathlen[PFDSMAX]                             = {0};
static size_t           php_num_headers[PFDSMAX]                         = {PHP_NUM_HEADERS};
static struct           phr_header php_headers[PFDSMAX][PHP_NUM_HEADERS] = {0};
static int              php_minor_version[PFDSMAX]                       = {0};
static size_t           php_prevbuflen[PFDSMAX]                          = {0};
static const char *     php_content_type[PFDSMAX]                        = {0};
static const char *     php_content[PFDSMAX]                             = {0};
static size_t           php_contentlen[PFDSMAX]                          = {0};
static enum http_status client_status_code[PFDSMAX]                      = {http_status_status_unset};

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

static inline
void pollfd_close_and_clear(struct pollfd *pfds, nfds_t *nfds)
{
  while(close(pfds->fd) == -1 && errno == EINTR);

  pfds->fd = -1;
  pfds->events = -1;
  pfds->revents = -1;
  --(*nfds);
}

int main()
{
  ssize_t n;
  nfds_t nfds, i;
  int sockfd, connfd, ready;
  struct pollfd pfds[PFDSMAX];

  memset(pfds, -1, sizeof(struct pollfd) * PFDSMAX);

  sockfd = http_get_non_blocking_listener();
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

      switch ((pfds[i].revents & (POLLIN | POLLOUT | POLLERR | POLLHUP | POLLNVAL)))
      {
        case POLLIN:
          if (pfds[i].fd == sockfd)
          {
            printf("server sockfd got POLLIN\n");

            // right now I don't care about the
            // of the connected peer ip address
            // when I do this will need to change
            while ((connfd = accept(sockfd, NULL, NULL))
                == -1 && errno == EINTR)
            if (connfd == -1)
            {
              switch (errno)
              {
                case EWOULDBLOCK:
                  pfds[i].events = POLLIN;
                  pfds[i].revents = -1;
                  continue;
                case ECONNABORTED:
                  perror("accept: closing conntection");
                  pollfd_close_and_clear(&pfds[i], &nfds);
                  continue;
                default:
                  err_exit("accept");
                  break;
              }
            }

            if ((fcntl(connfd, F_SETFL, O_NONBLOCK)) == -1)
              err_exit("fcntl: O_NONBLOCK");
            
            if ((pollfd_add(&nfds, pfds, connfd, POLLIN)) == -1)
            {
              fprintf(stderr,
               "cannot accept new connection, pfds array at capacity\n");
              continue;
            }
          } else {
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

            php_num_headers[client_idx()] = PHP_NUM_HEADERS;
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
              php_buflen[client_idx()] = 0;
            } else if (n == -1) {
              err_exit("phr_parse_request: failed, ignore the ->");
            } else {
              assert(n == -2);
              if (php_buflen[client_idx()] == sizeof(php_buf[client_idx()]))
                err_exit("php_parse_request: request_to_long");

              pfds[i].events = POLLIN;
              pfds[i].revents = -1;
              continue;
            }

            if (!http_method_is(http_method_get, php_method[client_idx()])) {
              client_status_code[client_idx()] = http_status_methodnotallowed;
              pfds[i].events = POLLOUT;
              pfds[i].revents = -1;
              continue;
            }

            if (http_path_is("/login_submit",
                  php_path[client_idx()]))
            {
              //client_status_code[client_idx()] = http_status_ok;
              //php_content[client_idx()] = (char *)html_login_html;
              //php_contentlen[client_idx()] = (size_t)html_login_html_len;
              //php_content_type[client_idx()] = "text/html";
              ssize_t idx_or_not;
              size_t n_query_params;
              struct url_query_param *username, *password;
              struct url_query_param query_params[QUERY_PARAM_MAX] = {0};

              n_query_params = 0;
              if (parse_query_params((const unsigned char *)php_path[client_idx()],
                    php_pathlen[client_idx()], query_params, &n_query_params,
                    QUERY_PARAM_MAX) == -1)
                goto error_not_found;

              idx_or_not = url_query_param_get_value(query_params, n_query_params,
                  "username", (sizeof "username") - 1);
              // TODO: do something better here like  
              // reporting an error or 
              // redirect to a signup page
              if (idx_or_not == -1) goto error_not_found; 
              username = &query_params[idx_or_not];

              printf("username=%.*s\n", (int)username->valuelen, username->value);
              fflush(stdout);

              idx_or_not = url_query_param_get_value(query_params, n_query_params,
                  "password", (sizeof "password") - 1);
              // TODO: do something better here like  
              // reporting an error or 
              // redirect to a signup page
              if (idx_or_not == -1) goto error_not_found; 
              password = &query_params[idx_or_not];

              printf("password=%.*s\n", (int)password->valuelen, password->value);
              fflush(stdout);

              // check if username is in our database or matches our records
              // or_else go to goto error_not_found || or some other thing like that;
              // we for sure need a signup page of some sort tho

              return 0;

              pfds[i].events = POLLOUT;
               pfds[i].revents = -1;
               continue;
            }

            if (http_path_is("/login",
                  php_path[client_idx()]))
            {
              client_status_code[client_idx()] = http_status_ok;
              php_content[client_idx()] = (char *)html_login_html;
              php_contentlen[client_idx()] = (size_t)html_login_html_len;
              php_content_type[client_idx()] = "text/html";

              pfds[i].events = POLLOUT;
              pfds[i].revents = -1;
              continue;
            }

            if (http_path_is("/",
                  php_path[client_idx()]))
            {
              client_status_code[client_idx()] = http_status_ok;
              php_content[client_idx()] =(char *)html_index_html;
              php_contentlen[client_idx()] = (size_t)html_index_html_len;
              php_content_type[client_idx()] = "text/html";

              pfds[i].events = POLLOUT;
              pfds[i].revents = -1;
              continue;
            }

error_not_found:
            client_status_code[client_idx()] = http_status_notfound;
            php_content[client_idx()] =
              http_status_to_str[client_status_code[client_idx()]];
            php_contentlen[client_idx()] =
              strlen(http_status_to_str[client_status_code[client_idx()]]);
            php_content_type[client_idx()] = "text/plain";

            pfds[i].events = POLLOUT;
            pfds[i].revents = -1;
            continue;
          }
        break;
        case POLLOUT: 
        printf("fd %d fired POLLOUT event\n", pfds[i].fd);


        if (http_response_format_write(pfds[i].fd,
              php_minor_version[client_idx()],
              client_status_code[client_idx()],
              php_content_type[client_idx()],
              php_content[client_idx()],
              php_contentlen[client_idx()]) <= 0) {
          err_exit("http_response_format_write: dprintf");
        }

        pollfd_close_and_clear(&pfds[i], &nfds);

        break;
        case POLLERR:
        printf("POLLERR fired on pfds[%zu].fd=%d closing it.\n",
            i, pfds[i].fd);
        pollfd_close_and_clear(&pfds[i], &nfds);
        break;
        case POLLHUP:
        printf("POLLHUP fired on pfds[%zu].fd=%d closing it.\n",
            i, pfds[i].fd);
        pollfd_close_and_clear(&pfds[i], &nfds);
        break;
        printf("POLLHUP fired on pfds[%zu].fd=%d closing it.\n",
            i, pfds[i].fd);
        pollfd_close_and_clear(&pfds[i], &nfds);
        break;
        default:
        printf("something is messed but brother, pfds[%ld].revents=%d\n",
            i, pfds[i].revents); 
        
        printf("has POLLIN=%s\n",
            (pfds[i].revents & POLLIN) == POLLIN ? "true" : "false");
        printf("has POLLPRI=%s\n",
            (pfds[i].revents & POLLPRI) == POLLPRI ? "true" : "false");
        printf("has POLLOUT=%s\n",
            (pfds[i].revents & POLLOUT) == POLLOUT ? "true" : "false");
#if defined __USE_XOPEN || defined __USE_XOPEN2K8
        printf("has POLLRDNORM=%s\n",
            (pfds[i].revents & POLLRDNORM) == POLLRDNORM ? "true" : "false");
        printf("has POLLRDBAND=%s\n",
            (pfds[i].revents & POLLRDBAND) == POLLRDBAND ? "true" : "false");
        printf("has POLLWRNORM=%s\n",
            (pfds[i].revents & POLLWRNORM) == POLLWRNORM ? "true" : "false");
        printf("has POLLWRBAND=%s\n",
            (pfds[i].revents & POLLWRBAND) == POLLWRBAND ? "true" : "false");
        printf("has POLLWRNORM=%s\n",
            (pfds[i].revents & POLLWRNORM) == POLLWRNORM ? "true" : "false");
#endif
#ifdef __USE_GNU
        printf("has POLLMSG=%s\n",
            (pfds[i].revents & POLLMSG) == POLLMSG ? "true" : "false");
        printf("has POLLREMOVE=%s\n",
            (pfds[i].revents & POLLREMOVE) == POLLREMOVE ? "true" : "false");
        printf("has POLLRDHUP=%s\n",
            (pfds[i].revents & POLLRDHUP) == POLLRDHUP ? "true" : "false");
#endif
        printf("has POLLERR=%s\n",
            (pfds[i].revents & POLLERR) == POLLERR ? "true" : "false");
        printf("has POLLHUP=%s\n",
            (pfds[i].revents & POLLHUP) == POLLHUP ? "true" : "false");
        printf("has POLLNVAL=%s\n",
            (pfds[i].revents & POLLNVAL) == POLLNVAL ? "true" : "false");

        printf("closing and clearing fd from pfds array\n");
        pollfd_close_and_clear(&pfds[i], &nfds);
        break;
      }
    }
  }

  close(sockfd);
  return(0);
}
