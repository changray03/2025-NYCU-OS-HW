.PHONY: all clean

CC= riscv64-linux-gnu-gcc
FLAG= -Wall -Wextra -Werror -O3 -Wpedantic -static
SOURCE= sched_demo_314551128.c
TARGET= sched_demo_314551128

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(FLAG) -o $@ $^

clean: 
	@rm -f $(TARGET)