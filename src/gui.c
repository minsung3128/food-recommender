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

enum {
    ID_LIST = 100, ID_RATING, ID_DATE, ID_ADD, ID_REC, ID_HIST,
    ID_F_NAME, ID_F_CAT, ID_F_ING, ID_F_COOK, ID_F_KCAL,
    ID_FADD, ID_FEDIT, ID_FDEL
};

static Food *g_foods = NULL;
static int g_nf = 0;
static HWND hList, hRating, hDate, hOut;
static HWND hFName, hFCat, hFIng, hFCook, hFKcal;

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

/* ---------- food list helpers ---------- */

static void refresh_list(int select) {
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_nf; i++) {
        char line[256];
        snprintf(line, sizeof line, "%s | %s | %s | %s | %d kcal",
                 g_foods[i].name, g_foods[i].category,
                 g_foods[i].ingredient, g_foods[i].cooking, g_foods[i].kcal);
        list_add(hList, line);
    }
    if (select >= 0 && select < g_nf)
        SendMessageW(hList, LB_SETCURSEL, select, 0);
}

static void clear_fields(void) {
    set_text(hFName, "");
    set_text(hFCat, "");
    set_text(hFIng, "");
    set_text(hFCook, "");
    set_text(hFKcal, "");
}

static void fill_fields(int sel) {
    if (sel < 0 || sel >= g_nf) { clear_fields(); return; }
    char k[16];
    set_text(hFName, g_foods[sel].name);
    set_text(hFCat, g_foods[sel].category);
    set_text(hFIng, g_foods[sel].ingredient);
    set_text(hFCook, g_foods[sel].cooking);
    snprintf(k, sizeof k, "%d", g_foods[sel].kcal);
    set_text(hFKcal, k);
}

/* Reads an edit box as UTF-8. Returns 0 if empty, too long, or has a comma. */
static int get_field(HWND h, char *out, int cap) {
    wchar_t w[128];
    GetWindowTextW(h, w, 128);
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL) == 0)
        return 0;
    if (out[0] == '\0') return 0;
    if (strchr(out, ',')) return 0;
    return 1;
}

static int read_food_fields(Food *f) {
    char k[32];
    if (!get_field(hFName, f->name, MAX_NAME) ||
        !get_field(hFCat, f->category, MAX_ATTR) ||
        !get_field(hFIng, f->ingredient, MAX_ATTR) ||
        !get_field(hFCook, f->cooking, MAX_ATTR)) {
        set_text(hOut, "모든 칸을 입력하세요.\r\n"
                       "쉼표(,)는 쓸 수 없고, 이름은 20자, 나머지 칸은 10자 정도까지 입력할 수 있습니다.");
        return 0;
    }
    if (!get_field(hFKcal, k, sizeof k) || atoi(k) <= 0) {
        set_text(hOut, "칼로리는 1 이상의 숫자로 입력하세요.");
        return 0;
    }
    f->kcal = atoi(k);
    return 1;
}

static int find_by_name(const char *name, int skip) {
    for (int i = 0; i < g_nf; i++)
        if (i != skip && strcmp(g_foods[i].name, name) == 0) return i;
    return -1;
}

static void on_food_add(void) {
    Food f;
    if (!read_food_fields(&f)) return;
    if (find_by_name(f.name, -1) >= 0) {
        set_text(hOut, "같은 이름의 음식이 이미 있습니다.");
        return;
    }
    Food *tmp = realloc(g_foods, (g_nf + 1) * sizeof(Food));
    if (!tmp) { set_text(hOut, "메모리가 부족합니다."); return; }
    g_foods = tmp;
    g_foods[g_nf++] = f;

    refresh_list(g_nf - 1);
    char msg[256];
    if (save_foods(FOODS_PATH, g_foods, g_nf) == 0)
        snprintf(msg, sizeof msg, "추가했습니다: %s (총 %d개)", f.name, g_nf);
    else
        snprintf(msg, sizeof msg, "추가했지만 파일 저장에 실패했습니다.");
    set_text(hOut, msg);
}

static void on_food_edit(void) {
    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        set_text(hOut, "수정할 음식을 목록에서 먼저 클릭하세요.");
        return;
    }
    Food f;
    if (!read_food_fields(&f)) return;
    if (find_by_name(f.name, sel) >= 0) {
        set_text(hOut, "같은 이름의 음식이 이미 있습니다.");
        return;
    }
    int renamed = (strcmp(g_foods[sel].name, f.name) != 0);
    g_foods[sel] = f;

    refresh_list(sel);
    char msg[320];
    if (save_foods(FOODS_PATH, g_foods, g_nf) == 0) {
        snprintf(msg, sizeof msg, "수정했습니다: %s", f.name);
        if (renamed)
            strcat(msg, "\r\n이름을 바꾸면 이전 이름으로 남은 식사 기록은 추천에 반영되지 않습니다.");
    } else {
        snprintf(msg, sizeof msg, "수정했지만 파일 저장에 실패했습니다.");
    }
    set_text(hOut, msg);
}

