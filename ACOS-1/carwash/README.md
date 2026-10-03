# Автомойка (вариант 43)

Последовательная имитация автоматической автомойки на C (libc + системные вызовы).

## Сборка и запуск

Все команды - из папки `carwash`.

```bash
cmake -S . -B build && cmake --build build
./bin/carwash                         # обычный день, живая схема в терминале
./bin/carwash -c data/rush-hour.cfg   # другой сценарий
./bin/carwash -m log -s 42            # лента событий, повторяемый прогон
./bin/carwash -u                      # без конца дня, остановка по Ctrl+C
./bin/carwash -h                      # все ключи
```

| Ключ | Что делает |
|---|---|
| `-c FILE` | файл конфигурации (пример формата - `data/normal.cfg`) |
| `-s SEED` | seed генератора |
| `-d MS` | задержка на одну минуту модели |
| `-m live\|log` | живая схема или лента событий |
| `-l FILE` | журнал, по умолчанию `carwash.log` |
| `-u` | без ограничения времени |
| `-i` | ввести основные параметры с клавиатуры |
| `-x` | специально сломать проверку места, чтобы аудитор это поймал |

Коды выхода: `0` - всё вымыто, `1` - плохие параметры, `2` - тупик,
`3` - прервали сигналом, `4` - нарушен инвариант, `5` - нет памяти.

## Что где лежит

```
include/   заголовки
src/       реализация
  line.c       координатор линии (сама модель)
  car.c post.c buffer.c   машина, пост, очередь
  events.c     шина событий
  view_log.c view_live.c auditor.c gantt.c stats.c   подписчики на события
  config.c     разбор параметров
  main.c       ключи, сигналы, главный цикл
data/      сценарии
tests/     run_tests.c - сценарные тесты
```

Модель ничего не печатает сама, она только шлёт события. Лог, живая схема,
аудитор инвариантов, диаграмма Ганта и статистика на них подписаны.
Каждую минуту стадии обходятся от сушки к въезду, поэтому освободившееся
место сразу занимает следующая машина и блокировки снимаются сами.

## Тесты, форматирование, clang-tidy

```bash
cmake --build build --target check    # сценарные тесты (tests/run_tests.c)
cmake --build build --target format   # clang-format по .clang-format
cmake --build build --target tidy     # clang-tidy по .clang-tidy
```

Тесты написаны на C: `run_tests` запускает `bin/carwash` на файлах из `data/`
через fork/exec и проверяет код выхода и вывод. Их же можно запустить через
`ctest --test-dir build` или напрямую `./bin/run_tests ./bin/carwash`.

Для `format` и `tidy` нужны clang-format и clang-tidy. На macOS: `brew install llvm`
(CMake сам найдёт их в `/opt/homebrew/opt/llvm/bin`), на Linux: `apt install clang-format clang-tidy`.
Если их нет, цели напишут, что поставить.
