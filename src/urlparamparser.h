#ifndef _urlparamparser_h
#define  _urlparamparser_h

#ifdef URLPARAMPARSER_IMPLEMENTATION
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#endif // URLPARAMPARSER_IMPLEMENTATION

enum url_protocol {
  url_protocol_unknown = 0x0,
  url_protocol_insecure = 0x7, //protocol len
  url_protocol_secure = 0x8, // protocol len
};

struct url_query_param {
  const unsigned char *key;
  const unsigned char *value;
  size_t keylen;
  size_t valuelen;
};

enum url_token {
  url_token_question_mark = '?',
  url_token_and = '&',
  url_token_equal = '=',
  url_token_quote = '"',
};

static inline
void check_protocol(const unsigned char *url, size_t len, enum url_protocol *protocol);

/**
 * @return -1 in case of error or 0 if success
 */
static inline
int consume_query_param(struct url_query_param *query_param,
    const unsigned char *url, size_t urllen, size_t *idx);

/**
 * @note THIS FUNCTION DOES NOT ALLOCATE 
 *
 * @param url - should be at the index and the start of the domain,
 *   directly after the protocol
 * @param urllen - (original len of the url) - (the len of the protocol)
 * @param query_params - should be 0 initialized
 * @param n_query_params - the number of filled query params in array
 * @param query_params_max - the capacity of the query params array
 *
 * @return -1 means error 0 means success
 */
static inline
int internal__parse_query_params(const unsigned char *url, size_t urllen,
    struct url_query_param query_params[], size_t *n_query_params,
    const size_t query_params_max);

/**
 * @return -1 means error 0 means success 
 */
static inline
int parse_query_params(const unsigned char *url, size_t urllen,
    struct url_query_param query_params[], size_t *n_query_params,
    const size_t query_params_max);

/**
 * @return the index of the found key or -1 if not found
 */
static inline
ssize_t url_query_param_get_value(struct url_query_param qp[], size_t qplen,
    const unsigned char *key, size_t keylen);

#ifdef URLPARAMPARSER_IMPLEMENTATION

static inline
void check_protocol(const unsigned char *url, size_t len, enum url_protocol *protocol)
{
  assert(len > 8);

  if (memcmp(url, "http://", 7) == 0)
    *protocol = url_protocol_insecure;
  else if (memcmp(url, "https://", 8) == 0)
    *protocol = url_protocol_secure;
  else
    *protocol = url_protocol_unknown;
}

static inline
int consume_query_param(struct url_query_param *query_param,
    const unsigned char *url, size_t urllen, size_t *idx)
{
  assert(query_param->keylen == 0);
  assert(query_param->valuelen == 0);
  assert(url[*idx] == url_token_and ||
      url[*idx] == url_token_question_mark);

  ++(*idx);
  if (*idx == urllen) return -1;
  assert(isalpha(url[*idx]));

  switch (url[*idx])
  {
    case url_token_question_mark:
    case url_token_and:
    case url_token_equal:
    case url_token_quote: return -1;
    default: break;
  }

  if (*idx == urllen) return -1;

  query_param->key = &url[*idx];
  while (*idx < urllen)
  {
    switch (url[*idx])
    {
      case url_token_question_mark:
      case url_token_quote:
      case url_token_and: return -1;
      case url_token_equal: goto found_equal;
    }
    ++(*idx);
    ++query_param->keylen;
  }

found_equal:
  assert(url[*idx] == url_token_equal);
  if (*idx == urllen) return -1;

  ++(*idx);
  if (*idx == urllen) return -1;

  if (*idx == url_token_quote)
  {
    ++(*idx);
    if (*idx == urllen) return -1;
    query_param->value = &url[*idx];
    while (*idx < urllen)
   {
      if (url[*idx] == url_token_quote)
        goto found_end_quote;

      ++(*idx);
      ++query_param->valuelen;
    }

found_end_quote:
    assert(url[*idx] != url_token_quote);
    if (*idx < urllen)
      ++(*idx);
    
    switch (url[*idx])
    {
      case url_token_quote: // we didn't increment *idx
      case url_token_and:
      case url_token_question_mark:
        return 0;
      default:
        return -1;
    }
  }
  else
  {
    if (*idx == urllen) return -1;
    assert(isalnum(url[*idx]));

    // look parse to ? &
    query_param->value = &url[*idx];
    while (*idx < urllen)
    {
      switch (url[*idx])
      {
        case url_token_and:
        case url_token_question_mark: return 0;
        case url_token_quote:
        case url_token_equal: return -1;
      }
      ++(*idx);
      ++query_param->valuelen;
    }

    return 0;
  }
}

static inline
int internal__parse_query_params(
    const unsigned char *url,
    size_t urllen,
    struct url_query_param query_params[],
    size_t *n_query_params,
    const size_t query_params_max)
{
  assert(urllen > 0);
  assert(query_params_max > 0);
  assert(query_params_max > *n_query_params);

  size_t i;

  i = 0;
  while ( i < urllen &&
      *n_query_params < query_params_max )
  {
    switch(url[i])
    {
      case url_token_question_mark:
      case url_token_and:
        if (consume_query_param(
              &query_params[*n_query_params],
              url, urllen, &i) == 0) 
          ++(*n_query_params);
        else
          return -1;
        break;
      default: ++i;
    }
  }

  return 0;
}

static inline
int parse_query_params(
    const unsigned char *url,
    size_t urllen,
    struct url_query_param query_params[],
    size_t *n_query_params,
    const size_t query_params_max)
{
  enum url_protocol p;

  check_protocol(url, urllen, &p);
  if (url_protocol_unknown)
    return -1;
  else
    return internal__parse_query_params(&url[p], urllen -  p,
        query_params, n_query_params, query_params_max);
}

static inline
ssize_t url_query_param_get_value(struct url_query_param qp[], size_t qplen,
    const unsigned char *key, size_t keylen)
{
  assert(qplen != (size_t)(0 - 1));
  do {
    --qplen;
    if (memcmp(qp[qplen].key, key, keylen)) // everything but 0 is true, thanks ken
      continue;
    else
      return qplen;
  } while (qplen != (size_t)(0 - 1));

  return -1;
}

#endif // URLPARAMPARSER_IMPLEMENTATION
#endif // _urlparamparser_h
