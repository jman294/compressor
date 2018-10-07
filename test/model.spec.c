#include "acutest.h"
#include "model.h"

void test_new (void) {
  Model * m = malloc(sizeof(Model));
  MO_New(m, 0);
  TEST_CHECK(m->code == 0);
}

TEST_LIST = {
    { "new_m", test_new },
    { NULL, NULL }
};
