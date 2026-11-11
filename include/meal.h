#ifndef MEAL_H
#define MEAL_H

#include "food.h"

typedef struct {
    char date[11];          /* "YYYY-MM-DD" */
    char food_name[MAX_NAME];
    int rating;             /* 0 = not rated, 1~5 */
} Meal;

/* Loads meals into a malloc'd array. Returns count (0 if no file yet), -1 on error. */
int load_meals(const char *path, Meal **out);

/* Appends one meal to the file. Returns 0 on success. */
int append_meal(const char *path, const Meal *m);

#endif