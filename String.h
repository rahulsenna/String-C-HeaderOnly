#pragma once
#include <stdint.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define SSO_MAX 15

typedef enum { STR_KIND_SSO = 0, STR_KIND_HEAP, STR_KIND_VIEW } StrKind;
typedef struct String
{
  size_t len;
  union
  {
    char buf[SSO_MAX + 1];
    struct
    {
      char* ptr;
      size_t cap;
    };
  };
  StrKind kind;
} String;


#define _STR_VIEW(s, cstr, n)        \
    do {                             \
        (s).len = (n);               \
        (s).ptr = (char*)(cstr);     \
        (s).cap = 0;                 \
        (s).kind = STR_KIND_VIEW;    \
    } while (0)

#define _STR_CPY(s, cstr, n)                     \
    do {                                         \
        size_t _n = (n);                         \
        (s).len = _n;                            \
        if (_n <= SSO_MAX) {                     \
            (s).kind = STR_KIND_SSO;             \
            memcpy((s).buf, (cstr), _n);         \
            (s).buf[_n] = '\0';                  \
        } else {                                 \
            (s).kind = STR_KIND_HEAP;            \
            (s).cap = _n + 1;                    \
            (s).ptr = (char*)malloc(_n + 1);     \
            memcpy((s).ptr, (cstr), _n);         \
            (s).ptr[_n] = '\0';                  \
        }                                        \
    } while (0)

#define STR_IS_SSO(s)   ((s).kind == STR_KIND_SSO)
#define STR_IS_VIEW(s)  ((s).kind == STR_KIND_VIEW)
#define STR_IS_HEAP(s)  ((s).kind == STR_KIND_HEAP)

static inline char* str_data(String* s)
{
  return s->kind == STR_KIND_SSO ? s->buf : s->ptr;
}
#define STR_DATA(s)  str_data(&(s))
#define STR_FREE(s)                     \
    do {                                \
        if (!STR_IS_SSO(s) &&           \
            !STR_IS_VIEW(s))            \
            free((s).ptr);              \
        (s).len = 0;                    \
        (s).kind = STR_KIND_SSO;        \
        (s).buf[0] = '\0';              \
    } while (0)

static inline String str_clone(String s)
{
  String out;
  _STR_CPY(out, STR_DATA(s), s.len);
  return out;
}

#ifndef __cplusplus
static inline String _str_from_cstr(const char* s)
{
  String out;
  _STR_CPY(out, s, strlen(s));
  return out;
}

static inline String _str_view_from_cstr(const char* s)
{
  String out;
  _STR_VIEW(out, s, strlen(s));
  return out;
}
static inline String _str_from_string(String s)
{
  return s;
}
//-----------------------------
static inline String _str_from_cstr_mut(char *s)
{
    return _str_from_cstr((const char *)s);
}

static inline String _str_view_from_cstr_mut(char *s)
{
    return _str_view_from_cstr((const char *)s);
}

#define STR(x)                        \
    _Generic((0, (x)),                \
        String: _str_from_string,     \
        char *: _str_from_cstr_mut,   \
        const char *: _str_from_cstr  \
    )(x)

#define STR_VIEW(x)                        \
    _Generic((0, (x)),                     \
        String: _str_from_string,          \
        char *: _str_view_from_cstr_mut,   \
        const char *: _str_view_from_cstr  \
    )(x)
//-----------------------------

#else
static inline String STR(String s)
{
  return s;
}
static inline String STR(char* s)
{
  String out;
  _STR_CPY(out, s, strlen(s));
  return out;
}
static inline String STR(const char* s)
{
  String out;
  _STR_CPY(out, s, strlen(s));
  return out;
}

static inline String STR_VIEW(String s) { return s; }
static inline String STR_VIEW(const char* s)
{
  String out;
  _STR_VIEW(out, s, strlen(s));
  return out;
}
static inline String STR_VIEW(char* s)
{
  String out;
  _STR_VIEW(out, s, strlen(s));
  return out;
}
#endif

static inline int str_eq(String a, String b)
{
  return a.len == b.len && memcmp(STR_DATA(a), STR_DATA(b), a.len) == 0;
}
static inline int c_str_eq(String a, char* b)
{
  return a.len == strlen(b) && memcmp(STR_DATA(a), b, a.len) == 0;
}

static inline int _str_reserve(String* s, size_t need)
{
  if (need <= SSO_MAX + 1 && !STR_IS_HEAP(*s))
    return 1;

  if (STR_IS_HEAP(*s))
  {
    if (s->cap >= need)
      return 1;

    char* p = (char*) realloc(s->ptr, need);
    if (!p)
      return 0;

    s->ptr = p;
    s->cap = need;
    return 1;
  }

  char* p = (char*) malloc(need);
  if (!p)
    return 0;

  memcpy(p, STR_DATA(*s), s->len);
  p[s->len] = '\0';
  s->ptr = p;
  s->cap = need;
  s->kind = STR_KIND_HEAP;
  return 1;
}

static inline int str_cat(String* dst, String src)
{
  size_t old_len = dst->len;
  size_t new_len = old_len + src.len;

  if (new_len <= SSO_MAX)
  {
    char* src_data = STR_DATA(src);
    if (STR_IS_VIEW(*dst))
    {
      const char* view_ptr = dst->ptr;
      memmove(dst->buf, view_ptr, dst->len);
      dst->kind = STR_KIND_SSO;
    }
    memmove(dst->buf + old_len, src_data, src.len);
    dst->buf[new_len] = '\0';
    dst->len = new_len;
    return 1;
  }

  size_t alias_offset = 0;
  int aliased = STR_IS_HEAP(*dst) && STR_DATA(src) >= dst->ptr && STR_DATA(src) < dst->ptr + dst->len;
  if (aliased)
    alias_offset = STR_DATA(src) - dst->ptr;

  if (!_str_reserve(dst, new_len + 1))
    return 0;
  char* src_data = aliased ? dst->ptr + alias_offset : STR_DATA(src);

  memmove(dst->ptr + old_len, src_data, src.len);
  dst->ptr[new_len] = '\0';
  dst->len = new_len;
  dst->kind = STR_KIND_HEAP;
  return 1;
}


#define STR_EQ(a, b)       str_eq((a), (b))
#define STR_EMPTY(s)       ((s).len == 0)
#define C_STR_EQ(a, b)     c_str_eq((a), (b))
#define STR_CAT(dst, src)  str_cat(&(dst), (src))