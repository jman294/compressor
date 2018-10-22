#include "acutest.h"
#include "model.h"
#include "modelenum.h"
#include "util.h"

void test_new (void) {
  Model * m = malloc(sizeof(*m));
  MO_New(m, TEXT1);
  TEST_CHECK(m->code == 0);
}

void test_range (void) {
  Model * m = malloc(sizeof(*m));
  MO_New(m, TEXT1);

  uint prediction = MO_GetPrediction(m, 0);
  TEST_CHECK(prediction >= 0 && prediction <= MODEL_LIMIT);
}

void test_enumerate_models (void) {
  Model * text1 = malloc(sizeof(*text1));
  MO_New(text1, TEXT1);
  Model * text2 = malloc(sizeof(*text2));
  MO_New(text2, TEXT2);
  ModelArray_t mos1 = malloc(sizeof(Model * (*)[2]));
  (*mos1)[0] = text1;
  (*mos1)[1] = text2;

  ModelArray_t mos2 = malloc(sizeof(Model * (*)[2]));
  S_MO_EnumerateAllModels(mos2);

  for (int i = 0; i < sizeof(*mos1)/sizeof((*mos1)[0]); i++) {
    TEST_CHECK((*mos1)[i]->code == (*mos2)[i]->code);
  }
}


TEST_LIST = {
    { "new_m", test_new },
    { "test_range", test_range },
    { "test_enumerate_models", test_enumerate_models },
    { NULL, NULL }
};
