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
#include "http_helpers.h"
#include "login.html.h"
#include "index.html.h"

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
#define PHP_NUM_HEADERS 64 

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

static const char *http_status_to_str[] = {
	[http_status_continue] = "Continue",
	[http_status_switchingprotocols] = "Switching Protocols",
	[http_status_processing] = "Processing",
	[http_status_earlyhints] = "Early Hints",
	[http_status_ok] = "Ok",
	[http_status_created] = "Created",
	[http_status_accepted] = "Accepted",
	[http_status_nonauthoritativeinfo] = "Non Authoritative Info",
	[http_status_nocontent] = "No Content",
	[http_status_resetcontent] = "Reset Content",
	[http_status_partialcontent] = "Partial Content",
	[http_status_multistatus] = "Multi Status",
	[http_status_alreadyreported] = "Already Reported",
	[http_status_imused] = "Im Used",
	[http_status_multiplechoices] = "Multiple Choices",
	[http_status_movedpermanently] = "Moved Permanently",
	[http_status_found] = "Found",
	[http_status_seeother] = "See Other",
	[http_status_notmodified] = "Not Modified",
	[http_status_useproxy] = "Use Proxy",
	[http_status_temporaryredirect] = "Temporary Redirect",
	[http_status_permanentredirect] = "Permanent Redirect",
	[http_status_badrequest] = "Bad Request",
	[http_status_unauthorized] = "Unauthorized",
	[http_status_paymentrequired] = "Payment Required",
	[http_status_forbidden] = "Forbidden",
	[http_status_notfound] = "Not Found",
	[http_status_methodnotallowed] = "Method Not Allowed",
	[http_status_notacceptable] = "Not Acceptable",
	[http_status_proxyauthrequired] = "Proxy Auth Required",
	[http_status_requesttimeout] = "Request Timeout",
	[http_status_conflict] = "Conflict",
	[http_status_gone] = "Gone",
	[http_status_lengthrequired] = "Length Required",
	[http_status_preconditionfailed] = "Precondition Failed",
	[http_status_requestentitytoolarge] = "Request Entity Too Large",
	[http_status_requesturitoolong] = "Request Uri Too Long",
	[http_status_unsupportedmediatype] = "Unsupported Media Type",
	[http_status_requestedrangenotsatisfiable] = "Requested Range Not Satisfiable",
	[http_status_expectationfailed] = "Expectation Failed",
	[http_status_teapot] = "Teapot",
	[http_status_misdirectedrequest] = "Misdirected Request",
	[http_status_unprocessableentity] = "Unprocessable Entity",
	[http_status_locked] = "Locked",
	[http_status_faileddependency] = "Failed Dependency",
	[http_status_tooearly] = "Too Early",
	[http_status_upgraderequired] = "Upgrade Required",
	[http_status_preconditionrequired] = "Precondition Required",
	[http_status_toomanyrequests] = "Too Many Requests",
	[http_status_requestheaderfieldstoolarge] = "Request Header Fields Too Large",
	[http_status_unavailableforlegalreasons] = "Unavailable For Legal Reasons",
	[http_status_internalservererror] = "Internal Server Error",
	[http_status_notimplemented] = "Not Implemented",
	[http_status_badgateway] = "Bad Gateway",
	[http_status_serviceunavailable] = "Service Unavailable",
	[http_status_gatewaytimeout] = "Gateway Timeout",
	[http_status_httpversionnotsupported] = "Http Version Not Supported",
	[http_status_variantalsonegotiates] = "Variant Also Negotiates",
	[http_status_insufficientstorage] = "Insufficient Storage",
	[http_status_loopdetected] = "Loop Detected",
	[http_status_notextended] = "Not Extended",
	[http_status_networkauthenticationrequired] = "Network Authentication Required",
};

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

static inline
void close_and_clear_pfd(struct pollfd *pfds)
{
  while(close(pfds->fd) == -1 && errno == EINTR);

  pfds->fd = -1;
  pfds->events = -1;
  pfds->revents = -1;
}

