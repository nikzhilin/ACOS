// Сценарные тесты: запускаем bin/carwash на файлах из data/ и проверяем
// код выхода и вывод. Запуск из корня проекта:
//   cmake --build build --target check
// или вручную: ./bin/run_tests ./bin/carwash

#include <dirent.h>
#include <fcntl.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_ARGS 32

static const char *bin = "./bin/carwash";
static char tmpDir[64];
static int passed = 0, failed = 0;
static int color = 0;

static void Report(int ok, const char *name, const char *what) {
    const char *mark = ok ? "PASS" : "FAIL";
    if (color) {
        printf("  %s%s\033[0m %s: %s\n", ok ? "\033[32m" : "\033[31m", mark, name, what);
    } else {
        printf("  %s %s: %s\n", mark, name, what);
    }
    ok ? ++passed : ++failed;
}

static void TmpPath(char *buf, size_t size, const char *name, const char *ext) {
    snprintf(buf, size, "%s/%s.%s", tmpDir, name, ext);
}

// Читает файл целиком; вызывающий освобождает память
static char *ReadFile(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }
    size_t size = 0, cap = 4096;
    char *buf = malloc(cap);
    ssize_t got;
    while (buf && (got = read(fd, buf + size, cap - size - 1)) > 0) {
        size += (size_t)got;
        if (cap - size < 2) {
            cap *= 2;
            char *bigger = realloc(buf, cap);
            if (bigger == NULL) {
                free(buf);
            }
            buf = bigger;  // при ошибке NULL, и цикл закончится
        }
    }
    close(fd);
    if (buf) {
        buf[size] = '\0';
    }
    return buf;
}

// Запускает carwash -m log -l <tmp>/<name>.log <args...>, вывод идёт в <tmp>/<name>.out.
// Возвращает pid; дождаться завершения - Wait()
static pid_t Start(const char *name, const char *const args[]) {
    char out[256], log[256];
    TmpPath(out, sizeof(out), name, "out");
    TmpPath(log, sizeof(log), name, "log");

    const char *argv[MAX_ARGS] = {bin, "-m", "log", "-l", log};
    int n = 5;
    for (int i = 0; args[i] != NULL && n < MAX_ARGS - 1; ++i) {
        argv[n++] = args[i];
    }
    argv[n] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);
        execv(bin, (char *const *)argv);
        perror("execv");
        _exit(127);
    }
    return pid;
}

static int Wait(pid_t pid) {
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Запуск с проверкой кода выхода
static void Run(const char *name, int expected, const char *const args[]) {
    int code = Wait(Start(name, args));
    char what[64];
    snprintf(what, sizeof(what), "exit code %d (expected %d)", code, expected);
    Report(code == expected, name, what);
}

static int Matches(const char *name, const char *ext, const char *pattern) {
    char path[256];
    TmpPath(path, sizeof(path), name, ext);
    char *text = ReadFile(path);
    regex_t re;
    int found = 0;
    if (text && regcomp(&re, pattern, REG_EXTENDED | REG_NEWLINE | REG_NOSUB) == 0) {
        found = regexec(&re, text, 0, NULL, 0) == 0;
        regfree(&re);
    }
    free(text);
    return found;
}

static void Expect(const char *name, const char *pattern, const char *what) {
    Report(Matches(name, "out", pattern), name, what);
}

static void NotExpect(const char *name, const char *pattern, const char *what) {
    Report(!Matches(name, "out", pattern), name, what);
}

// Номер первой строки с подстрокой или -1
static long LineOf(const char *name, const char *needle) {
    char path[256];
    TmpPath(path, sizeof(path), name, "out");
    char *text = ReadFile(path);
    char *at = text ? strstr(text, needle) : NULL;
    long line = -1;
    if (at) {
        line = 1;
        for (char *p = text; p < at; ++p) {
            line += *p == '\n';
        }
    }
    free(text);
    return line;
}

static int SameFiles(const char *a, const char *b) {
    char *x = ReadFile(a);
    char *y = ReadFile(b);
    int same = x && y && strcmp(x, y) == 0;
    free(x);
    free(y);
    return same;
}

// Копия конфига, где строка "failure = ..." заменена на свою
static void WriteWithFailure(const char *src, const char *dst, const char *failure) {
    char *text = ReadFile(src);
    FILE *f = fopen(dst, "w");
    if (text == NULL || f == NULL) {
        free(text);
        if (f) {
            fclose(f);
        }
        return;
    }
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        fprintf(f, "%s\n", strncmp(line, "failure", 7) == 0 ? failure : line);
    }
    fclose(f);
    free(text);
}

