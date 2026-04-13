#include <stdio.h>
#include <string.h>
#include "String.h"

#define GRN "\033[32m"
#define RED "\033[31m"
#define RST "\033[0m"

static int passed = 0, failed = 0;

#define CHECK(label, expr)                                                    \
    do {                                                                      \
        if (expr) { printf(GRN "PASSED" RST " — %s\n", label); passed++; }    \
        else      { printf(RED "FAILED" RST " — %s\n", label); failed++; }    \
    } while (0)

#define SECTION(name) printf("\n── %s\n", name)

int main(void)
{
  // ════════════════════════════════════════════════════════════════════════
  SECTION("aliased offset");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ"); // 26 chars (HEAP)
    String mid = str_view(str_data(&s) + 13);     // VIEW → "NOPQRSTUVWXYZ"
    str_cat(&s, &mid);
    CHECK("aliased mid-buffer (offset 13)",
      strcmp(str_data(&s), "ABCDEFGHIJKLMNOPQRSTUVWXYZNOPQRSTUVWXYZ") == 0);
    STR_FREE(s);
  }

  {
    String s = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    String tail = str_view(str_data(&s) + 1);     // VIEW → "BCDEFGHIJKLMNOPQRSTUVWXYZ"
    str_cat(&s, &tail);
    CHECK("aliased mid-buffer (offset 1)",
      strcmp(str_data(&s), "ABCDEFGHIJKLMNOPQRSTUVWXYZBCDEFGHIJKLMNOPQRSTUVWXYZ") == 0);
    STR_FREE(s);
  }

  // maximum valid offset — last byte only
  {
    String s = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    String last = str_view(str_data(&s) + 25);    // VIEW → "Z"
    str_cat(&s, &last);
    CHECK("aliased last byte only (offset 25)",
      strcmp(str_data(&s), "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "Z") == 0);
    STR_FREE(s);
  }

  // offset=0: aliased path
  {
    String s = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    str_cat(&s, &s);
    CHECK("self-cat HEAP (offset 0)",
      strcmp(str_data(&s), "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ") == 0);
    STR_FREE(s);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("str_data &single evaluation fix");
  // ════════════════════════════════════════════════════════════════════════

  {
    String arr[2];
    arr[0] = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");   // HEAP (len > 22)
    arr[1] = str("123456789012345678901234");
    int idx = 0;
    const char* p = str_data(&arr[idx++]);
    CHECK("str_data &single evaluation",
      idx == 1 && p == str_data(&arr[0]));
    STR_FREE(arr[0]);
    STR_FREE(arr[1]);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("Self-concatenation");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = str("Hello!!");
    str_cat(&s, &s);
    CHECK("self-cat SSO",
      strcmp(str_data(&s), "Hello!!Hello!!") == 0);
    STR_FREE(s);
  }

  {
    String s = str("123456789012");    // 12 chars
    str_cat(&s, &s);                   // 12 + 12 = 24 chars (Promotes to HEAP)
    CHECK("self-cat SSO → HEAP",
      strcmp(str_data(&s), "123456789012123456789012") == 0);
    STR_FREE(s);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("SSO / HEAP boundary");
  // ════════════════════════════════════════════════════════════════════════

  {
    String a = str("123456789012345678901");  // 21 chars
    String b = str("2");
    str_cat(&a, &b);
    CHECK("SSO: 21+1=22 stays SSO",
      STR_IS_SSO(a) && strcmp(str_data(&a), "1234567890123456789012") == 0);
    STR_FREE(a);
  }

  {
    String a = str("1234567890123456789012"); // 22 chars (SSO_MAX)
    String b = str("3");
    str_cat(&a, &b);
    CHECK("SSO → HEAP: 22+1=23 promotes",
      STR_IS_HEAP(a) && strcmp(str_data(&a), "12345678901234567890123") == 0);
    STR_FREE(a);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("VIEW promotion");
  // ════════════════════════════════════════════════════════════════════════

  {
    const char* base = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    String v = str_view(base);
    String sfx = str("123");
    str_cat(&v, &sfx);
    CHECK("VIEW → HEAP promotion",
      STR_IS_HEAP(v) &&
      strcmp(str_data(&v), "ABCDEFGHIJKLMNOPQRSTUVWXYZ123") == 0);
    STR_FREE(v);
  }

  // VIEW + small suffix stays SSO
  {
    const char* raw = "hello";
    String v = str_view(raw);
    String sfx = str("!!");
    str_cat(&v, &sfx);
    CHECK("VIEW → SSO (small result)",
      STR_IS_SSO(v) && strcmp(str_data(&v), "hello!!") == 0);
    STR_FREE(v);
  }

  // VIEW + suffix exactly crosses SSO_MAX
  {
    const char* raw = "1234567890123456789012";   // 22 = SSO_MAX
    String v = str_view(raw);
    String sfx = str("!");
    str_cat(&v, &sfx);
    CHECK("VIEW → HEAP (exactly SSO_MAX+1)",
      STR_IS_HEAP(v) && strcmp(str_data(&v), "1234567890123456789012!") == 0);
    STR_FREE(v);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("str_eq / c_str_eq");
  // ════════════════════════════════════════════════════════════════════════

  CHECK("eq: SSO == SSO same content", str_eq(str("hello"), str("hello")));
  CHECK("eq: prefix mismatch", !str_eq(str("hi"), str("high")));
  CHECK("eq: empty == empty", str_eq(str(""), str("")));
  CHECK("eq: empty != nonempty", !str_eq(str(""), str("x")));

  {
    String heap = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
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
    STR_FREE(sso);
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
    String a = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    String b = str_clone(a);
    CHECK("clone HEAP: result is HEAP", STR_IS_HEAP(b));
    CHECK("clone HEAP: distinct ptr", str_data(&a) != str_data(&b));
    STR_FREE(a);
    CHECK("clone HEAP: survives free of original",
      strcmp(str_data(&b), "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == 0);
    STR_FREE(b);
  }

  {
    const char* raw = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
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
    String s = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    STR_FREE(s);
    CHECK("free HEAP: kind becomes SSO", STR_IS_SSO(s));
    CHECK("free HEAP: len becomes 0", str_len(&s) == 0);
    CHECK("free HEAP: buf empty", str_data(&s)[0] == '\0');
  }

  {
    String s = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    STR_FREE(s);
    STR_FREE(s);    // must not crash
    CHECK("double STR_FREE: safe no-op", STR_IS_SSO(s) && str_len(&s) == 0);
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
    String heap = str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    CHECK("null term: SSO after str", str_data(&sso)[str_len(&sso)] == '\0');
    CHECK("null term: HEAP after str", str_data(&heap)[str_len(&heap)] == '\0');

    String sfx1 = str("!");
    String sfx2 = str("123");
    str_cat(&sso, &sfx1);
    str_cat(&heap, &sfx2);
    
    CHECK("null term: SSO after cat", str_data(&sso)[str_len(&sso)] == '\0');
    CHECK("null term: HEAP after cat", str_data(&heap)[str_len(&heap)] == '\0');

    const char* raw = "1234567890123456789012";
    String      view = str_view(raw);
    String      sfx3 = str("!");
    str_cat(&view, &sfx3);
    CHECK("null term: VIEW→HEAP after cat", str_data(&view)[str_len(&view)] == '\0');

    STR_FREE(heap);
    STR_FREE(view);
  }


  // ════════════════════════════════════════════════════════════════════════
  SECTION("Sequential cats across SSO/HEAP boundary");
  // ════════════════════════════════════════════════════════════════════════

  {
    String s = str("");
    c_str_cat(&s, "1234567890"); // 10
    c_str_cat(&s, "12345"); // 15
    c_str_cat(&s, "12345"); // 20
    c_str_cat(&s, "12"); // 22 = SSO_MAX
    
    CHECK("sequential cat: at SSO_MAX is SSO", STR_IS_SSO(s));
    
    String t5 = str("!");                
    str_cat(&s, &t5);                                      // 23 → HEAP
    
    CHECK("sequential cat: SSO_MAX+1 is HEAP", STR_IS_HEAP(s));
    CHECK("sequential cat: content correct",
      strcmp(str_data(&s), "1234567890123451234512!") == 0);
    STR_FREE(s);
  }

  printf("\n%d/%d passed\n", passed, passed + failed);
}