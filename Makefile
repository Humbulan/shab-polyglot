CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -O2
CFLAGS_DEV = -std=c11 -Wall -Wextra -Wpedantic -O0 -g
PREFIX ?= /usr/local

TARGET = shab
SRC = shab.c

.PHONY: all dev test clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

dev: $(SRC)
	$(CC) $(CFLAGS_DEV) $< -o $(TARGET)

test: $(TARGET)
	./test.sh

install: $(TARGET)
	install -m 0755 $(TARGET) $(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(TARGET)
