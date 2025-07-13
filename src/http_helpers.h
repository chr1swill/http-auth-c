#ifndef http_helpers_h
#define http_helpers_h

#ifdef HTTP_HELPERS_IMPLEMENTATION
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#endif // HTTP_HELPERS_IMPLEMENTATION

enum http_method {
  http_method_get = 0x0,
  http_method_head,
  http_method_post,
  http_method_put,
  http_method_patch,
  http_method_delete,
  http_method_connect,
  http_method_option,
  http_method_trace,
  http_method_invalid_method,
};

enum http_status {
	http_status_continue                             = 100,
	http_status_switchingprotocols                   = 101,
	http_status_processing                           = 102,
	http_status_earlyhints                           = 103,
	http_status_ok                                   = 200,
	http_status_created                              = 201,
	http_status_accepted                             = 202,
	http_status_nonauthoritativeinfo                 = 203,
	http_status_nocontent                            = 204,
	http_status_resetcontent                         = 205,
	http_status_partialcontent                       = 206,
	http_status_multistatus                          = 207,
	http_status_alreadyreported                      = 208,
	http_status_imused                               = 226,
	http_status_multiplechoices                      = 300,
	http_status_movedpermanently                     = 301,
	http_status_found                                = 302,
	http_status_seeother                             = 303,
	http_status_notmodified                          = 304,
	http_status_useproxy                             = 305,
	http_status_temporaryredirect                    = 307,
	http_status_permanentredirect                    = 308,
	http_status_badrequest                           = 400,
	http_status_unauthorized                         = 401,
	http_status_paymentrequired                      = 402,
	http_status_forbidden                            = 403,
	http_status_notfound                             = 404,
	http_status_methodnotallowed                     = 405,
	http_status_notacceptable                        = 406,
	http_status_proxyauthrequired                    = 407,
	http_status_requesttimeout                       = 408,
	http_status_conflict                             = 409,
	http_status_gone                                 = 410,
	http_status_lengthrequired                       = 411,
	http_status_preconditionfailed                   = 412,
	http_status_requestentitytoolarge                = 413,
	http_status_requesturitoolong                    = 414,
	http_status_unsupportedmediatype                 = 415,
	http_status_requestedrangenotsatisfiable         = 416,
	http_status_expectationfailed                    = 417,
	http_status_teapot                               = 418,
	http_status_misdirectedrequest                   = 421,
	http_status_unprocessableentity                  = 422,
	http_status_locked                               = 423,
	http_status_faileddependency                     = 424,
	http_status_tooearly                             = 425,
	http_status_upgraderequired                      = 426,
	http_status_preconditionrequired                 = 428,
	http_status_toomanyrequests                      = 429,
	http_status_requestheaderfieldstoolarge          = 431,
	http_status_unavailableforlegalreasons           = 451,
	http_status_internalservererror                  = 500,
	http_status_notimplemented                       = 501,
	http_status_badgateway                           = 502,
	http_status_serviceunavailable                   = 503,
	http_status_gatewaytimeout                       = 504,
	http_status_httpversionnotsupported              = 505,
	http_status_variantalsonegotiates                = 506,
	http_status_insufficientstorage                  = 507,
	http_status_loopdetected                         = 508,
	http_status_notextended                          = 510,
	http_status_networkauthenticationrequired        = 511,
  http_status_status_unset,
};

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
enum http_method as_http_method(const char *method);
static inline
bool http_method_is(enum http_method wanted, const char *method);
// desired_path must be null terminated
static inline
bool http_path_is(const char *desired_path, const char *received_path);
static inline
int http_response_format_write(int connfd,
    int minor_version, enum http_status status,
    const char *content_type, const char *content, size_t contentlen);
static inline
int http_get_non_blocking_listener();

#ifdef HTTP_HELPERS_IMPLEMENTATION

static inline
enum http_method as_http_method(const char *method)
{
  if (memcmp(method, "GET", sizeof("GET") - 1) == 0) 
    return http_method_get;
  else if (memcmp(method, "HEAD", sizeof("HEAD") - 1) == 0) 
    return http_method_head;
  else if (memcmp(method, "POST", sizeof("POST") - 1) == 0) 
    return http_method_post;
  else if (memcmp(method, "PUT", sizeof("PUT") - 1) == 0) 
    return http_method_put;
  else if (memcmp(method, "PATCH", sizeof("PATCH") - 1 ) == 0) 
    return http_method_patch;
  else if (memcmp(method, "DELETE", sizeof("DELETE") - 1) == 0) 
    return http_method_delete;
  else if (memcmp(method, "CONNECT", sizeof("CONNECT") - 1) == 0) 
    return http_method_connect;
  else if (memcmp(method, "OPTION", sizeof("OPTION") - 1) == 0) 
    return http_method_option;
  else if (memcmp(method, "TRACE", sizeof("TRACE") - 1) == 0) 
    return http_method_trace;
  else 
    return http_method_invalid_method;
}

static inline
bool http_method_is(enum http_method wanted, const char *method)
{
  return (wanted == as_http_method(method));
}

static inline
bool http_path_is(const char *desired_path, const char *received_path)
{
  return (memcmp(desired_path, received_path, strlen(desired_path)) == 0);
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

static inline
int http_get_non_blocking_listener()
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
#endif // HTTP_HELPERS_IMPLEMENTATION
#endif // http_helpers_h
