#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "food.h"
#include "meal.h"
#include "recommend.h"
#include "exclude.h"

#define FOODS_PATH   "data/foods.txt"
#define HISTORY_PATH "data/history.txt"
#define EXCLUDE_PATH "data/exclude.txt"

enum {
    ID_LIST = 100, ID_RATING, ID_DATE, ID_ADD, ID_REC, ID_HIST,
    ID_F_NAME, ID_F_CAT, ID_F_ING, ID_F_COOK, ID_F_KCAL,
    ID_FADD, ID_FEDIT, ID_FDEL,
    ID_HLIST, ID_H_DATE, ID_H_RATING, ID_HEDIT, ID_HDEL,
    ID_EXBTN, ID_EXLIST
};

typedef struct {
    int kind;
    char value[MAX_ATTR];
    int count;   /* number of foods that have this value */
} ExRow;

static Food *g_foods = NULL;
static int g_nf = 0;
static Meal *g_meals = NULL;
static int g_nm = 0;
static Exclusion *g_ex = NULL;
static int g_nex = 0;
static ExRow *g_rows = NULL;
static int g_nrows = 0;
static int g_building = 0;   /* 1 while the exclusion table is being filled */

static HWND hList, hRating, hDate, hOut, hResLabel;
static HWND hFName, hFCat, hFIng, hFCook, hFKcal;
static HWND hHist, hHTitle, hHDate, hHRating;
static HWND hExcl;
static int g_sort_col = -1;
static int g_sort_asc = 1;

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

/* ---------- right-hand panel: result box, history table, exclusion table ---------- */

static int count_excluded_foods(void) {
    int c = 0;
    for (int i = 0; i < g_nf; i++)
        if (is_excluded(&g_foods[i], g_ex, g_nex)) c++;
    return c;
}

static void excl_label(void) {
    char t[128];
    snprintf(t, sizeof t, "기피 설정 (체크=제외, 현재 %d개)", count_excluded_foods());
    set_text(hResLabel, t);
}

