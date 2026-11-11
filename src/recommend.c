#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "recommend.h"

#define ALPHA            0.1   /* learning rate per matching attribute */
#define W_SAME_FOOD      0.3   /* extra weight for rating of the same food */
#define PEN_Y_ATTR       0.5   /* yesterday: per matching attribute */
#define PEN_Y_SAME       2.0   /* yesterday: same food */
#define PEN_RECENT_SAME  1.0   /* 2~RECENT_DAYS days ago: same food */
#define RECENT_DAYS      3
#define BONUS_EXPLORE    0.5

static void date_str(char *out, int days_ago) {
    time_t t = time(NULL) - (time_t)days_ago * 86400;
    strftime(out, 11, "%Y-%m-%d", localtime(&t));
}

/* Returns how many days ago the date was (0..30), or -1 if outside. */
static int days_ago_of(const char *date) {
    char buf[11];
    for (int d = 0; d <= 30; d++) {
        date_str(buf, d);
        if (strcmp(buf, date) == 0) return d;
    }
    return -1;
}

static const Food *find_food(const Food *foods, int nf, const char *name) {
    for (int i = 0; i < nf; i++)
        if (strcmp(foods[i].name, name) == 0) return &foods[i];
    return NULL;
}

static int cmp_desc(const void *a, const void *b) {
    double x = ((const Score *)a)->total;
    double y = ((const Score *)b)->total;
    return (x < y) - (x > y);
}

int recommend(const Food *foods, int nf, const Meal *meals, int nm,
              Score *out_top, int top_k) {
    if (nf <= 0) return 0;
    Score *all = calloc(nf, sizeof(Score));
    if (!all) return -1;

    for (int i = 0; i < nf; i++) {
        const Food *f = &foods[i];
        Score *s = &all[i];
        s->food_index = i;
        int eaten = 0;

        for (int j = 0; j < nm; j++) {
            const Food *mf = find_food(foods, nf, meals[j].food_name);
            if (!mf) continue;

            int same = (strcmp(mf->name, f->name) == 0);
            int attr = (strcmp(mf->category, f->category) == 0)
                     + (strcmp(mf->ingredient, f->ingredient) == 0)
                     + (strcmp(mf->cooking, f->cooking) == 0);
            if (same) eaten = 1;

            if (meals[j].rating > 0) {
                double delta = meals[j].rating - 3;
                s->taste += ALPHA * delta * attr;
                if (same) s->taste += W_SAME_FOOD * delta;
            }

            int ago = days_ago_of(meals[j].date);
            if (ago == 1) {
                s->yesterday -= PEN_Y_ATTR * attr;
                if (same) s->yesterday -= PEN_Y_SAME;
            } else if (ago >= 2 && ago <= RECENT_DAYS && same) {
                s->recent -= PEN_RECENT_SAME;
            }
        }

        if (!eaten) s->explore = BONUS_EXPLORE;
        s->total = s->taste + s->yesterday + s->recent + s->explore;
    }

    qsort(all, nf, sizeof(Score), cmp_desc);

    int k = (top_k < nf) ? top_k : nf;
    for (int i = 0; i < k; i++) out_top[i] = all[i];
    free(all);
    return k;
}