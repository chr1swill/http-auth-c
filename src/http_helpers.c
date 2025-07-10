#include "http_helpers.h"
#include <string.h>

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

bool http_method_is(enum http_method wanted, const char *method)
{
  return (wanted == as_http_method(method));
}

bool http_path_is(const char *desired_path, const char *received_path)
{
  return (memcmp(desired_path, received_path, strlen(desired_path)) == 0);
}
