#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "food.h"

int load_foods(const char *path, Food **out) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    int cap = 16, n = 0;
    Food *arr = malloc(cap * sizeof(Food));
    if (!arr) { fclose(fp); return -1; }

    char line[256];
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        Food f;
        if (sscanf(line, "%63[^,],%31[^,],%31[^,],%31[^,],%d",
                   f.name, f.category, f.ingredient, f.cooking, &f.kcal) != 5)
            continue;

        if (n == cap) {
            cap *= 2;
            Food *tmp = realloc(arr, cap * sizeof(Food));
            if (!tmp) { free(arr); fclose(fp); return -1; }
            arr = tmp;
        }
        arr[n++] = f;
    }

    fclose(fp);
    *out = arr;
    return n;
}

int save_foods(const char *path, const Food *foods, int n) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    for (int i = 0; i < n; i++)
        fprintf(fp, "%s,%s,%s,%s,%d\n", foods[i].name, foods[i].category,
                foods[i].ingredient, foods[i].cooking, foods[i].kcal);
    fclose(fp);
    return 0;
}