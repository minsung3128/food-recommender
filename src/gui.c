#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "food.h"
#include "meal.h"
#include "recommend.h"

#define FOODS_PATH   "data/foods.txt"
#define HISTORY_PATH "data/history.txt"

enum { ID_LIST = 100, ID_RATING, ID_DATE, ID_ADD, ID_REC, ID_HIST };

static Food *g_foods = NULL;
static int g_nf = 0;
static HWND hList, hRating, hDate, hOut;

static void date_offset(char *out, int days_ago) {
    time_t t = time(NULL) - (time_t)days_ago * 86400;
    strftime(out, 11, "%Y-%m-%d", localtime(&t));
}

/* UTF-8 -> newly allocated UTF-16 string (caller frees) */
static wchar_t *to_wide(const char *s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = malloc(len * sizeof(wchar_t));
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, len);
    return w;
}

static void set_text(HWND h, const char *s) {
    wchar_t *w = to_wide(s);
    if (!w) return;
    SetWindowTextW(h, w);
    free(w);
}

static void list_add(HWND h, const char *s) {
    wchar_t *w = to_wide(s);
    if (!w) return;
    SendMessageW(h, LB_ADDSTRING, 0, (LPARAM)w);
    free(w);
}

static void on_add(void) {
    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        set_text(hOut, "먼저 왼쪽 목록에서 먹은 음식을 클릭하세요.");
        return;
    }
    int rating = (int)SendMessageW(hRating, CB_GETCURSEL, 0, 0);
    int ago = (int)SendMessageW(hDate, CB_GETCURSEL, 0, 0);
    if (rating == CB_ERR) rating = 0;
    if (ago == CB_ERR) ago = 0;

    Meal m;
    date_offset(m.date, ago);
    strcpy(m.food_name, g_foods[sel].name);
    m.rating = rating;

    char msg[256];
    if (append_meal(HISTORY_PATH, &m) == 0)
        snprintf(msg, sizeof msg, "기록했습니다: %s | %s | 별점 %d",
                 m.date, m.food_name, m.rating);
    else
        snprintf(msg, sizeof msg, "저장에 실패했습니다.");
    set_text(hOut, msg);
}

static void on_rec(void) {
    Meal *meals = NULL;
    int nm = load_meals(HISTORY_PATH, &meals);
    if (nm < 0) { set_text(hOut, "기록을 불러오지 못했습니다."); return; }

    Score top[3];
    int k = recommend(g_foods, g_nf, meals, nm, top, 3);
    if (k <= 0) { set_text(hOut, "추천할 음식이 없습니다."); free(meals); return; }

    char buf[2048];
    int pos = snprintf(buf, sizeof buf, "기록 %d건 기준 오늘의 추천\r\n\r\n", nm);
    for (int i = 0; i < k; i++) {
        const Food *f = &g_foods[top[i].food_index];
        pos += snprintf(buf + pos, sizeof buf - pos,
            "%d위: %s (%s, %s, %s)  총점 %.2f\r\n"
            "     취향 %+.2f | 어제 %+.2f | 최근 %+.2f | 탐험 %+.2f\r\n\r\n",
            i + 1, f->name, f->category, f->ingredient, f->cooking, top[i].total,
            top[i].taste, top[i].yesterday, top[i].recent, top[i].explore);
    }
    set_text(hOut, buf);
    free(meals);
}

static void on_hist(void) {
    Meal *meals = NULL;
    int nm = load_meals(HISTORY_PATH, &meals);
    if (nm <= 0) { set_text(hOut, "기록이 없습니다."); free(meals); return; }

    size_t cap = (size_t)nm * 160 + 64;
    char *buf = malloc(cap);
    if (!buf) { free(meals); return; }
    size_t pos = 0;
    buf[0] = '\0';
    for (int i = 0; i < nm; i++)
        pos += snprintf(buf + pos, cap - pos, "%s | %s | 별점 %d\r\n",
                        meals[i].date, meals[i].food_name, meals[i].rating);
    set_text(hOut, buf);
    free(buf);
    free(meals);
}

