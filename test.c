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
    String s = STR("ABCDEFGHIJKLMNOP");
    String mid = STR_VIEW(STR_DATA(s) + 8);        // VIEW → "IJKLMNOP"
    STR_CAT(s, mid);
    CHECK("aliased mid-buffer (offset 8)",
      strcmp(STR_DATA(s), "ABCDEFGHIJKLMNOPIJKLMNOP") == 0);
    STR_FREE(s);
  }

  {
    String s = STR("ABCDEFGHIJKLMNOP");
    String tail = STR_VIEW(STR_DATA(s) + 1);       // VIEW → "BCDEFGHIJKLMNOP"
    STR_CAT(s, tail);
    CHECK("aliased mid-buffer (offset 1)",
      strcmp(STR_DATA(s), "ABCDEFGHIJKLMNOPBCDEFGHIJKLMNOP") == 0);
    STR_FREE(s);
  }

  // maximum valid offset — last byte only
  {
    String s = STR("ABCDEFGHIJKLMNOP");
    String last = STR_VIEW(STR_DATA(s) + 15);      // VIEW → "P"
    STR_CAT(s, last);
    CHECK("aliased last byte only (offset 15)",
      strcmp(STR_DATA(s), "ABCDEFGHIJKLMNOP" "P") == 0);
    STR_FREE(s);
  }

  // offset=0: aliased path but accidentally correct even when buggy
  {
    String s = STR("ABCDEFGHIJKLMNOP");
    STR_CAT(s, s);
    CHECK("self-cat HEAP (offset 0)",
      strcmp(STR_DATA(s), "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP") == 0);
    STR_FREE(s);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("STR_DATA single evaluation fix");
  // ════════════════════════════════════════════════════════════════════════

  {
    String arr[2];
    arr[0] = STR("ABCDEFGHIJKLMNOP");   // must stay HEAP (len > SSO_MAX)
    arr[1] = STR("QRSTUVWXYZ!!!!!!");
    int idx = 0;
    const char* p = STR_DATA(arr[idx++]);
    // If STR_DATA regresses to a macro: idx→2, p→arr[1].ptr
    CHECK("STR_DATA single evaluation",
      idx == 1 && p == arr[0].ptr);
    STR_FREE(arr[0]);
    STR_FREE(arr[1]);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("Self-concatenation");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = STR("Hello!!");
    STR_CAT(s, s);
    CHECK("self-cat SSO",
      strcmp(STR_DATA(s), "Hello!!Hello!!") == 0);
    STR_FREE(s);
  }

  {
    String s = STR("Hello, World!!!");    // 15 chars = SSO_MAX
    STR_CAT(s, s);
    CHECK("self-cat SSO → HEAP",
      strcmp(STR_DATA(s), "Hello, World!!!Hello, World!!!") == 0);
    STR_FREE(s);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("SSO / HEAP boundary");
  // ════════════════════════════════════════════════════════════════════════

  {
    String a = STR("12345678901234");
    String b = STR("5");
    STR_CAT(a, b);
    CHECK("SSO: 14+1=15 stays SSO",
      STR_IS_SSO(a) && strcmp(STR_DATA(a), "123456789012345") == 0);
  }

  {
    String a = STR("123456789012345");    // SSO_MAX
    String b = STR("6");
    STR_CAT(a, b);
    CHECK("SSO → HEAP: 15+1=16 promotes",
      STR_IS_HEAP(a) && strcmp(STR_DATA(a), "1234567890123456") == 0);
    STR_FREE(a);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("VIEW promotion");
  // ════════════════════════════════════════════════════════════════════════

  {
    const char* base = "ABCDEFGHIJKLMNOP";
    String v = STR_VIEW(base);
    String sfx = STR("QRSTUVWXYZ");
    STR_CAT(v, sfx);
    CHECK("VIEW → HEAP promotion",
      STR_IS_HEAP(v) &&
      strcmp(STR_DATA(v), "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == 0);
    STR_FREE(v);
  }

  // VIEW + small suffix stays SSO
  {
    const char* raw = "hello";
    String v = STR_VIEW(raw);
    String sfx = STR("!!");
    STR_CAT(v, sfx);
    CHECK("VIEW → SSO (small result)",
      STR_IS_SSO(v) && strcmp(STR_DATA(v), "hello!!") == 0);
  }

  // VIEW + suffix exactly crosses SSO_MAX
  {
    const char* raw = "Hello, World!!!";   // 15 = SSO_MAX
    String v = STR_VIEW(raw);
    String sfx = STR("!");
    STR_CAT(v, sfx);
    CHECK("VIEW → HEAP (exactly SSO_MAX+1)",
      STR_IS_HEAP(v) && strcmp(STR_DATA(v), "Hello, World!!!!") == 0);
    STR_FREE(v);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("str_eq / STR_EQ");
  // ════════════════════════════════════════════════════════════════════════

  CHECK("eq: SSO == SSO same content", STR_EQ(STR("hello"), STR("hello")));
  CHECK("eq: prefix mismatch", !STR_EQ(STR("hi"), STR("high")));
  CHECK("eq: empty == empty", STR_EQ(STR(""), STR("")));
  CHECK("eq: empty != nonempty", !STR_EQ(STR(""), STR("x")));

  {
    String heap = STR("ABCDEFGHIJKLMNOP");
    String heap2 = str_clone(heap);
    CHECK("eq: HEAP == HEAP same content", STR_EQ(heap, heap2));
    STR_FREE(heap);
    STR_FREE(heap2);
  }

  {
    String      sso = STR("hello");
    const char* raw = "hello";
    String      view = STR_VIEW(raw);
    CHECK("eq: SSO == VIEW same content", STR_EQ(sso, view));
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("str_clone");
  // ════════════════════════════════════════════════════════════════════════

  {
    String a = STR("hello");
    String b = str_clone(a);
    CHECK("clone SSO: result is SSO", STR_IS_SSO(b));
    CHECK("clone SSO: same content", STR_EQ(a, b));
  }

  {
    String a = STR("ABCDEFGHIJKLMNOP");
    String b = str_clone(a);
    CHECK("clone HEAP: result is HEAP", STR_IS_HEAP(b));
    CHECK("clone HEAP: distinct ptr", a.ptr != b.ptr);
    STR_FREE(a);
    CHECK("clone HEAP: survives free of original",
      strcmp(STR_DATA(b), "ABCDEFGHIJKLMNOP") == 0);
    STR_FREE(b);
  }

  {
    const char* raw = "ABCDEFGHIJKLMNOP";
    String      view = STR_VIEW(raw);
    String      owned = str_clone(view);
    CHECK("clone VIEW: result not VIEW", !STR_IS_VIEW(owned));
    CHECK("clone VIEW: same content", STR_EQ(view, owned));
    CHECK("clone VIEW: ptr != raw source", STR_DATA(owned) != raw);
    STR_FREE(owned);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("STR_FREE");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = STR("ABCDEFGHIJKLMNOP");
    STR_FREE(s);
    CHECK("free HEAP: kind becomes SSO", STR_IS_SSO(s));
    CHECK("free HEAP: len becomes 0", s.len == 0);
    CHECK("free HEAP: buf empty", s.buf[0] == '\0');
  }

  {
    String s = STR("ABCDEFGHIJKLMNOP");
    STR_FREE(s);
    STR_FREE(s);    // must not crash
    CHECK("double STR_FREE: safe no-op", STR_IS_SSO(s) && s.len == 0);
  }

  {
    char buf[] = "hello";
    String v = STR_VIEW(buf);
    STR_FREE(v);
    CHECK("free VIEW: source buffer untouched", strcmp(buf, "hello") == 0);
    CHECK("free VIEW: kind becomes SSO", STR_IS_SSO(v));
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("STR_EMPTY");
  // ════════════════════════════════════════════════════════════════════════

  CHECK("STR_EMPTY: empty literal", STR_EMPTY(STR("")));
  CHECK("STR_EMPTY: non-empty", !STR_EMPTY(STR("x")));
  {
    String s = STR("hello");
    STR_FREE(s);
    CHECK("STR_EMPTY: after STR_FREE", STR_EMPTY(s));
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("Null termination");
  // ════════════════════════════════════════════════════════════════════════

  {
    String sso = STR("hello");
    String heap = STR("ABCDEFGHIJKLMNOP");
    CHECK("null term: SSO after STR", STR_DATA(sso)[sso.len] == '\0');
    CHECK("null term: HEAP after STR", STR_DATA(heap)[heap.len] == '\0');

    STR_CAT(sso, STR("!"));
    STR_CAT(heap, STR("QRSTUVWXYZ"));
    CHECK("null term: SSO after cat", STR_DATA(sso)[sso.len] == '\0');
    CHECK("null term: HEAP after cat", STR_DATA(heap)[heap.len] == '\0');

    const char* raw = "Hello, World!!!";
    String      view = STR_VIEW(raw);
    STR_CAT(view, STR("!"));
    CHECK("null term: VIEW→HEAP after cat", STR_DATA(view)[view.len] == '\0');

    STR_FREE(heap);
    STR_FREE(view);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("Sequential cats across SSO/HEAP boundary");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = STR("");
    STR_CAT(s, STR("Hello"));       // 5
    STR_CAT(s, STR(", "));          // 7
    STR_CAT(s, STR("World"));       // 12
    STR_CAT(s, STR("!!!"));         // 15 = SSO_MAX
    CHECK("sequential cat: at SSO_MAX is SSO", STR_IS_SSO(s));
    STR_CAT(s, STR("!"));           // 16 → HEAP
    CHECK("sequential cat: SSO_MAX+1 is HEAP", STR_IS_HEAP(s));
    CHECK("sequential cat: content correct",
      strcmp(STR_DATA(s), "Hello, World!!!!") == 0);
    STR_FREE(s);
  }


  printf("\n%d/%d passed\n", passed, passed + failed);
}