/* 0 = result box, 1 = history table, 2 = exclusion table */
static void show_panel(int p) {
    ShowWindow(hOut, p == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(hHist, p == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(hExcl, p == 2 ? SW_SHOW : SW_HIDE);
    if (p == 0) set_text(hResLabel, "결과");
    else if (p == 1) set_text(hResLabel, "식사 기록 (최근 기록이 위)");
    else excl_label();
}

/* Shows a message in the result box. */
static void say(const char *s) {
    show_panel(0);
    set_text(hOut, s);
}

/* Short status line above the history buttons. */
static void hnote(const char *s) {
    set_text(hHTitle, s);
}

/* Yes/No question. The default button is "No". */
static int confirm(const char *msg, const wchar_t *title) {
    wchar_t *w = to_wide(msg);
    if (!w) return 0;
    int r = MessageBoxW(GetParent(hList), w, title,
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    free(w);
    return r == IDYES;
}

/* ---------- generic list view helpers ---------- */

static int lv_sel(HWND h) {
    return (int)SendMessageW(h, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
}

static void select_item(HWND h, int i) {
    LVITEMW it;
    memset(&it, 0, sizeof it);
    it.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    it.state = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(h, LVM_SETITEMSTATE, i, (LPARAM)&it);
    SendMessageW(h, LVM_ENSUREVISIBLE, i, FALSE);
}

static void set_cell(HWND h, int row, int col, const char *s) {
    wchar_t *w = to_wide(s);
    if (!w) return;
    LVITEMW it;
    memset(&it, 0, sizeof it);
    it.iSubItem = col;
    it.pszText = w;
    SendMessageW(h, LVM_SETITEMTEXTW, row, (LPARAM)&it);
    free(w);
}

static void insert_row(HWND h, int row, const char *s) {
    wchar_t *w = to_wide(s);
    if (!w) return;
    LVITEMW it;
    memset(&it, 0, sizeof it);
    it.mask = LVIF_TEXT;
    it.iItem = row;
    it.pszText = w;
    SendMessageW(h, LVM_INSERTITEMW, 0, (LPARAM)&it);
    free(w);
}

/* ---------- food table ---------- */

static void refresh_list(int select) {
    SendMessageW(hList, LVM_DELETEALLITEMS, 0, 0);
    for (int i = 0; i < g_nf; i++) {
        char k[16];
        insert_row(hList, i, g_foods[i].name);
        set_cell(hList, i, 1, g_foods[i].category);
        set_cell(hList, i, 2, g_foods[i].ingredient);
        set_cell(hList, i, 3, g_foods[i].cooking);
        snprintf(k, sizeof k, "%d", g_foods[i].kcal);
        set_cell(hList, i, 4, k);
    }
    if (select >= 0 && select < g_nf) select_item(hList, select);
}

static int cmp_food(const void *a, const void *b) {
    const Food *x = a, *y = b;
    int r;
    switch (g_sort_col) {
    case 1:  r = strcmp(x->category, y->category); break;
    case 2:  r = strcmp(x->ingredient, y->ingredient); break;
    case 3:  r = strcmp(x->cooking, y->cooking); break;
    case 4:  r = (x->kcal > y->kcal) - (x->kcal < y->kcal); break;
    default: r = strcmp(x->name, y->name); break;
    }
    if (r == 0) r = strcmp(x->name, y->name);
    return g_sort_asc ? r : -r;
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

static void on_sort(int col) {
    static const char *names[] = { "이름", "분류", "주재료", "조리법", "칼로리" };
    if (col < 0 || col > 4) return;
    if (col == g_sort_col) g_sort_asc = !g_sort_asc;
    else { g_sort_col = col; g_sort_asc = 1; }

    qsort(g_foods, g_nf, sizeof(Food), cmp_food);
    refresh_list(-1);
    clear_fields();

    char msg[256];
    snprintf(msg, sizeof msg,
             "'%s' 기준 %s 정렬했습니다.\r\n같은 제목을 다시 클릭하면 순서가 바뀝니다.",
             names[col], g_sort_asc ? "오름차순으로" : "내림차순으로");
    say(msg);
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
        say("모든 칸을 입력하세요.\r\n"
            "쉼표(,)는 쓸 수 없고, 이름은 20자, 나머지 칸은 10자 정도까지 입력할 수 있습니다.");
        return 0;
    }
    if (!get_field(hFKcal, k, sizeof k) || atoi(k) <= 0) {
        say("칼로리는 1 이상의 숫자로 입력하세요.");
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

/* ---------- meal history ---------- */

static void reload_history(void) {
    free(g_meals);
    g_meals = NULL;
    g_nm = load_meals(HISTORY_PATH, &g_meals);
    if (g_nm < 0) { g_nm = 0; free(g_meals); g_meals = NULL; }
}

static int count_meals(const char *name) {
    int c = 0;
    for (int i = 0; i < g_nm; i++)
        if (strcmp(g_meals[i].food_name, name) == 0) c++;
    return c;
}

static int valid_date(const char *s) {
    if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') return 0;
    for (int i = 0; i < 10; i++)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return 0;
    int mo = atoi(s + 5), d = atoi(s + 8);
    return mo >= 1 && mo <= 12 && d >= 1 && d <= 31;
}

/* The table shows the newest record first: row r is meal g_nm-1-r. */
static void refresh_hist(int sel_idx) {
    SendMessageW(hHist, LVM_DELETEALLITEMS, 0, 0);
    for (int row = 0; row < g_nm; row++) {
        const Meal *m = &g_meals[g_nm - 1 - row];
        char r[16];
        insert_row(hHist, row, m->date);
        set_cell(hHist, row, 1, m->food_name);
        if (m->rating == 0) strcpy(r, "미평가");
        else snprintf(r, sizeof r, "%d", m->rating);
        set_cell(hHist, row, 2, r);
    }
    if (sel_idx >= 0 && sel_idx < g_nm) select_item(hHist, g_nm - 1 - sel_idx);
}

static void fill_hist_fields(int row) {
    int idx = g_nm - 1 - row;
    if (idx < 0 || idx >= g_nm) return;
    set_text(hHDate, g_meals[idx].date);
    SendMessageW(hHRating, CB_SETCURSEL, g_meals[idx].rating, 0);
}

static void on_hist(void) {
    reload_history();
    if (g_nm == 0) { say("기록이 없습니다."); return; }
    refresh_hist(-1);
    set_text(hHDate, "");
    SendMessageW(hHRating, CB_SETCURSEL, 0, 0);
    show_panel(1);
    hnote("기록 관리 (기록을 클릭하세요)");
}

static void on_hist_edit(void) {
    int row = lv_sel(hHist);
    if (!IsWindowVisible(hHist) || row < 0) {
        hnote("먼저 기록을 클릭하세요");
        return;
    }
    int idx = g_nm - 1 - row;
    char d[32];
    if (!get_field(hHDate, d, sizeof d) || !valid_date(d)) {
        hnote("날짜는 2026-11-12 형식");
        return;
    }
    int rating = (int)SendMessageW(hHRating, CB_GETCURSEL, 0, 0);
    if (rating == CB_ERR) rating = 0;

    strcpy(g_meals[idx].date, d);
    g_meals[idx].rating = rating;
    if (save_meals(HISTORY_PATH, g_meals, g_nm) == 0) hnote("기록을 수정했습니다");
    else hnote("저장에 실패했습니다");
    refresh_hist(idx);
}

static void on_hist_del(void) {
    int row = lv_sel(hHist);
    if (!IsWindowVisible(hHist) || row < 0) {
        hnote("먼저 기록을 클릭하세요");
        return;
    }
    int idx = g_nm - 1 - row;

    char msg[320];
    snprintf(msg, sizeof msg,
             "선택한 기록을 정말 삭제할까요?\n\n%s | %s | 별점 %d\n\n삭제하면 되돌릴 수 없습니다.",
             g_meals[idx].date, g_meals[idx].food_name, g_meals[idx].rating);
    if (!confirm(msg, L"기록 삭제 확인")) return;

    memmove(&g_meals[idx], &g_meals[idx + 1], (g_nm - idx - 1) * sizeof(Meal));
    g_nm--;
    if (save_meals(HISTORY_PATH, g_meals, g_nm) == 0) hnote("기록을 삭제했습니다");
    else hnote("저장에 실패했습니다");
    refresh_hist(-1);
    set_text(hHDate, "");
    SendMessageW(hHRating, CB_SETCURSEL, 0, 0);
}

/* ---------- excluded attributes ---------- */

static int cmp_row(const void *a, const void *b) {
    const ExRow *x = a, *y = b;
    if (x->kind != y->kind) return x->kind - y->kind;
    return strcmp(x->value, y->value);
}

/* Collects every (kind, value) that appears in the food list. */
static void build_rows(void) {
    free(g_rows);
    g_rows = NULL;
    g_nrows = 0;
    int cap = 0;

    for (int i = 0; i < g_nf; i++) {
        for (int k = 0; k < 3; k++) {
            const char *v = food_attr(&g_foods[i], k);
            int found = -1;
            for (int r = 0; r < g_nrows; r++)
                if (g_rows[r].kind == k && strcmp(g_rows[r].value, v) == 0) {
                    found = r;
                    break;
                }
            if (found >= 0) { g_rows[found].count++; continue; }

            if (g_nrows == cap) {
                cap = cap ? cap * 2 : 32;
                ExRow *tmp = realloc(g_rows, cap * sizeof(ExRow));
                if (!tmp) return;
                g_rows = tmp;
            }
            g_rows[g_nrows].kind = k;
            snprintf(g_rows[g_nrows].value, MAX_ATTR, "%s", v);
            g_rows[g_nrows].count = 1;
            g_nrows++;
        }
    }
    qsort(g_rows, g_nrows, sizeof(ExRow), cmp_row);
}

static void refresh_excl(void) {
    static const char *kinds[] = { "분류", "재료", "조리법" };
    build_rows();

    g_building = 1;   /* ignore the check-box events caused by filling */
    SendMessageW(hExcl, LVM_DELETEALLITEMS, 0, 0);
    for (int r = 0; r < g_nrows; r++) {
        char c[16];
        insert_row(hExcl, r, kinds[g_rows[r].kind]);
        set_cell(hExcl, r, 1, g_rows[r].value);
        snprintf(c, sizeof c, "%d개", g_rows[r].count);
        set_cell(hExcl, r, 2, c);

        if (find_exclusion(g_ex, g_nex, g_rows[r].kind, g_rows[r].value) >= 0) {
            LVITEMW it;
            memset(&it, 0, sizeof it);
            it.stateMask = LVIS_STATEIMAGEMASK;
            it.state = INDEXTOSTATEIMAGEMASK(2);   /* checked */
            SendMessageW(hExcl, LVM_SETITEMSTATE, r, (LPARAM)&it);
        }
    }
    g_building = 0;
}

static void on_excl_toggle(int row, int checked) {
    if (row < 0 || row >= g_nrows) return;
    if (set_exclusion(&g_ex, &g_nex, g_rows[row].kind, g_rows[row].value, checked) != 0)
        return;
    save_exclusions(EXCLUDE_PATH, g_ex, g_nex);
    excl_label();
}

static void on_excl(void) {
    refresh_excl();
    show_panel(2);
}

/* ---------- food management ---------- */

static void on_food_add(void) {
    Food f;
    if (!read_food_fields(&f)) return;
    if (find_by_name(f.name, -1) >= 0) {
        say("같은 이름의 음식이 이미 있습니다.");
        return;
    }
    Food *tmp = realloc(g_foods, (g_nf + 1) * sizeof(Food));
    if (!tmp) { say("메모리가 부족합니다."); return; }
    g_foods = tmp;
    g_foods[g_nf++] = f;

    refresh_list(g_nf - 1);
    char msg[256];
    if (save_foods(FOODS_PATH, g_foods, g_nf) == 0)
        snprintf(msg, sizeof msg, "추가했습니다: %s (총 %d개)", f.name, g_nf);
    else
        snprintf(msg, sizeof msg, "추가했지만 파일 저장에 실패했습니다.");
    say(msg);
}

static void on_food_edit(void) {
    int sel = lv_sel(hList);
    if (sel < 0) {
        say("수정할 음식을 목록에서 먼저 클릭하세요.");
        return;
    }
    Food f;
    if (!read_food_fields(&f)) return;
    if (find_by_name(f.name, sel) >= 0) {
        say("같은 이름의 음식이 이미 있습니다.");
        return;
    }

    char old[MAX_NAME];
    strcpy(old, g_foods[sel].name);
    int renamed = (strcmp(old, f.name) != 0);
    g_foods[sel] = f;
    refresh_list(sel);

    /* keep the meal history in sync with the new name */
    int moved = 0;
    if (renamed) {
        for (int i = 0; i < g_nm; i++)
            if (strcmp(g_meals[i].food_name, old) == 0) {
                strcpy(g_meals[i].food_name, f.name);
                moved++;
            }
        if (moved > 0) save_meals(HISTORY_PATH, g_meals, g_nm);
    }

    char msg[320];
    if (save_foods(FOODS_PATH, g_foods, g_nf) == 0) {
        snprintf(msg, sizeof msg, "수정했습니다: %s", f.name);
        if (moved > 0) {
            char t[96];
            snprintf(t, sizeof t, "\r\n식사 기록 %d건의 음식 이름도 함께 바꿨습니다.", moved);
            strcat(msg, t);
        }
    } else {
        snprintf(msg, sizeof msg, "수정했지만 파일 저장에 실패했습니다.");
    }
    say(msg);
}

static void on_food_del(void) {
    int sel = lv_sel(hList);
    if (sel < 0) {
        say("삭제할 음식을 목록에서 먼저 클릭하세요.");
        return;
    }

    char name[MAX_NAME];
    strcpy(name, g_foods[sel].name);
    int cnt = count_meals(name);

    char q[400];
    if (cnt > 0)
        snprintf(q, sizeof q,
                 "'%s'을(를) 정말 삭제할까요?\n\n이 음식은 식사 기록 %d건에 사용되었습니다.\n"
                 "삭제하면 해당 기록도 함께 삭제되고, 되돌릴 수 없습니다.", name, cnt);
    else
        snprintf(q, sizeof q,
                 "'%s'을(를) 정말 삭제할까요?\n\n삭제하면 되돌릴 수 없습니다.", name);
    if (!confirm(q, L"음식 삭제 확인")) return;

    memmove(&g_foods[sel], &g_foods[sel + 1], (g_nf - sel - 1) * sizeof(Food));
    g_nf--;

    if (cnt > 0) {
        int w = 0;
        for (int i = 0; i < g_nm; i++)
            if (strcmp(g_meals[i].food_name, name) != 0) g_meals[w++] = g_meals[i];
        g_nm = w;
        save_meals(HISTORY_PATH, g_meals, g_nm);
    }

    refresh_list(-1);
    clear_fields();
    char msg[320];
    if (save_foods(FOODS_PATH, g_foods, g_nf) == 0) {
        snprintf(msg, sizeof msg, "삭제했습니다: %s (남은 음식 %d개)", name, g_nf);
        if (cnt > 0) {
            char t[96];
            snprintf(t, sizeof t, "\r\n식사 기록 %d건도 함께 삭제했습니다.", cnt);
            strcat(msg, t);
        }
    } else {
        snprintf(msg, sizeof msg, "삭제했지만 파일 저장에 실패했습니다.");
    }
    say(msg);
}

/* ---------- add meal and recommendation ---------- */

static void on_add(void) {
    int sel = lv_sel(hList);
    if (sel < 0) {
        say("먼저 왼쪽 목록에서 먹은 음식을 클릭하세요.");
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
    if (append_meal(HISTORY_PATH, &m) == 0) {
        reload_history();
        snprintf(msg, sizeof msg, "기록했습니다: %s | %s | 별점 %d",
                 m.date, m.food_name, m.rating);
    } else {
        snprintf(msg, sizeof msg, "저장에 실패했습니다.");
    }
    say(msg);
}

static void on_rec(void) {
    Score top[3];
    int k = recommend_filtered(g_foods, g_nf, g_meals, g_nm, g_ex, g_nex, top, 3);
    if (k < 0) { say("추천 계산에 실패했습니다."); return; }
    if (k == 0) { say("추천할 음식이 없습니다.\r\n기피 설정을 확인하세요."); return; }

    int excluded = count_excluded_foods();
    char buf[2048];
    int pos = snprintf(buf, sizeof buf, "기록 %d건 기준 오늘의 추천\r\n", g_nm);
    if (excluded > 0)
        pos += snprintf(buf + pos, sizeof buf - pos,
                        "(기피 설정으로 음식 %d개 제외)\r\n", excluded);
    pos += snprintf(buf + pos, sizeof buf - pos, "\r\n");

    for (int i = 0; i < k; i++) {
        const Food *f = &g_foods[top[i].food_index];
        pos += snprintf(buf + pos, sizeof buf - pos,
            "%d위: %s (%s, %s, %s)  총점 %.2f\r\n"
            "     취향 %+.2f | 어제 %+.2f | 최근 %+.2f | 탐험 %+.2f\r\n\r\n",
            i + 1, f->name, f->category, f->ingredient, f->cooking, top[i].total,
            top[i].taste, top[i].yesterday, top[i].recent, top[i].explore);
    }
    say(buf);
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

static HWND make_table(HWND p, int id, int x, int y, int w, int h, DWORD visible) {
    HWND t = CreateWindowExW(0, L"SysListView32", NULL,
        WS_CHILD | visible | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        x, y, w, h, p, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(t, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                 LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    return t;
}

static void add_columns(HWND t, const wchar_t **names, const int *widths, int n) {
    for (int i = 0; i < n; i++) {
        LVCOLUMNW c;
        memset(&c, 0, sizeof c);
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = (LPWSTR)names[i];
        c.cx = widths[i];
        SendMessageW(t, LVM_INSERTCOLUMNW, i, (LPARAM)&c);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_nf = load_foods(FOODS_PATH, &g_foods);
        if (g_nf < 0) {
            MessageBoxW(hwnd, L"data/foods.txt를 불러오지 못했습니다.\n"
                              L"exe 파일이 data 폴더와 같은 위치에 있는지 확인하세요.",
                        L"오류", MB_OK | MB_ICONERROR);
            return -1;
        }

        /* food table */
        make_label(hwnd, L"음식 목록 (클릭해서 선택, 열 제목을 클릭하면 정렬)", 10, 8, 450);
        hList = make_table(hwnd, ID_LIST, 10, 30, 450, 380, WS_VISIBLE);
        static const wchar_t *fcols[] = { L"이름", L"분류", L"주재료", L"조리법", L"칼로리" };
        static const int fwidths[] = { 130, 60, 85, 70, 65 };
        add_columns(hList, fcols, fwidths, 5);

        /* meal controls */
        make_label(hwnd, L"별점", 480, 8, 100);
        hRating = CreateWindowExW(0, L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
            480, 30, 150, 200, hwnd, (HMENU)(INT_PTR)ID_RATING, NULL, NULL);
        make_label(hwnd, L"날짜", 650, 8, 100);
        hDate = CreateWindowExW(0, L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
            650, 30, 150, 200, hwnd, (HMENU)(INT_PTR)ID_DATE, NULL, NULL);

        make_button(hwnd, L"식사 기록 추가", ID_ADD, 480, 75, 120, 34);
        make_button(hwnd, L"오늘의 추천 보기", ID_REC, 610, 75, 130, 34);
        make_button(hwnd, L"기록 보기", ID_HIST, 750, 75, 110, 34);

        /* result box, history table and exclusion table share one area */
        hResLabel = make_label(hwnd, L"결과", 480, 125, 390);
        hOut = CreateWindowExW(0, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            480, 147, 380, 263, hwnd, NULL, NULL, NULL);

        hHist = make_table(hwnd, ID_HLIST, 480, 147, 380, 263, 0);
        static const wchar_t *hcols[] = { L"날짜", L"음식", L"별점" };
        static const int hwidths[] = { 100, 180, 70 };
        add_columns(hHist, hcols, hwidths, 3);

        hExcl = make_table(hwnd, ID_EXLIST, 480, 147, 380, 263, 0);
        SendMessageW(hExcl, LVM_SETEXTENDEDLISTVIEWSTYLE,
                     LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
        static const wchar_t *xcols[] = { L"구분", L"이름", L"음식 수" };
        static const int xwidths[] = { 110, 140, 80 };
        add_columns(hExcl, xcols, xwidths, 3);

        /* food management */
        make_label(hwnd, L"음식 관리 (목록에서 음식을 클릭하면 아래 칸에 채워집니다)",
                   10, 420, 450);
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
        make_button(hwnd, L"기피 설정", ID_EXBTN, 380, 505, 110, 34);

        /* history management */
        hHTitle = make_label(hwnd, L"기록 관리 (기록 보기에서 클릭)", 580, 420, 290);
        make_label(hwnd, L"날짜", 580, 445, 100);
        hHDate = make_edit(hwnd, ID_H_DATE, 580, 465, 110, 0);
        make_label(hwnd, L"별점", 710, 445, 60);
        hHRating = CreateWindowExW(0, L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
            710, 465, 80, 200, hwnd, (HMENU)(INT_PTR)ID_H_RATING, NULL, NULL);
        make_button(hwnd, L"기록 수정", ID_HEDIT, 580, 505, 100, 34);
        make_button(hwnd, L"기록 삭제", ID_HDEL, 690, 505, 100, 34);

        static const wchar_t *ratings[] = {
            L"0 (나중에 평가)", L"1", L"2", L"3", L"4", L"5" };
        for (int i = 0; i < 6; i++)
            SendMessageW(hRating, CB_ADDSTRING, 0, (LPARAM)ratings[i]);
        SendMessageW(hRating, CB_SETCURSEL, 0, 0);

        static const wchar_t *hratings[] = { L"0", L"1", L"2", L"3", L"4", L"5" };
        for (int i = 0; i < 6; i++)
            SendMessageW(hHRating, CB_ADDSTRING, 0, (LPARAM)hratings[i]);
        SendMessageW(hHRating, CB_SETCURSEL, 0, 0);

        static const wchar_t *dates[] = { L"오늘", L"어제", L"2일 전" };
        for (int i = 0; i < 3; i++)
            SendMessageW(hDate, CB_ADDSTRING, 0, (LPARAM)dates[i]);
        SendMessageW(hDate, CB_SETCURSEL, 0, 0);

        refresh_list(-1);
        reload_history();
        g_nex = load_exclusions(EXCLUDE_PATH, &g_ex);
        if (g_nex < 0) g_nex = 0;

        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
        EnumChildWindows(hwnd, set_font_proc, (LPARAM)font);
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR *nh = (NMHDR *)lp;
        NMLISTVIEW *nl = (NMLISTVIEW *)lp;
        if (nh->hwndFrom == hList) {
            if (nh->code == LVN_ITEMCHANGED) {
                if ((nl->uChanged & LVIF_STATE) && (nl->uNewState & LVIS_SELECTED))
                    fill_fields(nl->iItem);
            } else if (nh->code == LVN_COLUMNCLICK) {
                on_sort(nl->iSubItem);
            }
        } else if (nh->hwndFrom == hHist) {
            if (nh->code == LVN_ITEMCHANGED &&
                (nl->uChanged & LVIF_STATE) && (nl->uNewState & LVIS_SELECTED))
                fill_hist_fields(nl->iItem);
        } else if (nh->hwndFrom == hExcl) {
            if (nh->code == LVN_ITEMCHANGED && !g_building &&
                (nl->uChanged & LVIF_STATE) &&
                ((nl->uNewState ^ nl->uOldState) & LVIS_STATEIMAGEMASK))
                on_excl_toggle(nl->iItem,
                               ((nl->uNewState & LVIS_STATEIMAGEMASK) >> 12) == 2);
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ADD:   on_add();       break;
        case ID_REC:   on_rec();       break;
        case ID_HIST:  on_hist();      break;
        case ID_FADD:  on_food_add();  break;
        case ID_FEDIT: on_food_edit(); break;
        case ID_FDEL:  on_food_del();  break;
        case ID_HEDIT: on_hist_edit(); break;
        case ID_HDEL:  on_hist_del();  break;
        case ID_EXBTN: on_excl();      break;
        }
        return 0;
    case WM_DESTROY:
        free(g_foods);
        free(g_meals);
        free(g_ex);
        free(g_rows);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    (void)hPrev; (void)cmd;

    /* Move to the folder that contains the exe, so data/ is found
       no matter how the program is launched (double-click, shortcut). */
    wchar_t exe[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe, MAX_PATH) > 0) {
        wchar_t *slash = wcsrchr(exe, L'\\');
        if (slash) {
            *slash = L'\0';
            SetCurrentDirectoryW(exe);
        }
    }

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof icc;
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

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
        CW_USEDEFAULT, CW_USEDEFAULT, 890, 600, NULL, NULL, hInst, NULL);
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