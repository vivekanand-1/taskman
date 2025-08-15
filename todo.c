#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TITLE_MAX 128
#define FILE_PATH "tasks.csv"

typedef struct {
    int id;
    char title[TITLE_MAX];
    int done; // 0 = not done, 1 = done
} Task;

typedef struct {
    Task *data;
    size_t size;
    size_t cap;
} TaskList;

static void die(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

static void init_list(TaskList *list) {
    list->size = 0;
    list->cap = 8;
    list->data = (Task *)malloc(list->cap * sizeof(Task));
    if (!list->data) die("Out of memory");
}

static void free_list(TaskList *list) {
    free(list->data);
    list->data = NULL;
    list->size = list->cap = 0;
}

static void ensure_cap(TaskList *list, size_t need) {
    if (need <= list->cap) return;
    while (list->cap < need) list->cap *= 2;
    Task *nd = (Task *)realloc(list->data, list->cap * sizeof(Task));
    if (!nd) die("Out of memory");
    list->data = nd;
}

static void rstrip_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[--n] = '\0';
    }
}

static void sanitize_title(char *s) {
    // Replace commas with semicolons so CSV stays simple
    for (; *s; ++s) if (*s == ',') *s = ';';
}

static int parse_int(const char *s) {
    // Simple safe atoi
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return 0;
    if (v < -2147483648L) v = -2147483648L;
    if (v > 2147483647L) v = 2147483647L;
    return (int)v;
}

static int next_id(const TaskList *list) {
    int mx = 0;
    for (size_t i = 0; i < list->size; ++i) if (list->data[i].id > mx) mx = list->data[i].id;
    return mx + 1;
}

static void load_tasks(TaskList *list, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return; // okay if file doesn't exist yet
    char line[256 + TITLE_MAX];
    while (fgets(line, sizeof(line), f)) {
        rstrip_newline(line);
        if (line[0] == '\0') continue;
        // CSV: id,title,done
        char *id = strtok(line, ",");
        char *title = strtok(NULL, ",");
        char *done = strtok(NULL, ",");
        if (!id || !title || !done) continue;
        ensure_cap(list, list->size + 1);
        Task *t = &list->data[list->size++];
        t->id = parse_int(id);
        strncpy(t->title, title, TITLE_MAX - 1);
        t->title[TITLE_MAX - 1] = '\0';
        t->done = parse_int(done) ? 1 : 0;
    }
    fclose(f);
}

static void save_tasks(const TaskList *list, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) die("Cannot open tasks file for writing");
    for (size_t i = 0; i < list->size; ++i) {
        fprintf(f, "%d,%s,%d\n", list->data[i].id, list->data[i].title, list->data[i].done);
    }
    fclose(f);
}

static void list_tasks(const TaskList *list) {
    if (list->size == 0) {
        printf("No tasks yet. Add one!\n");
        return;
    }
    printf("\nID   Status  Title\n");
    printf("----------------------------------------\n");
    for (size_t i = 0; i < list->size; ++i) {
        const Task *t = &list->data[i];
        printf("%-4d %-7s %s\n", t->id, t->done ? "[x]" : "[ ]", t->title);
    }
}

static void add_task(TaskList *list) {
    char buf[TITLE_MAX];
    printf("Enter task title: ");
    if (!fgets(buf, sizeof(buf), stdin)) return;
    rstrip_newline(buf);
    if (buf[0] == '\0') {
        printf("Title cannot be empty.\n");
        return;
    }
    sanitize_title(buf);
    ensure_cap(list, list->size + 1);
    Task *t = &list->data[list->size++];
    t->id = next_id(list);
    strncpy(t->title, buf, TITLE_MAX - 1);
    t->title[TITLE_MAX - 1] = '\0';
    t->done = 0;
    printf("Added task #%d.\n", t->id);
}

static Task *find_by_id(TaskList *list, int id) {
    for (size_t i = 0; i < list->size; ++i) if (list->data[i].id == id) return &list->data[i];
    return NULL;
}

static void mark_done(TaskList *list) {
    char buf[32];
    printf("Enter task ID to mark done: ");
    if (!fgets(buf, sizeof(buf), stdin)) return;
    int id = parse_int(buf);
    Task *t = find_by_id(list, id);
    if (!t) {
        printf("No task with ID %d.\n", id);
        return;
    }
    t->done = 1;
    printf("Marked task #%d as done.\n", id);
}

static void delete_task(TaskList *list) {
    char buf[32];
    printf("Enter task ID to delete: ");
    if (!fgets(buf, sizeof(buf), stdin)) return;
    int id = parse_int(buf);
    size_t idx = list->size;
    for (size_t i = 0; i < list->size; ++i) if (list->data[i].id == id) { idx = i; break; }
    if (idx == list->size) {
        printf("No task with ID %d.\n", id);
        return;
    }
    for (size_t j = idx + 1; j < list->size; ++j) list->data[j-1] = list->data[j];
    list->size--;
    printf("Deleted task #%d.\n", id);
}

static void search_tasks(const TaskList *list) {
    char q[TITLE_MAX];
    printf("Enter search text: ");
    if (!fgets(q, sizeof(q), stdin)) return;
    rstrip_newline(q);
    if (q[0] == '\0') {
        printf("Search text cannot be empty.\n");
        return;
    }
    printf("\nResults for '%s':\n", q);
    int found = 0;
    for (size_t i = 0; i < list->size; ++i) {
        const Task *t = &list->data[i];
        if (strstr(t->title, q)) {
            printf("%-4d %-7s %s\n", t->id, t->done ? "[x]" : "[ ]", t->title);
            found = 1;
        }
    }
    if (!found) printf("No matching tasks.\n");
}

static void stats(const TaskList *list) {
    size_t done = 0;
    for (size_t i = 0; i < list->size; ++i) if (list->data[i].done) done++;
    printf("Total: %zu, Done: %zu, Pending: %zu\n", list->size, done, list->size - done);
}

static void press_enter_to_continue(void) {
    printf("\nPress ENTER to continue...");
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* consume */ }
}

static void menu(void) {
    printf("\n==== To-Do CLI ====/n");
    printf("1) List tasks\n");
    printf("2) Add task\n");
    printf("3) Mark task as done\n");
    printf("4) Delete task\n");
    printf("5) Search tasks\n");
    printf("6) Stats\n");
    printf("7) Save & Exit\n");
    printf("Choose: ");
}

int main(void) {
    TaskList list; 
    init_list(&list);
    load_tasks(&list, FILE_PATH);

    char choice[8];
    for (;;) {
        menu();
        if (!fgets(choice, sizeof(choice), stdin)) break;
        switch (choice[0]) {
            case '1':
                list_tasks(&list);
                press_enter_to_continue();
                break;
            case '2':
                add_task(&list);
                save_tasks(&list, FILE_PATH);
                press_enter_to_continue();
                break;
            case '3':
                mark_done(&list);
                save_tasks(&list, FILE_PATH);
                press_enter_to_continue();
                break;
            case '4':
                delete_task(&list);
                save_tasks(&list, FILE_PATH);
                press_enter_to_continue();
                break;
            case '5':
                search_tasks(&list);
                press_enter_to_continue();
                break;
            case '6':
                stats(&list);
                press_enter_to_continue();
                break;
            case '7':
                save_tasks(&list, FILE_PATH);
                printf("Saved to %s. Goodbye!\n", FILE_PATH);
                free_list(&list);
                return 0;
            default:
                printf("Invalid choice. Try again.\n");
                break;
        }
    }

    save_tasks(&list, FILE_PATH);
    free_list(&list);
    return 0;
}
