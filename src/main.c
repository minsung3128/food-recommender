#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "food.h"

int main(void) {
    SetConsoleOutputCP(65001);

    Food *foods = NULL;
    int n = load_foods("data/foods.txt", &foods);
    if (n < 0) {
        printf("Failed to load data/foods.txt\n");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        printf("%d. %s | %s | %s | %s | %d kcal\n", i + 1,
               foods[i].name, foods[i].category,
               foods[i].ingredient, foods[i].cooking, foods[i].kcal);
    }

    free(foods);
    return 0;
}