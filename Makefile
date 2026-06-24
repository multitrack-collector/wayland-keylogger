CC      := gcc
CFLAGS  := -Wall -Wextra -O2
LDFLAGS := 

all: xwayland_keylogger

xwayland_keylogger: xwayland_keylogger.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f xwayland_keylogger

.PHONY: all clean
