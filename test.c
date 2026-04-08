#include <stdio.h>
#include <string.h>
#include "String.h"

#define GRN "\033[32m"
#define RED "\033[31m"
#define RST "\033[0m"

static int passed = 0, failed = 0;

#define CHECK(label, expr)                                                    \
    do {                                                                      \
        if (expr) { printf(GRN "PASSED" RST " — %s\n", label); passed++; }  \
        else      { printf(RED "FAILED" RST " — %s\n", label); failed++; }  \
    } while (0)

#define SECTION(name) printf("\n── %s\n", name)


int main(void)
{
  // ════════════════════════════════════════════════════════════════════════
  SECTION("aliased offset");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = str("ABCDEFGHIJKLMNOP");
    String mid = str_view(str_data(&s) + 8);        // VIEW → "IJKLMNOP"
    str_cat(&s, mid);
    CHECK("aliased mid-buffer (offset 8)",
      strcmp(str_data(&s), "ABCDEFGHIJKLMNOPIJKLMNOP") == 0);
    STR_FREE(s);
  }

  {
    String s = str("ABCDEFGHIJKLMNOP");
    String tail = str_view(str_data(&s) + 1);       // VIEW → "BCDEFGHIJKLMNOP"
    str_cat(&s, tail);
    CHECK("aliased mid-buffer (offset 1)",
      strcmp(str_data(&s), "ABCDEFGHIJKLMNOPBCDEFGHIJKLMNOP") == 0);
    STR_FREE(s);
  }

  // maximum valid offset — last byte only
  {
    String s = str("ABCDEFGHIJKLMNOP");
    String last = str_view(str_data(&s) + 15);      // VIEW → "P"
    str_cat(&s, last);
    CHECK("aliased last byte only (offset 15)",
      strcmp(str_data(&s), "ABCDEFGHIJKLMNOP" "P") == 0);
    STR_FREE(s);
  }

  // offset=0: aliased path but accidentally correct even when buggy
  {
    String s = str("ABCDEFGHIJKLMNOP");
    str_cat(&s, s);
    CHECK("self-cat HEAP (offset 0)",
      strcmp(str_data(&s), "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP") == 0);
    STR_FREE(s);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("str_data &single evaluation fix");
  // ════════════════════════════════════════════════════════════════════════

  {
    String arr[2];
    arr[0] = str("ABCDEFGHIJKLMNOP");   // must stay HEAP (len > SSO_MAX)
    arr[1] = str("QRSTUVWXYZ!!!!!!");
    int idx = 0;
    const char* p = str_data(&arr[idx++]);
    // If str_data &regresses to a macro: idx→2, p→arr[1].ptr
    CHECK("str_data &single evaluation",
      idx == 1 && p == arr[0].ptr);
    STR_FREE(arr[0]);
    STR_FREE(arr[1]);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("Self-concatenation");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = str("Hello!!");
    str_cat(&s, s);
    CHECK("self-cat SSO",
      strcmp(str_data(&s), "Hello!!Hello!!") == 0);
    STR_FREE(s);
  }

  {
    String s = str("Hello, World!!!");    // 15 chars = SSO_MAX
    str_cat(&s, s);
    CHECK("self-cat SSO → HEAP",
      strcmp(str_data(&s), "Hello, World!!!Hello, World!!!") == 0);
    STR_FREE(s);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("SSO / HEAP boundary");
  // ════════════════════════════════════════════════════════════════════════

  {
    String a = str("12345678901234");
    String b = str("5");
    str_cat(&a, b);
    CHECK("SSO: 14+1=15 stays SSO",
      STR_IS_SSO(a) && strcmp(str_data(&a), "123456789012345") == 0);
  }

  {
    String a = str("123456789012345");    // SSO_MAX
    String b = str("6");
    str_cat(&a, b);
    CHECK("SSO → HEAP: 15+1=16 promotes",
      STR_IS_HEAP(a) && strcmp(str_data(&a), "1234567890123456") == 0);
    STR_FREE(a);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("VIEW promotion");
  // ════════════════════════════════════════════════════════════════════════

  {
    const char* base = "ABCDEFGHIJKLMNOP";
    String v = str_view(base);
    String sfx = str("QRSTUVWXYZ");
    str_cat(&v, sfx);
    CHECK("VIEW → HEAP promotion",
      STR_IS_HEAP(v) &&
      strcmp(str_data(&v), "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == 0);
    STR_FREE(v);
  }

  // VIEW + small suffix stays SSO
  {
    const char* raw = "hello";
    String v = str_view(raw);
    String sfx = str("!!");
    str_cat(&v, sfx);
    CHECK("VIEW → SSO (small result)",
      STR_IS_SSO(v) && strcmp(str_data(&v), "hello!!") == 0);
  }

  // VIEW + suffix exactly crosses SSO_MAX
  {
    const char* raw = "Hello, World!!!";   // 15 = SSO_MAX
    String v = str_view(raw);
    String sfx = str("!");
    str_cat(&v, sfx);
    CHECK("VIEW → HEAP (exactly SSO_MAX+1)",
      STR_IS_HEAP(v) && strcmp(str_data(&v), "Hello, World!!!!") == 0);
    STR_FREE(v);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("str_eq / str_eq");
  // ════════════════════════════════════════════════════════════════════════

  CHECK("eq: SSO == SSO same content", str_eq(str("hello"), str("hello")));
  CHECK("eq: prefix mismatch", !str_eq(str("hi"), str("high")));
  CHECK("eq: empty == empty", str_eq(str(""), str("")));
  CHECK("eq: empty != nonempty", !str_eq(str(""), str("x")));

  {
    String heap = str("ABCDEFGHIJKLMNOP");
    String heap2 = str_clone(heap);
    CHECK("eq: HEAP == HEAP same content", str_eq(heap, heap2));
    STR_FREE(heap);
    STR_FREE(heap2);
  }

  {
    String      sso = str("hello");
    const char* raw = "hello";
    String      view = str_view(raw);
    CHECK("eq: SSO == VIEW same content", str_eq(sso, view));
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("str_clone");
  // ════════════════════════════════════════════════════════════════════════

  {
    String a = str("hello");
    String b = str_clone(a);
    CHECK("clone SSO: result is SSO", STR_IS_SSO(b));
    CHECK("clone SSO: same content", str_eq(a, b));
  }

  {
    String a = str("ABCDEFGHIJKLMNOP");
    String b = str_clone(a);
    CHECK("clone HEAP: result is HEAP", STR_IS_HEAP(b));
    CHECK("clone HEAP: distinct ptr", a.ptr != b.ptr);
    STR_FREE(a);
    CHECK("clone HEAP: survives free of original",
      strcmp(str_data(&b), "ABCDEFGHIJKLMNOP") == 0);
    STR_FREE(b);
  }

  {
    const char* raw = "ABCDEFGHIJKLMNOP";
    String      view = str_view(raw);
    String      owned = str_clone(view);
    CHECK("clone VIEW: result not VIEW", !STR_IS_VIEW(owned));
    CHECK("clone VIEW: same content", str_eq(view, owned));
    CHECK("clone VIEW: ptr != raw source", str_data(&owned) != raw);
    STR_FREE(owned);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("STR_FREE");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = str("ABCDEFGHIJKLMNOP");
    STR_FREE(s);
    CHECK("free HEAP: kind becomes SSO", STR_IS_SSO(s));
    CHECK("free HEAP: len becomes 0", s.len == 0);
    CHECK("free HEAP: buf empty", s.buf[0] == '\0');
  }

  {
    String s = str("ABCDEFGHIJKLMNOP");
    STR_FREE(s);
    STR_FREE(s);    // must not crash
    CHECK("double STR_FREE: safe no-op", STR_IS_SSO(s) && s.len == 0);
  }

  {
    char buf[] = "hello";
    String v = str_view(buf);
    STR_FREE(v);
    CHECK("free VIEW: source buffer untouched", strcmp(buf, "hello") == 0);
    CHECK("free VIEW: kind becomes SSO", STR_IS_SSO(v));
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("str_empty");
  // ════════════════════════════════════════════════════════════════════════

  CHECK("str_empty: empty literal", str_empty(str("")));
  CHECK("str_empty: non-empty", !str_empty(str("x")));
  {
    String s = str("hello");
    STR_FREE(s);
    CHECK("str_empty: after STR_FREE", str_empty(s));
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("Null termination");
  // ════════════════════════════════════════════════════════════════════════

  {
    String sso = str("hello");
    String heap = str("ABCDEFGHIJKLMNOP");
    CHECK("null term: SSO after str", str_data(&sso)[sso.len] == '\0');
    CHECK("null term: HEAP after str", str_data(&heap)[heap.len] == '\0');

    str_cat(&sso, str("!"));
    str_cat(&heap, str("QRSTUVWXYZ"));
    CHECK("null term: SSO after cat", str_data(&sso)[sso.len] == '\0');
    CHECK("null term: HEAP after cat", str_data(&heap)[heap.len] == '\0');

    const char* raw = "Hello, World!!!";
    String      view = str_view(raw);
    str_cat(&view, str("!"));
    CHECK("null term: VIEW→HEAP after cat", str_data(&view)[view.len] == '\0');

    STR_FREE(heap);
    STR_FREE(view);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("Sequential cats across SSO/HEAP boundary");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = str("");
    str_cat(&s, str("Hello"));       // 5
    str_cat(&s, str(", "));          // 7
    str_cat(&s, str("World"));       // 12
    str_cat(&s, str("!!!"));         // 15 = SSO_MAX
    CHECK("sequential cat: at SSO_MAX is SSO", STR_IS_SSO(s));
    str_cat(&s, str("!"));           // 16 → HEAP
    CHECK("sequential cat: SSO_MAX+1 is HEAP", STR_IS_HEAP(s));
    CHECK("sequential cat: content correct",
      strcmp(str_data(&s), "Hello, World!!!!") == 0);
    STR_FREE(s);
  }


  printf("\n%d/%d passed\n", passed, passed + failed);
}