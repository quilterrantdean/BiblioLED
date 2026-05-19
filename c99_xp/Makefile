CC ?= gcc
CFLAGS_COMMON = -m32 -std=c99 -Wall -Wextra -Iinclude
LDFLAGS_COMMON = -lsqlite3

SRC = src/main_cli.c src/config_store.c src/datetime_utils.c src/loans_repo.c src/metrics.c src/csv_export.c
OUT = biblioled_cli.exe

all: release

debug:
	$(CC) $(CFLAGS_COMMON) -O0 -g $(SRC) -o $(OUT) $(LDFLAGS_COMMON)

release:
	$(CC) $(CFLAGS_COMMON) -O2 -s $(SRC) -o $(OUT) $(LDFLAGS_COMMON)

clean:
	rm -f $(OUT)
