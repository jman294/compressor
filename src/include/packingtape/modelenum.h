#ifndef MODELENUM_H_   /* Include guard */
#define MODELENUM_H_

#include "model.h"

// NOTE This is an enumeration of all available models
// Constants correspond to that model's index in the enumerated arrat of models

#define NUM_MODELS 2

#define TEXT1 0
static const ModelData_t TEXT1_Data = {
  300, 4000, 400, 600, 200, 300, 300, 4000, 400, 600, 200, 300, 300, 4000, 400, 600, 200, 300, 300, 4000, 400, 600, 200, 300
};

#define TEXT2 1
static const ModelData_t TEXT2_Data = {
  600, 95, 1000, 450, 111, 400, 400, 400, 400, 600, 200, 300, 300, 400, 400, 600, 200, 300, 300, 400, 400, 4000, 200, 300
};

#endif // MODELENUM_H_
