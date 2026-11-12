#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "meal.h"

int load_meals(const char *path, Meal **out) {
    *out = NULL;
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;   /* no history yet */

    int cap = 16, n = 0;
    Meal *arr = malloc(cap * sizeof(Meal));
    if (!arr) { fclose(fp); return -1; }

    char line[256];
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        Meal m;
        if (sscanf(line, "%10[^,],%63[^,],%d", m.date, m.food_name, &m.rating) != 3)
            continue;

        if (n == cap) {
            cap *= 2;
            Meal *tmp = realloc(arr, cap * sizeof(Meal));
            if (!tmp) { free(arr); fclose(fp); return -1; }
            arr = tmp;
        }
        arr[n++] = m;
    }

    fclose(fp);
    *out = arr;
    return n;
}

int append_meal(const char *path, const Meal *m) {
    FILE *fp = fopen(path, "a");
    if (!fp) return -1;
    fprintf(fp, "%s,%s,%d\n", m->date, m->food_name, m->rating);
    fclose(fp);
    return 0;
}

int save_meals(const char *path, const Meal *meals, int n) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    for (int i = 0; i < n; i++)
        fprintf(fp, "%s,%s,%d\n", meals[i].date, meals[i].food_name, meals[i].rating);
    fclose(fp);
    return 0;
}