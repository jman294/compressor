#include "acutest.h"

#include "decompressorpredictor.h"
#include "model.h"
#include "modelenum.h"

void test_new (void) {
  DecompressorPredictor *p = malloc(sizeof(*p));
  ModelArray_t mos = malloc(sizeof(mos));
  S_MO_EnumerateAllModels(mos);
  DP_New(p, mos, 0);
}

void test_select (void) {
  DecompressorPredictor *p = malloc(sizeof(*p));
  DP_New(p, NULL, 0);
  Model * m = malloc(sizeof(*m));
  MO_New(m, TEXT1);
  DP_SelectModel(p, m);
  TEST_CHECK(p->m->code == m->code);
}

TEST_LIST = {
    { "new_dp", test_new },
    { "select", test_select },
    { NULL, NULL }
};
