#ifndef FOOD_H
#define FOOD_H

#define MAX_NAME 64
#define MAX_ATTR 32

typedef struct {
    char name[MAX_NAME];
    char category[MAX_ATTR];
    char ingredient[MAX_ATTR];
    char cooking[MAX_ATTR];
    int kcal;
} Food;

/* Reads foods from a CSV file into a malloc'd array.
   Returns the number of foods, or -1 on error. Caller must free(*out). */
int load_foods(const char *path, Food **out);

/* Writes all foods to a CSV file (overwrites). Returns 0 on success. */
int save_foods(const char *path, const Food *foods, int n);

#endif