static inline
int http_response_format_write(int connfd,
    int minor_version, enum http_status status,
    const char *content_type, const char *content, size_t contentlen)
{
  return dprintf(connfd,
      "HTTP/1.%d %d\r\n" 
      "accept-ranges: bytes\r\n"
      "content-type: %s\r\n" // example value => text/html; charset=utf-8
      "content-length: %zu\r\n"
      "\r\n"
      "%.*s",
      minor_version, status,
      content_type,
      contentlen,
      (int)contentlen, content);
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
                case EBADF: 
                  assert(1 == 0 &&
                      "accept EBADF should never happend: unreachable"); 
                  break;
                case ECONNABORTED:
                  perror("accept");
                  continue;
                case EFAULT:
                  assert(1 == 0 &&
                      "accept EFAULT should never happend: unreachable");
                  break;
                case EINTR:
                  assert(1 == 0 &&
                      "accept EINTR should never happend: unreachable");
                  break;
                case EINVAL:
                  assert(1 == 0 &&
                      "accept EINVAL should never happend: unreachable");
                  break;
                case EMFILE:
                  err_exit("accept errno == EMFILE");
                  break;
                case ENFILE:
                  err_exit("accept errno == EMFILE");
                  break;
                case ENOBUFS:
                case ENOMEM:
                  err_exit("accept errno == ENOBUFS || errno == ENOMEM");
                  break;
                case ENOTSOCK:
                  assert(1 == 0 &&
                      "accept ENOTSOCK should never happend: unreachable");
                  break;
                case EOPNOTSUPP:
                  assert(1 == 0 &&
                      "accept EOPNOTSUPP should never happend: unreachable");
                  break;
                case EPERM:
                  err_exit("accept: issue with your fire wall setup");
                  break;
                case EPROTO:
                case ENOSR:
                case ESOCKTNOSUPPORT:
                case ERESTART:
                case ETIMEDOUT:
                case EPROTONOSUPPORT:
                  err_exit("accept: some sort of protocol or network error occured");
                  break;
                default:
                  err_exit("accept: an unknow error occured");
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

            if (http_path_is("/login",
                  php_path[client_idx()],
                  php_pathlen[client_idx()]))
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
                  php_path[client_idx()],
                  php_pathlen[client_idx()]))
            {
              client_status_code[client_idx()] = http_status_ok;
              php_content[client_idx()] =(char *)html_index_html;
              php_contentlen[client_idx()] = (size_t)html_index_html_len;
              php_content_type[client_idx()] = "text/html";

              pfds[i].events = POLLOUT;
              pfds[i].revents = -1;
              continue;
            }

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

        close_and_clear_pfd(&pfds[i]);

        break;
        case POLLERR:
        printf("POLLERR fired on pfds[%zu].fd=%d closing it.\n", i, pfds[i].fd);
        close_and_clear_pfd(&pfds[i]);
        break;
        case POLLHUP:
        printf("POLLHUP fired on pfds[%zu].fd=%d closing it.\n", i, pfds[i].fd);
        close_and_clear_pfd(&pfds[i]);
        break;
        printf("POLLHUP fired on pfds[%zu].fd=%d closing it.\n", i, pfds[i].fd);
        close_and_clear_pfd(&pfds[i]);
        break;
        default:
        printf("something is messed but brother, pfds[%ld].revents=%d\n",
            i, pfds[i].revents); 
        
        printf("has POLLIN=%s\n", (pfds[i].revents & POLLIN) == POLLIN ? "true" : "false");
        printf("has POLLPRI=%s\n", (pfds[i].revents & POLLPRI) == POLLPRI ? "true" : "false");
        printf("has POLLOUT=%s\n", (pfds[i].revents & POLLOUT) == POLLOUT ? "true" : "false");
#if defined __USE_XOPEN || defined __USE_XOPEN2K8
        printf("has POLLRDNORM=%s\n", (pfds[i].revents & POLLRDNORM) == POLLRDNORM ? "true" : "false");
        printf("has POLLRDBAND=%s\n", (pfds[i].revents & POLLRDBAND) == POLLRDBAND ? "true" : "false");
        printf("has POLLWRNORM=%s\n", (pfds[i].revents & POLLWRNORM) == POLLWRNORM ? "true" : "false");
        printf("has POLLWRBAND=%s\n", (pfds[i].revents & POLLWRBAND) == POLLWRBAND ? "true" : "false");
        printf("has POLLWRNORM=%s\n", (pfds[i].revents & POLLWRNORM) == POLLWRNORM ? "true" : "false");
#endif
#ifdef __USE_GNU
        printf("has POLLMSG=%s\n", (pfds[i].revents & POLLMSG) == POLLMSG ? "true" : "false");
        printf("has POLLREMOVE=%s\n", (pfds[i].revents & POLLREMOVE) == POLLREMOVE ? "true" : "false");
        printf("has POLLRDHUP=%s\n", (pfds[i].revents & POLLRDHUP) == POLLRDHUP ? "true" : "false");
#endif
        printf("has POLLERR=%s\n", (pfds[i].revents & POLLERR) == POLLERR ? "true" : "false");
        printf("has POLLHUP=%s\n", (pfds[i].revents & POLLHUP) == POLLHUP ? "true" : "false");
        printf("has POLLNVAL=%s\n", (pfds[i].revents & POLLNVAL) == POLLNVAL ? "true" : "false");

        printf("closing and clearing fd from pfds array\n");
        close_and_clear_pfd(&pfds[i]);
        break;
      }
    }
  }

  close(sockfd);
  return(0);
}
