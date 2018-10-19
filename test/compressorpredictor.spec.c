#include "acutest.h"
#include "util.h"
#include "compressorpredictor.h"
#include "model.h"
#include "modelenum.h"

void test_new (void) {
  CompressorPredictor * cp = malloc(sizeof(CompressorPredictor));
  context cxt = 0;
  ModelArray_t mos = malloc(sizeof(mos));
  S_MO_EnumerateAllModels(mos);

  CP_New(cp, NULL, 0, cxt);
  TEST_CHECK(cp->ctx == cxt);
  TEST_CHECK(cp->models == NULL);
  TEST_CHECK(cp->modelCount == 0);

  cp = malloc(sizeof(CompressorPredictor));
  CP_New(cp, mos, NUM_MODELS, cxt);
  TEST_CHECK(cp->ctx == cxt);
  TEST_CHECK(cp->models == mos);
  TEST_CHECK(cp->modelCount == NUM_MODELS);

  TEST_CHECK(cp->models[0] == mos[0]);
}

void test_update (void) {
  CompressorPredictor * cp = malloc(sizeof(CompressorPredictor));
  context cxt = 0;
  CP_New(cp, NULL, 0, cxt);
  TEST_CHECK(cp->ctx == cxt);

  CP_Update(cp, 0);
  cxt = (cxt << 1) | 0;
  TEST_CHECK_(cp->ctx == cxt, "The context did not update with a zero");

  CP_Update(cp, 1);
  cxt = (cxt << 1) | 1;
  TEST_CHECK_(cp->ctx == cxt, "The context did not update with a one");

  TEST_CHECK(cp->ctx == cxt);
}

void test_predict (void) {
  CompressorPredictor *p = malloc(sizeof(*p));
  ModelArray_t mos = malloc(sizeof(mos));
  S_MO_EnumerateAllModels(mos);
  CP_New(p, mos, NUM_MODELS, 0);
  int prediction = CP_Predict(p);

  TEST_CHECK(prediction >= 0);
}

void test_select_model (void) {
  CompressorPredictor *p = malloc(sizeof(*p));
  CP_New(p, NULL, 0, 0);
  Model * m = malloc(sizeof(*m));
  MO_New(m, TEXT1);

  CP_SelectModel(p, m);
  TEST_CHECK(p->currentModel->code == TEXT1);

  m = malloc(sizeof(*m));
  MO_New(m, TEXT2);
  CP_SelectModel(p, m);
  TEST_CHECK(p->currentModel->code == TEXT2);
}

TEST_LIST = {
    { "new_cp", test_new },
    { "update", test_update },
    { "prediction", test_predict },
    { "select_model", test_select_model },
    { NULL, NULL }
};
