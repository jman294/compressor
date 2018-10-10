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

void test_enumerate_models (void) {
  Model * text1 = malloc(sizeof(*text1));
  MO_New(text1, TEXT1);
  Model * text2 = malloc(sizeof(*text1));
  MO_New(text2, TEXT2);
  Model * mos[2] = malloc(sizeof(Model * mos[2]));
  S_MO_EnumerateAllModels(mos);
}


TEST_LIST = {
    { "new_m", test_new },
    { "test_range", test_range },
    { "test_enumerate_models", test_enumerate_models },
    { NULL, NULL }
};
