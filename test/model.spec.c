#include "acutest.h"
#include "model.h"
#include "modelenum.h"
#include "util.h"

void test_new (void) {
  Model * m = malloc(sizeof(*m));
  MO_New(m, TEXT1);
  TEST_CHECK(m->code == TEXT1);
  TEST_CHECK((*m->data)[5] == TEXT1_Data[5]);
}

void test_get_prediction (void) {
  Model * m = malloc(sizeof(*m));
  MO_New(m, TEXT1);

  context expContext = 0;
  int prediction = MO_GetPrediction(m, expContext);
  TEST_CHECK(prediction >= 0 && prediction <= MODEL_LIMIT);
  TEST_CHECK(prediction == TEXT1_Data[expContext]);
}

void test_setdata (void) {
  Model * m = malloc(sizeof(*m));
  MO_New(m, TEXT1);

  TEST_CHECK((*m->data)[0] == TEXT1_Data[0]);

  MO_SetData(m, &TEXT1_Data);
  TEST_CHECK((*m->data)[0] == TEXT1_Data[0]);

  MO_SetData(m, &TEXT2_Data);
  TEST_CHECK((*m->data)[0] == TEXT2_Data[0]);
}

void test_enumerate_models (void) {
  Model * text1 = malloc(sizeof(*text1));
  MO_New(text1, TEXT1);
  Model * text2 = malloc(sizeof(*text2));
  MO_New(text2, TEXT2);
  MO_SetData(text2, &TEXT2_Data);
  ModelArray_t mos1 = malloc(sizeof(Model * (*)[2]));
  (*mos1)[0] = text1;
  (*mos1)[1] = text2;

  ModelArray_t mos2 = malloc(sizeof(Model * (*)[2]));
  S_MO_EnumerateAllModels(mos2);

  for (int i = 0; i < sizeof(*mos1)/sizeof((*mos1)[0]); i++) {
    TEST_CHECK((*mos1)[i]->code == (*mos2)[i]->code);
    TEST_CHECK((*(*mos1)[i]->data)[0] == (*(*mos2)[i]->data)[0]); // TODO Fix this s***
  }
}

TEST_LIST = {
    { "new_m", test_new },
    { "test_get_prediction", test_get_prediction },
    { "test_setdata", test_setdata },
    { "test_enumerate_models", test_enumerate_models },
    { NULL, NULL }
};
