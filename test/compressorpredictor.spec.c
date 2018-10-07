#include "acutest.h"
#include "util.h"
#include "compressorpredictor.h"

void test_new (void) {
  CompressorPredictor * cp = malloc(sizeof(CompressorPredictor));
  context cxt = 0;
  CP_New(cp, NULL, cxt);
  TEST_CHECK(cp->ctx == cxt);
}

void test_update (void) {
  CompressorPredictor * cp = malloc(sizeof(CompressorPredictor));
  context cxt = 0;
  CP_New(cp, NULL, cxt);
  TEST_CHECK(cp->ctx == cxt);

  CP_Update(cp, 0);
  cxt = (cxt << 1) | 0;
  TEST_CHECK_(cp->ctx == cxt, "The context did not update with a zero");

  CP_Update(cp, 1);
  cxt = (cxt << 1) | 1;
  TEST_CHECK_(cp->ctx == cxt, "The context did not update with a one");

  TEST_CHECK(cp->ctx == cxt);
}

TEST_LIST = {
    { "new_cp", test_new },
    { "update", test_update },
    { NULL, NULL }
};