static void RemoveTmpDir(void) {
    DIR *dir = opendir(tmpDir);
    if (dir == NULL) {
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", tmpDir, entry->d_name);
            unlink(path);
        }
    }
    closedir(dir);
    rmdir(tmpDir);
}

//------------------------------------------------------------------------------

static void TestNormalDay(void) {
    puts("== 1. Normal working day");
    Run("normal", 0, (const char *[]){"-c", "data/normal.cfg", "-s", "42", NULL});
    Expect("normal", "violations: 0", "no invariant violations");
    Expect("normal", "still inside: 0", "every accepted car is washed");
    Expect("normal", "ENTRANCE CLOSED", "entrance closes");
    Expect("normal", "Bottleneck", "Gantt chart names the bottleneck");

    char log[256];
    TmpPath(log, sizeof(log), "normal", "log");
    char *journal = ReadFile(log);
    int ok = journal && strstr(journal, "TIMELINE OF POSTS") && strstr(journal, "AUDITOR");
    Report(ok, "normal", "journal has statistics, timeline and audit");
    free(journal);
}

static void TestAllEvents(void) {
    puts("== 2. All required events are shown");
    Run("events", 0, (const char *[]){"-c", "data/breakdowns.cfg", "-s", "1", NULL});
    const char *patterns[] = {
        "arrives at the entrance",
        "chooses",
        "enters .* post",
        "leaves .* post",
        "waits in .* queue",
        "waits on .* is full",
        "moves .* -> ",
        "starts on car",
        "finished car",
        "BREAKS DOWN",
        "is repaired",
        "is clean",
    };
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        Expect("events", patterns[i], patterns[i]);
    }
}

static void TestRushHour(void) {
    puts("== 3. Rush hour: blocking and turned-away cars");
    Run("rush", 0, (const char *[]){"-c", "data/rush-hour.cfg", "-s", "3", NULL});
    Expect("rush", "turned away [1-9]", "some cars are turned away");
    Expect("rush", "waits on .* is full", "posts are blocked by a full next stage");
    Expect("rush", "violations: 0", "queues never overflow");
}

static void TestStall(void) {
    puts("== 4. Stall");
    Run("dead", 2, (const char *[]){"-c", "data/dead-stage.cfg", "-s", "3", NULL});
    Expect("dead", "WARNING: program Premium needs stage wax", "warning at start");
    Expect("dead", "STALL: .* stage wax has no posts", "stall names the stage");
    Expect("dead", "violations: 0", "no invariant violations");

    // одна невыполнимая программа не должна останавливать мойку для остальных
    Run("deadprog", 2, (const char *[]){"-c", "data/dead-program.cfg", "-s", "2", NULL});
    long closed = LineOf("deadprog", "ENTRANCE CLOSED");
    long stall = LineOf("deadprog", "STALL:");
    Report(closed > 0 && stall > closed, "deadprog", "stall comes only after the entrance closes");

    // раньше из-за поломок пустых постов тупик находился только на 29-е сутки
    char cfg[256];
    TmpPath(cfg, sizeof(cfg), "deadfail", "cfg");
    WriteWithFailure("data/dead-stage.cfg", cfg, "failure = 0.2 5 40");
    Run("deadfail", 2, (const char *[]){"-c", cfg, "-s", "2", NULL});
    NotExpect("deadfail", "^\\[d[0-9]+ ", "stall is found on the first day despite failures");
}

static void TestPriorityAndZeroBuffers(void) {
    puts("== 5. Priority rules and zero-capacity queues");
    Run("priority", 0, (const char *[]){"-c", "data/priority.cfg", "-s", "8", NULL});
    Expect("priority", "Priority rule: program", "rule 'program' is used");
    Expect("priority", "violations: 0", "no invariant violations");
    Run("zero", 0, (const char *[]){"-c", "data/zero-buffer.cfg", "-s", "8", NULL});
    Expect("zero", "violations: 0", "no invariant violations");
    Expect("zero", "no free .* post and no waiting space", "cars go straight to a post");
}

