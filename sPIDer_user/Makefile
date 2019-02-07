TARGET = user_governor
# CROSS_COMPILE=aarch64-linux-gnu- make
#
CC=gcc

ifdef DEBUG
	CFLAGS=-static -lc -g
else
	CFLAGS=-static -lc -O2
endif

all:
	$(CROSS_COMPILE)$(CC) $(CFLAGS) src/pid.c src/controller.c src/main.c -o $(TARGET)

clean:
	rm -rf $(TARGET)

.PHONY: all clean
