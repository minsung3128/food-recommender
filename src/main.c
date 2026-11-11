#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include "food.h"
#include "meal.h"

#define FOODS_PATH   "data/foods.txt"
#define HISTORY_PATH "data/history.txt"

static int read_int(const char *prompt) {
    char buf[64];
    int v;
    printf("%s", prompt);
    if (!fgets(buf, sizeof buf, stdin)) return -1;
    if (sscanf(buf, "%d", &v) != 1) return -1;
    return v;
}

static void today(char *out) {
    time_t t = time(NULL);
    strftime(out, 11, "%Y-%m-%d", localtime(&t));
}

static void list_foods(const Food *foods, int n) {
    for (int i = 0; i < n; i++)
        printf("%2d. %s | %s | %s | %s | %d kcal\n", i + 1,
               foods[i].name, foods[i].category,
               foods[i].ingredient, foods[i].cooking, foods[i].kcal);
}

static void record_meal(const Food *foods, int n) {
    list_foods(foods, n);
    int idx = read_int("먹은 음식 번호: ");
    if (idx < 1 || idx > n) { printf("잘못된 번호입니다.\n"); return; }

    int rating = read_int("별점 (1~5, 0=나중에): ");
    if (rating < 0 || rating > 5) { printf("별점은 0~5입니다.\n"); return; }

    Meal m;
    char buf[64];
    printf("날짜 (엔터=오늘, 예: 2026-11-10): ");
    if (!fgets(buf, sizeof buf, stdin)) return;
    buf[strcspn(buf, "\r\n")] = '\0';
    int y, mo, d;
    if (buf[0] == '\0') today(m.date);
    else if (strlen(buf) == 10 && sscanf(buf, "%d-%d-%d", &y, &mo, &d) == 3)
        strcpy(m.date, buf);
    else { printf("날짜 형식이 잘못되었습니다.\n"); return; }

    strcpy(m.food_name, foods[idx - 1].name);
    m.rating = rating;

    if (append_meal(HISTORY_PATH, &m) == 0) printf("기록했습니다.\n");
    else printf("저장에 실패했습니다.\n");
}

static void show_history(void) {
    Meal *meals = NULL;
    int n = load_meals(HISTORY_PATH, &meals);
    if (n <= 0) { printf("기록이 없습니다.\n"); free(meals); return; }
    for (int i = 0; i < n; i++)
        printf("%s | %s | 별점 %d\n", meals[i].date, meals[i].food_name, meals[i].rating);
    free(meals);
}

int main(void) {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    Food *foods = NULL;
    int n = load_foods(FOODS_PATH, &foods);
    if (n < 0) { printf("Failed to load %s\n", FOODS_PATH); return 1; }

    for (;;) {
        printf("\n[음식 추천 프로그램]\n");
        printf("1. 음식 목록\n2. 식사 기록 추가\n3. 식사 기록 보기\n0. 종료\n");
        int menu = read_int("선택: ");
        if (menu == 0) break;
        else if (menu == 1) list_foods(foods, n);
        else if (menu == 2) record_meal(foods, n);
        else if (menu == 3) show_history();
        else printf("잘못된 선택입니다.\n");
    }

    free(foods);
    return 0;
}