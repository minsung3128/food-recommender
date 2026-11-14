#ifndef EXCLUDE_H
#define EXCLUDE_H

#include "food.h"
#include "meal.h"
#include "recommend.h"

enum { EX_CATEGORY = 0, EX_INGREDIENT = 1, EX_COOKING = 2 };

typedef struct {
    int kind;                 /* EX_CATEGORY, EX_INGREDIENT or EX_COOKING */
    char value[MAX_ATTR];     /* e.g. "해산물" */
} Exclusion;

/* The attribute of a food that belongs to the given kind. */
const char *food_attr(const Food *f, int kind);

/* Loads exclusions into a malloc'd array. Returns count (0 if no file), -1 on error. */
int load_exclusions(const char *path, Exclusion **out);

/* Rewrites the whole file. Returns 0 on success. */
int save_exclusions(const char *path, const Exclusion *ex, int n);

/* Returns the index of (kind, value) in the list, or -1. */
int find_exclusion(const Exclusion *ex, int n, int kind, const char *value);

/* Adds (on=1) or removes (on=0) one exclusion. Returns 0 on success. */
int set_exclusion(Exclusion **ex, int *n, int kind, const char *value, int on);

/* 1 if any attribute of the food is excluded. */
int is_excluded(const Food *f, const Exclusion *ex, int n);

/* Like recommend(), but skips excluded foods. Returns the count, -1 on error. */
int recommend_filtered(const Food *foods, int nf, const Meal *meals, int nm,
                       const Exclusion *ex, int nex, Score *out_top, int top_k);

#endif