static BOOL CALLBACK set_font_proc(HWND h, LPARAM lp) {
    SendMessageW(h, WM_SETFONT, (WPARAM)lp, TRUE);
    return TRUE;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_nf = load_foods(FOODS_PATH, &g_foods);
        if (g_nf < 0) {
            MessageBoxW(hwnd, L"data/foods.txt를 불러오지 못했습니다.\n"
                              L"프로젝트 폴더에서 실행했는지 확인하세요.",
                        L"오류", MB_OK | MB_ICONERROR);
            return -1;
        }

        CreateWindowExW(0, L"STATIC", L"음식 목록 (클릭해서 선택)",
            WS_CHILD | WS_VISIBLE, 10, 8, 300, 20, hwnd, NULL, NULL, NULL);
        hList = CreateWindowExW(0, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
            10, 30, 310, 380, hwnd, (HMENU)(INT_PTR)ID_LIST, NULL, NULL);

        CreateWindowExW(0, L"STATIC", L"별점",
            WS_CHILD | WS_VISIBLE, 340, 8, 100, 20, hwnd, NULL, NULL, NULL);
        hRating = CreateWindowExW(0, L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
            340, 30, 150, 200, hwnd, (HMENU)(INT_PTR)ID_RATING, NULL, NULL);

        CreateWindowExW(0, L"STATIC", L"날짜",
            WS_CHILD | WS_VISIBLE, 510, 8, 100, 20, hwnd, NULL, NULL, NULL);
        hDate = CreateWindowExW(0, L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
            510, 30, 150, 200, hwnd, (HMENU)(INT_PTR)ID_DATE, NULL, NULL);

        CreateWindowExW(0, L"BUTTON", L"식사 기록 추가",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            340, 75, 120, 34, hwnd, (HMENU)(INT_PTR)ID_ADD, NULL, NULL);
        CreateWindowExW(0, L"BUTTON", L"오늘의 추천 보기",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            470, 75, 130, 34, hwnd, (HMENU)(INT_PTR)ID_REC, NULL, NULL);
        CreateWindowExW(0, L"BUTTON", L"기록 보기",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            610, 75, 110, 34, hwnd, (HMENU)(INT_PTR)ID_HIST, NULL, NULL);

        CreateWindowExW(0, L"STATIC", L"결과",
            WS_CHILD | WS_VISIBLE, 340, 125, 100, 20, hwnd, NULL, NULL, NULL);
        hOut = CreateWindowExW(0, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            340, 147, 380, 263, hwnd, NULL, NULL, NULL);

        static const wchar_t *ratings[] = {
            L"0 (나중에 평가)", L"1", L"2", L"3", L"4", L"5" };
        for (int i = 0; i < 6; i++)
            SendMessageW(hRating, CB_ADDSTRING, 0, (LPARAM)ratings[i]);
        SendMessageW(hRating, CB_SETCURSEL, 0, 0);

        static const wchar_t *dates[] = { L"오늘", L"어제", L"2일 전" };
        for (int i = 0; i < 3; i++)
            SendMessageW(hDate, CB_ADDSTRING, 0, (LPARAM)dates[i]);
        SendMessageW(hDate, CB_SETCURSEL, 0, 0);

        for (int i = 0; i < g_nf; i++) {
            char line[256];
            snprintf(line, sizeof line, "%s | %s | %s | %s | %d kcal",
                     g_foods[i].name, g_foods[i].category,
                     g_foods[i].ingredient, g_foods[i].cooking, g_foods[i].kcal);
            list_add(hList, line);
        }

        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
        EnumChildWindows(hwnd, set_font_proc, (LPARAM)font);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ADD:  on_add();  break;
        case ID_REC:  on_rec();  break;
        case ID_HIST: on_hist(); break;
        }
        return 0;
    case WM_DESTROY:
        free(g_foods);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    (void)hPrev; (void)cmd;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"FoodRecommenderGui";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"음식 추천 프로그램",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 750, 480, NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}