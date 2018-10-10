#include "acutest.h"
#include "decompressorpredictor.h"

void test_new (void) {
  //TEST NEW HERE
}

void test_select (void) {
  DecompressorPredictor *p = malloc(sizeof(*p));
  DP_New(p);
  TEST_CHECK(p->code == TEXT1);
}

TEST_LIST = {
    { "new_dp", test_new },
    { "select", test_select },
    { NULL, NULL }
};
