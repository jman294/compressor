#ifndef MODEL_H_   /* Include guard */
#define MODEL_H_

typedef struct Model {
} Model;

void MO_New (Model * m);

int MO_GetPrediction (Model * m, int context);

#endif // MODEL_H_
