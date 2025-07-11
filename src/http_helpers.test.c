#include <string.h>
#include <stdbool.h>
#include "http_helpers.h"
#include <assert.h>

int main(void)
{
  bool res;
  size_t i;
  const char *cur;

  const char *paths[] = {
    "/",
    "/login",
    "/signup",
    "/login/?username=what-the-helly&password=ontay",
  };

  i = 0;

  cur = paths[i];
  res = http_path_is("/", cur);
  assert( res );

  res = http_path_is("/doggy/", cur);
  assert( !res );
  ++i;

  cur = paths[i];
  res = http_path_is("/login", cur);
  assert( res );

  res = http_path_is("/looogin", cur);
  assert( !res );
  ++i;

  cur = paths[i];
  res = http_path_is("/signup", cur);
  assert( res );

  res = http_path_is("/seenoohp", cur);
  assert( !res );
  ++i;

  cur = paths[i];
  res = http_path_is("/login/", cur);
  assert( res );

  res = http_path_is("/whatupsbrother", cur);
  assert( !res );

  return 0;
}
