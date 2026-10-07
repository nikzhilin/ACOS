/**
 * @file main.c
 * @brief Автомойка, вариант 43. Одна итерация главного цикла = одна минута модели.
 *
 * Коды выхода: 0 - всё вымыто, 1 - плохие параметры, 2 - тупик,
 * 3 - прервали сигналом, 4 - аудитор нашёл нарушение, 5 - кончилась память.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "auditor.h"
#include "config.h"
#include "events.h"
#include "gantt.h"
#include "line.h"
#include "rng.h"
#include "soft_assert.h"
#include "stats.h"
#include "view_live.h"
#include "view_log.h"

/// в обработчике сигнала можно только выставить флаг, остальное делает main
static volatile sig_atomic_t stopRequested = 0;

static void OnSignal(int sig) {
    (void)sig;
    stopRequested = 1;
}

static void InstallSignals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = OnSignal;
    sa.sa_flags = SA_RESTART;  // чтобы сигнал не оборвал запись в журнал
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/// nanosleep прерывается сигналом, так что Ctrl+C срабатывает сразу
static void Delay(int ms) {
    struct timespec left = {ms / 1000, (long)(ms % 1000) * 1000000L};
    while (ms > 0 && nanosleep(&left, &left) == -1 && errno == EINTR && !stopRequested) {
    }
}

static void PrintResults(int fd, const Line *line, int color) {
    StatsPrint(fd, line);
    GanttPrint(fd, line, color);
    AuditorPrint(fd);
}

static int ExitCode(const Line *line) {
    if (AuditorViolations() > 0) {
        return 4;
    }
    switch (line->status) {
    case LINE_FINISHED: return 0;
    case LINE_STALLED: return 2;
    default: return 3;
    }
}

static void Usage(const char *prog) {
    dprintf(1,
            "Automatic car wash simulation (variant 43)\n"
            "Usage: %s [options]\n"
            "  -c FILE   configuration file (default: built-in typical car wash)\n"
            "  -s SEED   random seed for a reproducible run\n"
            "  -d MS     display delay per model minute, ms\n"
            "  -m MODE   view: live (animated line) or log (event feed)\n"
            "  -l FILE   journal file (default: carwash.log)\n"
            "  -u        unlimited mode: no day end and no car limit, stop with Ctrl+C\n"
            "  -i        ask the main parameters interactively\n"
            "  -x        sabotage: inject an off-by-one bug to prove the auditor catches it\n"
            "  -h        this help\n"
            "Exit codes: 0 done, 1 bad parameters, 2 stall, 3 interrupted, 4 invariant violated,\n"
            "            5 out of memory\n",
            prog);
}

int main(int argc, char **argv) {
    // ключи командной строки важнее того, что написано в конфиге
    const char *cfgFile = NULL, *seedArg = NULL, *delayArg = NULL;
    const char *modeArg = NULL, *journalArg = NULL;
    int unlimited = 0, interactive = 0, sabotage = 0, opt;
    while ((opt = getopt(argc, argv, "c:s:d:m:l:uixh")) != -1) {
        switch (opt) {
        case 'c': cfgFile = optarg; break;
        case 's': seedArg = optarg; break;
        case 'd': delayArg = optarg; break;
        case 'm': modeArg = optarg; break;
        case 'l': journalArg = optarg; break;
        case 'u': unlimited = 1; break;
        case 'i': interactive = 1; break;
        case 'x': sabotage = 1; break;
        case 'h': Usage(argv[0]); return 0;
        default: Usage(argv[0]); return 1;
        }
    }

    static Config cfg;
    char err[256];
    ConfigDefaults(&cfg);
    int rc = cfgFile ? ConfigLoadFile(&cfg, cfgFile, err, sizeof(err))
                     : ConfigParseText(&cfg, CONFIG_BUILTIN, err, sizeof(err));
    int number = 0;
    if (seedArg) {
        if (ParseInt(seedArg, &number) != 0) {
            dprintf(2, "carwash: bad seed '%s'\n", seedArg);
            return 1;
        }
        cfg.seed = (unsigned)number;
        cfg.seedSet = 1;
    }
    if (delayArg) {
        if (ParseInt(delayArg, &number) != 0) {
            dprintf(2, "carwash: bad delay '%s'\n", delayArg);
            return 1;
        }
        cfg.delayMs = number;
    }
    if (modeArg) {
        if (strcmp(modeArg, "live") == 0) {
            cfg.view = VIEW_LIVE;
        } else if (strcmp(modeArg, "log") == 0) {
            cfg.view = VIEW_LOG;
        } else {
            dprintf(2, "carwash: unknown view mode '%s' (use live or log)\n", modeArg);
            return 1;
        }
    }
    if (journalArg) {
        snprintf(cfg.journal, sizeof(cfg.journal), "%s", journalArg);
    }
    if (unlimited) {
        cfg.day = cfg.cars = 0;
    }
    cfg.sabotage = sabotage;
    if (rc == 0 && interactive) {
        ConfigInteractive(&cfg);
    }
    if (rc != 0 || ConfigValidate(&cfg, err, sizeof(err)) != 0) {
        dprintf(2, "carwash: configuration error: %s\n", err);
        return 1;
    }
    if (!cfg.seedSet) {
        cfg.seed = (unsigned)time(NULL) ^ (unsigned)getpid();
    }
    RngSeed(cfg.seed);

    // живая схема нужна только в терминале, в файл пишем обычный лог
    int tty = isatty(1);
    ViewMode view = cfg.view == VIEW_AUTO ? (tty ? VIEW_LIVE : VIEW_LOG) : cfg.view;
    int delay = cfg.delayMs >= 0 ? cfg.delayMs : (view == VIEW_LIVE ? 150 : 0);

    int journal = open(cfg.journal, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (journal == -1) {
        perror("carwash: cannot open journal, continuing without it");
    }

    AuditorInit(2);
    EventsSubscribe(AuditorListener);
    EventsSubscribe(StatsListener);
    EventsSubscribe(GanttListener);
    ViewLogInit(view == VIEW_LOG ? 1 : -1, tty, journal);
    EventsSubscribe(ViewLogListener);
    if (view == VIEW_LIVE) {
        ViewLiveInit(1);
        EventsSubscribe(ViewLiveListener);
    } else {
        dprintf(1, "=== AUTOMATIC CAR WASH: simulation of one working day ===\n");
        ConfigPrint(&cfg, 1);
        dprintf(1, "\n");
    }
    if (journal != -1) {
        dprintf(journal, "=== AUTOMATIC CAR WASH: journal ===\n");
        ConfigPrint(&cfg, journal);
        dprintf(journal, "\n");
    }

    InstallSignals();
    static Line line;
    LineInit(&line, &cfg);
    while (line.status == LINE_RUNNING) {
        if (stopRequested) {
            LineInterrupt(&line);
            break;
        }
        LineTick(&line);
        Delay(delay);
    }
    if (view == VIEW_LIVE) {
        ViewLiveEnd();
    }

    PrintResults(1, &line, tty);
    if (journal != -1) {
        PrintResults(journal, &line, 0);
        dprintf(journal, "seed %u (repeat the run with -s %u)\n", cfg.seed, cfg.seed);
        close(journal);
        dprintf(1, "Journal saved to %s (seed %u)\n", cfg.journal, cfg.seed);
    }

    if (SoftAssertFailures() > 0) {
        dprintf(2, "carwash: soft asserts failed %ld time(s), see messages above\n",
                SoftAssertFailures());
    }

    int code = ExitCode(&line);
    LineFree(&line);
    AuditorFree();
    GanttFree();
    return code;
}
