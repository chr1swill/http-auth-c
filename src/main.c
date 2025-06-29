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

typedef _Bool bool;

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
#define PHP_NUM_HEADERS  8

#define err_exit(msg) \
do { perror((msg)); exit(EXIT_FAILURE); } while(0); \

enum http_method {
  GET = 0x0,
  HEAD,
  POST,
  PUT,
  PATCH,
  DELETE,
  CONNECT,
  OPTION,
  TRACE,
  INVALID_METHOD,
};

enum http_status {
	continue                      = 100 // RFC 9110, 15.2.1
	switchingprotocols            = 101 // RFC 9110, 15.2.2
	processing                    = 102 // RFC 2518, 10.1
	earlyhints                    = 103 // RFC 8297

	ok                            = 200 // RFC 9110, 15.3.1
	created                       = 201 // RFC 9110, 15.3.2
	accepted                      = 202 // RFC 9110, 15.3.3
	nonauthoritativeinfo          = 203 // RFC 9110, 15.3.4
	nocontent                     = 204 // RFC 9110, 15.3.5
	resetcontent                  = 205 // RFC 9110, 15.3.6
	partialcontent                = 206 // RFC 9110, 15.3.7
	multistatus                   = 207 // RFC 4918, 11.1
	alreadyreported               = 208 // RFC 5842, 7.1
	imused                        = 226 // RFC 3229, 10.4.1

	multiplechoices               = 300 // RFC 9110, 15.4.1
	movedpermanently              = 301 // RFC 9110, 15.4.2
	found                         = 302 // RFC 9110, 15.4.3
	seeother                      = 303 // RFC 9110, 15.4.4
	notmodified                   = 304 // RFC 9110, 15.4.5
	useproxy                      = 305 // RFC 9110, 15.4.6

	temporaryredirect             = 307 // RFC 9110, 15.4.8
	permanentredirect             = 308 // RFC 9110, 15.4.9

	badrequest                    = 400 // RFC 9110, 15.5.1
	unauthorized                  = 401 // RFC 9110, 15.5.2
	paymentrequired               = 402 // RFC 9110, 15.5.3
	forbidden                     = 403 // RFC 9110, 15.5.4
	notfound                      = 404 // RFC 9110, 15.5.5
	methodnotallowed              = 405 // RFC 9110, 15.5.6
	notacceptable                 = 406 // RFC 9110, 15.5.7
	proxyauthrequired             = 407 // RFC 9110, 15.5.8
	requesttimeout                = 408 // RFC 9110, 15.5.9
	conflict                      = 409 // RFC 9110, 15.5.10
	gone                          = 410 // RFC 9110, 15.5.11
	lengthrequired                = 411 // RFC 9110, 15.5.12
	preconditionfailed            = 412 // RFC 9110, 15.5.13
	requestentitytoolarge         = 413 // RFC 9110, 15.5.14
	requesturitoolong             = 414 // RFC 9110, 15.5.15
	unsupportedmediatype          = 415 // RFC 9110, 15.5.16
	requestedrangenotsatisfiable  = 416 // RFC 9110, 15.5.17
	expectationfailed             = 417 // RFC 9110, 15.5.18
	teapot                        = 418 // RFC 9110, 15.5.19 (Unused)
	misdirectedrequest            = 421 // RFC 9110, 15.5.20
	unprocessableentity           = 422 // RFC 9110, 15.5.21
	locked                        = 423 // RFC 4918, 11.3
	faileddependency              = 424 // RFC 4918, 11.4
	tooearly                      = 425 // RFC 8470, 5.2.
	upgraderequired               = 426 // RFC 9110, 15.5.22
	preconditionrequired          = 428 // RFC 6585, 3
	toomanyrequests               = 429 // RFC 6585, 4
	requestheaderfieldstoolarge   = 431 // RFC 6585, 5
	unavailableforlegalreasons    = 451 // RFC 7725, 3

	internalservererror           = 500 // RFC 9110, 15.6.1
	notimplemented                = 501 // RFC 9110, 15.6.2
	badgateway                    = 502 // RFC 9110, 15.6.3
	serviceunavailable            = 503 // RFC 9110, 15.6.4
	gatewaytimeout                = 504 // RFC 9110, 15.6.5
	httpversionnotsupported       = 505 // RFC 9110, 15.6.6
	variantalsonegotiates         = 506 // RFC 2295, 8.1
	insufficientstorage           = 507 // RFC 4918, 11.5
	loopdetected                  = 508 // RFC 5842, 7.2
	notextended                   = 510 // RFC 2774, 7
	networkauthenticationrequired = 511 // RFC 6585, 6
};

