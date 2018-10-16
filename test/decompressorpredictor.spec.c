#include "acutest.h"

#include "decompressorpredictor.h"
#include "model.h"
#include "modelenum.h"

void test_new (void) {
  //TEST NEW HERE
  DecompressorPredictor *p = malloc(sizeof(*p));
  ModelArray_t mos = malloc(sizeof(mos));
  S_MO_EnumerateAllModels(mos);
  DP_New(p, mos, 0);
}

void test_select (void) {
  DecompressorPredictor *p = malloc(sizeof(*p));
  /*DP_New(p);*/
  /*TEST_CHECK(p->code == TEXT1);*/
}

TEST_LIST = {
    { "new_dp", test_new },
    { "select", test_select },
    { NULL, NULL }
};
