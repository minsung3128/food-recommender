#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exclude.h"

static const char *KIND_WORDS[] = { "category", "ingredient", "cooking" };

const char *food_attr(const Food *f, int kind) {
    switch (kind) {
    case EX_CATEGORY:   return f->category;
    case EX_INGREDIENT: return f->ingredient;
    default:            return f->cooking;
    }
}

int find_exclusion(const Exclusion *ex, int n, int kind, const char *value) {
    for (int i = 0; i < n; i++)
        if (ex[i].kind == kind && strcmp(ex[i].value, value) == 0) return i;
    return -1;
}

int load_exclusions(const char *path, Exclusion **out) {
    *out = NULL;
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;   /* no settings yet */

    int cap = 8, n = 0;
    Exclusion *arr = malloc(cap * sizeof(Exclusion));
    if (!arr) { fclose(fp); return -1; }

    char line[128];
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';

        char word[16], value[MAX_ATTR];
        if (sscanf(line, "%15[^,],%31[^,]", word, value) != 2) continue;

        int kind = -1;
        for (int k = 0; k < 3; k++)
            if (strcmp(word, KIND_WORDS[k]) == 0) kind = k;
        if (kind < 0) continue;
        if (find_exclusion(arr, n, kind, value) >= 0) continue;

        if (n == cap) {
            cap *= 2;
            Exclusion *tmp = realloc(arr, cap * sizeof(Exclusion));
            if (!tmp) { free(arr); fclose(fp); return -1; }
            arr = tmp;
        }
        arr[n].kind = kind;
        snprintf(arr[n].value, MAX_ATTR, "%s", value);
        n++;
    }

    fclose(fp);
    *out = arr;
    return n;
}

int save_exclusions(const char *path, const Exclusion *ex, int n) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    for (int i = 0; i < n; i++)
        fprintf(fp, "%s,%s\n", KIND_WORDS[ex[i].kind], ex[i].value);
    fclose(fp);
    return 0;
}

int set_exclusion(Exclusion **ex, int *n, int kind, const char *value, int on) {
    int i = find_exclusion(*ex, *n, kind, value);
    if (on && i < 0) {
        Exclusion *tmp = realloc(*ex, (*n + 1) * sizeof(Exclusion));
        if (!tmp) return -1;
        *ex = tmp;
        (*ex)[*n].kind = kind;
        snprintf((*ex)[*n].value, MAX_ATTR, "%s", value);
        (*n)++;
    } else if (!on && i >= 0) {
        memmove(&(*ex)[i], &(*ex)[i + 1], (*n - i - 1) * sizeof(Exclusion));
        (*n)--;
    }
    return 0;
}

int is_excluded(const Food *f, const Exclusion *ex, int n) {
    for (int i = 0; i < n; i++)
        if (strcmp(food_attr(f, ex[i].kind), ex[i].value) == 0) return 1;
    return 0;
}

int recommend_filtered(const Food *foods, int nf, const Meal *meals, int nm,
                       const Exclusion *ex, int nex, Score *out_top, int top_k) {
    if (nf <= 0) return 0;

    /* Ask for every food in score order, then keep the first top_k that
       are not excluded. */
    Score *all = malloc(nf * sizeof(Score));
    if (!all) return -1;
    int k = recommend(foods, nf, meals, nm, all, nf);
    if (k < 0) { free(all); return -1; }

    int n = 0;
    for (int i = 0; i < k && n < top_k; i++)
        if (!is_excluded(&foods[all[i].food_index], ex, nex))
            out_top[n++] = all[i];

    free(all);
    return n;
}