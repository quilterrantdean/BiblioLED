CC ?= gcc
CFLAGS_COMMON = -m32 -std=c99 -Wall -Wextra -Iinclude
LDFLAGS_COMMON = -lsqlite3
WIN_LIBS = -lcomctl32

SRC_CORE = src/config_store.c src/datetime_utils.c src/loans_repo.c src/metrics.c src/csv_export.c
SRC_CLI = src/main_cli.c $(SRC_CORE)
SRC_GUI = src/win_gui.c $(SRC_CORE)

OUT_CLI = biblioled_cli.exe
OUT_GUI = biblioled_gui.exe

all: gui

release:
	$(CC) $(CFLAGS_COMMON) -O2 -s $(SRC_CLI) -o $(OUT_CLI) $(LDFLAGS_COMMON)

debug:
	$(CC) $(CFLAGS_COMMON) -O0 -g $(SRC_CLI) -o $(OUT_CLI) $(LDFLAGS_COMMON)

gui:
	$(CC) $(CFLAGS_COMMON) -O2 -s $(SRC_GUI) -o $(OUT_GUI) $(LDFLAGS_COMMON) $(WIN_LIBS)

gui-debug:
	$(CC) $(CFLAGS_COMMON) -O0 -g $(SRC_GUI) -o $(OUT_GUI) $(LDFLAGS_COMMON) $(WIN_LIBS)

clean:
	rm -f $(OUT_CLI) $(OUT_GUI)