static void TestBadParameters(void) {
    puts("== 6. Bad parameters");
    Run("badroute", 1, (const char *[]){"-c", "data/bad-route.cfg", NULL});
    Expect("badroute", "route must follow the line order", "route order is checked");
    Run("nofile", 1, (const char *[]){"-c", "data/missing.cfg", NULL});
    Expect("nofile", "cannot open config", "missing file is reported");
    Run("badmode", 1, (const char *[]){"-m", "lvie", NULL});
    Expect("badmode", "unknown view mode", "mistyped view mode is rejected");
    Run("badseed", 1, (const char *[]){"-s", "abc", NULL});
    Expect("badseed", "bad seed", "non-numeric seed is rejected");
}

static void TestSabotage(void) {
    puts("== 7. Sabotage: the auditor catches an off-by-one bug");
    Run("sabotage", 4, (const char *[]){"-c", "data/rush-hour.cfg", "-s", "3", "-x", NULL});
    Expect("sabotage", "I5 queue capacity is not exceeded +FAILED", "queue overflow is caught");
}

static void TestSameSeed(void) {
    puts("== 8. Same seed gives the same journal");
    Wait(Start("seed-a", (const char *[]){"-s", "77", NULL}));
    Wait(Start("seed-b", (const char *[]){"-s", "77", NULL}));
    char a[256], b[256];
    TmpPath(a, sizeof(a), "seed-a", "log");
    TmpPath(b, sizeof(b), "seed-b", "log");
    Report(SameFiles(a, b), "seed", "journals are identical");
}

static void TestSignals(void) {
    puts("== 9. Unlimited mode is stopped by signals");
    const int signals[] = {SIGINT, SIGTERM};
    const char *names[] = {"SIGINT", "SIGTERM"};
    for (int i = 0; i < 2; ++i) {
        pid_t pid = Start(names[i], (const char *[]){"-u", "-d", "1", "-s", "5", NULL});
        sleep(1);
        kill(pid, signals[i]);
        Report(Wait(pid) == 3, names[i], "exit code 3");
        Expect(names[i], "INTERRUPTED by user", "interruption is reported");

        char log[256];
        TmpPath(log, sizeof(log), names[i], "log");
        char *journal = ReadFile(log);
        Report(journal && strstr(journal, "RESULTS"), names[i], "journal has results");
        free(journal);
    }
}

static void TestManySeeds(void) {
    puts("== 10. Stress: 5 scenarios x 40 seeds");
    const char *configs[] = {"data/normal.cfg", "data/rush-hour.cfg", "data/breakdowns.cfg",
                             "data/zero-buffer.cfg", "data/priority.cfg"};
    int bad = 0;
    for (int c = 0; c < 5; ++c) {
        for (int seed = 1; seed <= 40; ++seed) {
            char s[16];
            snprintf(s, sizeof(s), "%d", seed);
            int code = Wait(Start("stress", (const char *[]){"-c", configs[c], "-s", s, NULL}));
            if (code != 0 || !Matches("stress", "out", "violations: 0")) {
                printf("    %s seed %d: exit %d\n", configs[c], seed, code);
                ++bad;
            }
        }
    }
    Report(bad == 0, "stress", "200 runs, 0 violations");
}

int main(int argc, char **argv) {
    if (argc > 1) {
        bin = argv[1];
    }
    if (access(bin, X_OK) != 0) {
        fprintf(stderr, "no %s, build the project first\n", bin);
        return 1;
    }
    snprintf(tmpDir, sizeof(tmpDir), "/tmp/carwash-tests-%d", (int)getpid());
    if (mkdir(tmpDir, 0700) != 0) {
        perror(tmpDir);
        return 1;
    }
    color = isatty(1);
    setvbuf(stdout, NULL, _IONBF, 0);  // чтобы порядок строк не путался с дочерними процессами

    TestNormalDay();
    TestAllEvents();
    TestRushHour();
    TestStall();
    TestPriorityAndZeroBuffers();
    TestBadParameters();
    TestSabotage();
    TestSameSeed();
    TestSignals();
    TestManySeeds();

    RemoveTmpDir();
    printf("\nPassed: %d, failed: %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
