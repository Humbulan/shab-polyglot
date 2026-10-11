CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
TARGET  := shab
SRC     := shab.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o

test: $(TARGET)
	@./$(TARGET) example.ir
	@./$(TARGET) arith.ir
	@./$(TARGET) countdown.ir
	@./$(TARGET) memory.ir
	@echo "ALL TESTS PASSED"