static void on_food_del(void) {
    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        set_text(hOut, "삭제할 음식을 목록에서 먼저 클릭하세요.");
        return;
    }
    if (MessageBoxW(GetParent(hList), L"선택한 음식을 삭제할까요?", L"삭제 확인",
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    char name[MAX_NAME];
    strcpy(name, g_foods[sel].name);
    memmove(&g_foods[sel], &g_foods[sel + 1], (g_nf - sel - 1) * sizeof(Food));
    g_nf--;

    refresh_list(-1);
    clear_fields();
    char msg[256];
    if (save_foods(FOODS_PATH, g_foods, g_nf) == 0)
        snprintf(msg, sizeof msg, "삭제했습니다: %s (남은 음식 %d개)", name, g_nf);
    else
        snprintf(msg, sizeof msg, "삭제했지만 파일 저장에 실패했습니다.");
    set_text(hOut, msg);
}

/* ---------- meal history and recommendation ---------- */

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

/* ---------- window ---------- */

static BOOL CALLBACK set_font_proc(HWND h, LPARAM lp) {
    SendMessageW(h, WM_SETFONT, (WPARAM)lp, TRUE);
    return TRUE;
}

static HWND make_label(HWND p, const wchar_t *text, int x, int y, int w) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                           x, y, w, 20, p, NULL, NULL, NULL);
}

static HWND make_edit(HWND p, int id, int x, int y, int w, DWORD extra) {
    return CreateWindowExW(0, L"EDIT", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | extra,
        x, y, w, 24, p, (HMENU)(INT_PTR)id, NULL, NULL);
}

static HWND make_button(HWND p, const wchar_t *text, int id,
                        int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           x, y, w, h, p, (HMENU)(INT_PTR)id, NULL, NULL);
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

        make_label(hwnd, L"음식 목록 (클릭해서 선택)", 10, 8, 300);
        hList = CreateWindowExW(0, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
            10, 30, 310, 380, hwnd, (HMENU)(INT_PTR)ID_LIST, NULL, NULL);

        make_label(hwnd, L"별점", 340, 8, 100);
        hRating = CreateWindowExW(0, L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
            340, 30, 150, 200, hwnd, (HMENU)(INT_PTR)ID_RATING, NULL, NULL);

        make_label(hwnd, L"날짜", 510, 8, 100);
        hDate = CreateWindowExW(0, L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
            510, 30, 150, 200, hwnd, (HMENU)(INT_PTR)ID_DATE, NULL, NULL);

        make_button(hwnd, L"식사 기록 추가", ID_ADD, 340, 75, 120, 34);
        make_button(hwnd, L"오늘의 추천 보기", ID_REC, 470, 75, 130, 34);
        make_button(hwnd, L"기록 보기", ID_HIST, 610, 75, 110, 34);

        make_label(hwnd, L"결과", 340, 125, 100);
        hOut = CreateWindowExW(0, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            340, 147, 380, 263, hwnd, NULL, NULL, NULL);

        /* food management */
        make_label(hwnd, L"음식 관리 (목록에서 음식을 클릭하면 아래 칸에 채워집니다)",
                   10, 420, 600);
        make_label(hwnd, L"이름", 10, 445, 100);
        make_label(hwnd, L"분류", 170, 445, 90);
        make_label(hwnd, L"주재료", 270, 445, 90);
        make_label(hwnd, L"조리법", 370, 445, 90);
        make_label(hwnd, L"칼로리", 470, 445, 70);
        hFName = make_edit(hwnd, ID_F_NAME, 10, 465, 150, 0);
        hFCat  = make_edit(hwnd, ID_F_CAT, 170, 465, 90, 0);
        hFIng  = make_edit(hwnd, ID_F_ING, 270, 465, 90, 0);
        hFCook = make_edit(hwnd, ID_F_COOK, 370, 465, 90, 0);
        hFKcal = make_edit(hwnd, ID_F_KCAL, 470, 465, 70, ES_NUMBER);

        make_button(hwnd, L"음식 추가", ID_FADD, 10, 505, 110, 34);
        make_button(hwnd, L"선택 수정", ID_FEDIT, 130, 505, 110, 34);
        make_button(hwnd, L"선택 삭제", ID_FDEL, 250, 505, 110, 34);

        static const wchar_t *ratings[] = {
            L"0 (나중에 평가)", L"1", L"2", L"3", L"4", L"5" };
        for (int i = 0; i < 6; i++)
            SendMessageW(hRating, CB_ADDSTRING, 0, (LPARAM)ratings[i]);
        SendMessageW(hRating, CB_SETCURSEL, 0, 0);

        static const wchar_t *dates[] = { L"오늘", L"어제", L"2일 전" };
        for (int i = 0; i < 3; i++)
            SendMessageW(hDate, CB_ADDSTRING, 0, (LPARAM)dates[i]);
        SendMessageW(hDate, CB_SETCURSEL, 0, 0);

        refresh_list(-1);

        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
        EnumChildWindows(hwnd, set_font_proc, (LPARAM)font);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_LIST:
            if (HIWORD(wp) == LBN_SELCHANGE)
                fill_fields((int)SendMessageW(hList, LB_GETCURSEL, 0, 0));
            break;
        case ID_ADD:   on_add();       break;
        case ID_REC:   on_rec();       break;
        case ID_HIST:  on_hist();      break;
        case ID_FADD:  on_food_add();  break;
        case ID_FEDIT: on_food_edit(); break;
        case ID_FDEL:  on_food_del();  break;
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
        CW_USEDEFAULT, CW_USEDEFAULT, 750, 600, NULL, NULL, hInst, NULL);
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