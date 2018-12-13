#include "acutest.h"

#include "decompressorpredictor.h"
#include "model.h"
#include "modelenum.h"
#include "util.h"

void test_new (void) {
  DecompressorPredictor *p = malloc(sizeof(*p));
  ModelArray_t mos = malloc(sizeof(mos));
  S_MO_EnumerateAllModels(mos);
  DP_New(p, mos, NUM_MODELS, 0);
  TEST_CHECK(p->ctx == 0);
}

void test_select (void) {
  DecompressorPredictor *p = malloc(sizeof(*p));
  ModelArray_t mos = malloc(sizeof(mos));
  S_MO_EnumerateAllModels(mos);

  DP_New(p, mos, NUM_MODELS, 0);
  /*Model * m = malloc(sizeof(*m));*/
  /*MO_New(m, TEXT1);*/
  DP_SelectModel(p, TEXT1);
  TEST_CHECK(p->currentModel->code == TEXT1);

  /*MO_New(m, TEXT2);*/
  DP_SelectModel(p, TEXT2);
  TEST_CHECK(p->currentModel->code == TEXT2);
}

void test_predict (void) {
  DecompressorPredictor *p = malloc(sizeof(*p));
  ModelArray_t mos = malloc(sizeof(mos));
  S_MO_EnumerateAllModels(mos);
  context ctx = 0;
  DP_New(p, mos, NUM_MODELS, ctx);

  Model * m = malloc(sizeof(*m));
  MO_New(m, TEXT1);
  DP_SelectModel(p, TEXT1);

  int prediction = DP_Predict(p);
  int mPrediction = MO_GetPrediction(m, ctx);

  TEST_CHECK(prediction == mPrediction);
  TEST_CHECK(prediction >= 0);
}

void test_update (void) {
  DecompressorPredictor *dp = malloc(sizeof(*dp));
  context cxt = 0;
  DP_New(dp, NULL, NUM_MODELS, 0);
  TEST_CHECK(dp->ctx == cxt);

  DP_Update(dp, 0);
  cxt = (cxt << 1) | 0;
  TEST_CHECK_(dp->ctx == cxt, "The context did not update with a zero");

  DP_Update(dp, 1);
  cxt = (cxt << 1) | 1;
  TEST_CHECK_(dp->ctx == cxt, "The context did not update with a one");

  TEST_CHECK(dp->ctx == cxt);
}

TEST_LIST = {
    { "new_dp", test_new },
    { "select", test_select },
    { "predict", test_predict },
    { "update", test_update },
    { NULL, NULL }
};