const char http_status_to_str[] {
	[http_status.continue] = "Continue",
	[http_status.switchingprotocols] = "Switching Protocols",
	[http_status.processing] = "Processing",
	[http_status.earlyhints] = "Early Hints",
	[http_status.ok] = "Ok",
	[http_status.created] = "Created",
	[http_status.accepted] = "Accepted",
	[http_status.nonauthoritativeinfo] = "Non Authoritative Info",
	[http_status.nocontent] = "No Content",
	[http_status.resetcontent] = "Reset Content",
	[http_status.partialcontent] = "Partial Content",
	[http_status.multistatus] = "Multi Status",
	[http_status.alreadyreported] = "Already Reported",
	[http_status.imused] = "Im Used",
	[http_status.multiplechoices] = "Multiple Choices",
	[http_status.movedpermanently] = "Moved Permanently",
	[http_status.found] = "Found",
	[http_status.seeother] = "See Other",
	[http_status.notmodified] = "Not Modified",
	[http_status.useproxy] = "Use Proxy",
	[http_status.temporaryredirect] = "Temporary Redirect",
	[http_status.permanentredirect] = "Permanent Redirect",
	[http_status.badrequest] = "Bad Request",
	[http_status.unauthorized] = "Unauthorized",
	[http_status.paymentrequired] = "Payment Required",
	[http_status.forbidden] = "Forbidden",
	[http_status.notfound] = "Not Found",
	[http_status.methodnotallowed] = "Method Not Allowed",
	[http_status.notacceptable] = "Not Acceptable",
	[http_status.proxyauthrequired] = "Proxy Auth Required",
	[http_status.requesttimeout] = "Request Timeout",
	[http_status.conflict] = "Conflict",
	[http_status.gone] = "Gone",
	[http_status.lengthrequired] = "Length Required",
	[http_status.preconditionfailed] = "Precondition Failed",
	[http_status.requestentitytoolarge] = "Request Entity Too Large",
	[http_status.requesturitoolong] = "Request Uri Too Long",
	[http_status.unsupportedmediatype] = "Unsupported Media Type",
	[http_status.requestedrangenotsatisfiable] = "Requested Range Not Satisfiable",
	[http_status.expectationfailed] = "Expectation Failed",
	[http_status.teapot] = "Teapot",
	[http_status.misdirectedrequest] = "Misdirected Request",
	[http_status.unprocessableentity] = "Unprocessable Entity",
	[http_status.locked] = "Locked",
	[http_status.faileddependency] = "Failed Dependency",
	[http_status.tooearly] = "Too Early",
	[http_status.upgraderequired] = "Upgrade Required",
	[http_status.preconditionrequired] = "Precondition Required",
	[http_status.toomanyrequests] = "Too Many Requests",
	[http_status.requestheaderfieldstoolarge] = "Request Header Fields Too Large",
	[http_status.unavailableforlegalreasons] = "Unavailable For Legal Reasons",
	[http_status.internalservererror] = "Internal Server Error",
	[http_status.notimplemented] = "Not Implemented",
	[http_status.badgateway] = "Bad Gateway",
	[http_status.serviceunavailable] = "Service Unavailable",
	[http_status.gatewaytimeout] = "Gateway Timeout",
	[http_status.httpversionnotsupported] = "Http Version Not Supported",
	[http_status.variantalsonegotiates] = "Variant Also Negotiates",
	[http_status.insufficientstorage] = "Insufficient Storage",
	[http_status.loopdetected] = "Loop Detected",
	[http_status.notextended] = "Not Extended",
	[http_status.networkauthenticationrequired] = "Network Authentication Required",
};

static char         php_buf[PFDSMAX][REQUESTBUFFERMAX]               = {0};
static size_t       php_buflen[PFDSMAX]                              = {0};
static const char * php_method[PFDSMAX]                              = {0};
static size_t       php_methodlen[PFDSMAX]                           = {0};
static const char * php_path[PFDSMAX]                                = {0};
static size_t       php_pathlen[PFDSMAX]                             = {0};
static size_t       php_num_headers[PFDSMAX]                         = {PHP_NUM_HEADERS};
static struct       phr_header php_headers[PFDSMAX][PHP_NUM_HEADERS] = {0};
static int          php_minor_version[PFDSMAX]                       = {0};
static size_t       php_prevbuflen[PFDSMAX]                          = {0};

static enum http_status client_status_code[PFDSMAX] = {http_status.STATUS_UNSET};

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
enum http_method as_http_method(const char *method, size_t methodlen)
{
  if (memcmp(method, "GET", sizeof "GET") == 0) {
    return http_method.GET;
  } else if (memcmp(method, "HEAD", sizeof "HEAD") == 0) {
    return http_method.HEAD;
  } else if (memcmp(method, "POST", sizeof "POST") == 0) {
    return http_method.POST;
  } else if (memcmp(method, "PUT", sizeof "PUT") == 0) {
    return http_method.PUT;
  } else if (memcmp(method, "PATCH", sizeof "PATCH") == 0) {
    return http_method.PATCH;
  } else if (memcmp(method, "DELETE", sizeof "DELETE") == 0) {
    return http_method.DELETE;
  } else if (memcmp(method, "CONNECT", sizeof "CONNECT") == 0) {
    return http_method.CONNECT;
  } else if (memcmp(method, "OPTION", sizeof "OPTION") == 0) {
    return http_method.OPTION;
  } else if (memcmp(method, "TRACE", sizeof "TRACE") == 0) {
    return http_method.TRACE;
  } else {
    return http_method.INVALID_METHOD;
  }
}

static inline
bool http_method_is(enum http_method wanted, const char *method, size_t methodlen)
{
  return (wanted == as_http_method(method, methodlen));
}

static inline
int http_response_format_write(int connfd,
    int minor_version, enum http_status status,
    char *content_type, char *content, size_t contentlen)
{
  return dprintf(connfd,
      "HTTP/1.%d %d\r\n" 
      "accept-ranges: bytes\r\n"
      "content-type: %s\r\n" // example value => text/html; charset=utf-8
      "content-length: %zu\r\n"
      "\r\n"
      "%.*s"
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
              
            if (!http_method_is(http_method.GET,
                  php_method[client_idx()],
                  php_methodlen[client_idx()])) {
              // format some http error buff and set fd ready for writing
              //pfds[i].events = POLLOUT;
              //pfds[i].revents = -1;
              //continue;
            }

            if (path_is("/",
                  php_path[client_idx()],
                  php_pathlen[client_idx()])) {
            }

            // do someshit http

            return(0);
          }
        break;
        case POLLOUT: 
        printf("POLLOUT\n");

        int http_response_format_write(int connfd,
            int minor_version, enum http_status status,
            char *content_type, char *content, size_t contentlen);
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
