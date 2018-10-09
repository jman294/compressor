#include "acutest.h"
#include "model.h"
#include "modelenum.h"
#include "util.h"

void test_new (void) {
  Model * m = malloc(sizeof(Model));
  MO_New(m, TEXT1);
  TEST_CHECK(m->code == 0);
}

void test_range (void) {
  Model * m = malloc(sizeof(Model));
  MO_New(m, TEXT1);

  uint prediction = MO_GetPrediction(m, 0);
  TEST_CHECK(prediction >= 0 && prediction <= 4095);
}

TEST_LIST = {
    { "new_m", test_new },
    { "test_range", test_range },
    { NULL, NULL }
};
