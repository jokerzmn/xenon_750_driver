TARGET := driver
CC := gcc
LDFLAGS := -lconfig -lusb-1.0
CFLAGS := -Werror -Wall -Wextra -Wfloat-equal -Wshadow -Wno-unused-parameter -std=c99 -O2
SRC := main.c

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

