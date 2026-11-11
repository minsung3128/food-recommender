#ifndef RECOMMEND_H
#define RECOMMEND_H

#include "food.h"
#include "meal.h"

typedef struct {
    int food_index;
    double taste;      /* learned preference from ratings */
    double yesterday;  /* penalty for similarity to yesterday's meal */
    double recent;     /* penalty for eating the same food in recent days */
    double explore;    /* bonus for foods never eaten */
    double total;
} Score;

/* Fills out_top with up to top_k best foods. Returns the count, -1 on error. */
int recommend(const Food *foods, int nf, const Meal *meals, int nm,
              Score *out_top, int top_k);

#endif