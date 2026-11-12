# Food Recommender

A program written in C that recommends what to eat today, based on what you ate yesterday, your recent meal history, and your own star ratings. It comes with two front ends that share the same core code: a console version and a clickable Win32 GUI version.

## Download
A ready-to-run Windows build is available on the [Releases](https://github.com/minsung3128/food-recommender/releases) page. Extract the zip, keep `foodrec_gui.exe` next to the `data` folder, and double-click the exe.

Windows may show a SmartScreen warning because the program is not code-signed. Click "More info" and then "Run anyway", or build from source (see below).

## Features
- Loads a food database (65 dishes) from a CSV file into a dynamic array (`malloc`/`realloc`)
- Records meals with a date and a star rating (0-5) in a history file
- Recommends the top 3 foods with a score breakdown
- Learns taste from ratings: a rating affects not only the rated food but also foods that share its category, main ingredient, or cooking method
- Avoids repetition: penalizes foods similar to yesterday's meal and foods eaten in the last few days
- Adds, edits, and deletes foods from the GUI; changes are saved to `data/foods.txt`
- Console menu and Win32 GUI (buttons, list box, drop-down boxes)
- The GUI starts from the exe's own folder, so a double click or a shortcut works

## 1. Motivation
TBD

## 2. Related Work
TBD

## 3. Design

### 3.1 Data
Each food has a name, a category, a main ingredient, a cooking method, and calories (approximate values). Each meal record has a date, a food name, and a rating.

`data/foods.txt` has one food per line, in UTF-8, in the order `name,category,ingredient,cooking,kcal`:
```
김치찌개,한식,돼지고기,국물,450
된장찌개,한식,채소,국물,350
```

`data/history.txt` has one meal per line, in UTF-8, in the order `date,food_name,rating`. A rating of 0 means "not rated":
```
2026-11-10,김치찌개,5
2026-11-09,짜장면,3
```

Meals refer to foods by name instead of by index, so the history stays valid when the food list is reordered.

Attribute words are kept consistent, because the recommender compares them as exact strings:

| Field | Values |
|---|---|
| Category | 한식, 중식, 일식, 양식, 분식, 동남아 |
| Ingredient | 돼지고기, 소고기, 닭고기, 해산물, 채소, 면, 밥, 떡, 치즈, 계란, 두부 |
| Cooking | 국물, 볶음, 구이, 튀김, 찜, 조림, 생식 |

### 3.2 Scoring
For every food, the program computes the score below and recommends the highest ones. A food's attributes are its category, main ingredient, and cooking method. For each past meal, `attr` is the number of attributes (0-3) it shares with the candidate food.

| Component | Rule |
|---|---|
| Taste (learned) | `0.1 x (rating - 3) x attr`, plus `0.3 x (rating - 3)` if it is the same food |
| Yesterday penalty | `-0.5 x attr` for yesterday's meal, and `-2.0` more if it is the same food |
| Recent penalty | `-1.0` if the same food was eaten 2-3 days ago |
| Exploration bonus | `+0.5` if the food has never been eaten |

`total = taste + yesterday + recent + explore`

A rating of 5 raises the weight of a food's attributes, a rating of 1 lowers it, and 3 is neutral. All constants are defined with `#define` at the top of `src/recommend.c`, so they can be tuned for experiments.

## 4. Implementation

```
food-recommender/
├─ data/
│  ├─ foods.txt
│  └─ history.txt
├─ include/
│  ├─ food.h          Food struct, load_foods(), save_foods()
│  ├─ meal.h          Meal struct, load_meals(), append_meal()
│  └─ recommend.h     Score struct, recommend()
├─ src/
│  ├─ food.c          reads and writes foods.txt (dynamic array)
│  ├─ meal.c          reads and appends history.txt
│  ├─ recommend.c     scoring and sorting (qsort)
│  ├─ main.c          console front end
│  └─ gui.c           Win32 GUI front end
└─ README.md
```

The core modules (`food.c`, `meal.c`, `recommend.c`) do not depend on any user interface. `main.c` and `gui.c` are two interchangeable front ends, so only one of them is compiled into an executable at a time.

## 5. Results
TBD

## 6. Analysis and Discussion
TBD

### Known Limitations
- History records refer to foods by name, so renaming or deleting a food leaves its old records unmatched; the recommender ignores them.
- Long rows in the GUI food list are cut off on the right side.

### Planned Work
- Simulation test mode with a virtual user, to analyze how recommendations change over 30 days
- Comparison experiments with different scoring constants
- Remaining report sections

## 7. References
TBD

## Acknowledgements and AI Assistance
TBD (state clearly which parts were written with AI assistance, which parts were modified or written by the author, and any external code or sources used)

## Development Environment
- Windows, VS Code
- gcc 16.2.0 (MSYS2 UCRT64)

## Build and Run
Run all commands from the project root folder, because the data files are opened with relative paths (the GUI switches to the exe's folder by itself).

GUI version:
```
gcc -Iinclude src/gui.c src/food.c src/meal.c src/recommend.c -o foodrec_gui.exe -mwindows -static
.\foodrec_gui.exe
```

Console version:
```
gcc -Iinclude src/main.c src/food.c src/meal.c src/recommend.c -o main.exe
.\main.exe
```

## How to Use (GUI)
1. Click a food in the list on the left.
2. Choose a rating and a date (today, yesterday, or 2 days ago) from the drop-down boxes.
3. Click "식사 기록 추가" to save the meal.
4. Click "오늘의 추천 보기" to see the top 3 recommendations with score details.
5. Click "기록 보기" to see all saved meals.
6. To manage foods, click a food, edit the boxes under "음식 관리", and click "선택 수정"; or fill the boxes and click "음식 추가"; or click "선택 삭제".