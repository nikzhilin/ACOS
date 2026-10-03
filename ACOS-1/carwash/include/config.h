#ifndef CONFIG_H
#define CONFIG_H

// Параметры модели. Читаются из файла вида "ключ = значение" (-c),
// а если файла нет - из встроенной конфигурации CONFIG_BUILTIN.

#define MAX_STAGES   6
#define MAX_POSTS    4
#define MAX_PROGRAMS 6
#define MAX_QUEUE    16  // размер массива; вместимость из конфига строго меньше
#define NAME_LEN     12

typedef enum { PRIO_FIFO, PRIO_PROGRAM, PRIO_SHORTEST } PriorityRule;

typedef enum {
    VIEW_AUTO,  // live в терминале, log если вывод перенаправлен
    VIEW_LOG,
    VIEW_LIVE,
} ViewMode;

typedef struct {
    char name[NAME_LEN];
    int posts;  // 0 постов допустимо - так проверяется тупик
    int tmin, tmax;
    int queue;
} StageSpec;

typedef struct {
    char name[NAME_LEN];
    int weight;  // как часто клиенты выбирают программу
    int priority;
    int route[MAX_STAGES];
    int len;
} ProgramSpec;

typedef struct {
    StageSpec stage[MAX_STAGES];
    int stages;
    ProgramSpec program[MAX_PROGRAMS];
    int programs;

    int cars;  // 0 - без ограничения
    int arrMin, arrMax;
    int day;  // минут, 0 - без ограничения
    double failP;
    int repairMin, repairMax;
    PriorityRule rule;

    unsigned seed;
    int seedSet;
    int delayMs;  // -1 - выбрать по режиму
    ViewMode view;
    char journal[256];
    int sabotage;
} Config;

extern const char *CONFIG_BUILTIN;

void ConfigDefaults(Config *c);

// функции разбора возвращают 0 или -1 и пишут причину в err
int ConfigParseText(Config *c, const char *text, char *err, int errSize);
int ConfigLoadFile(Config *c, const char *path, char *err, int errSize);
int ConfigValidate(const Config *c, char *err, int errSize);

void ConfigInteractive(Config *c);
void ConfigPrint(const Config *c, int fd);
const char *ConfigRuleName(PriorityRule rule);

// 0 и число в *out, или -1, если строка - не целое число
int ParseInt(const char *s, int *out);

#endif
