CC = gcc

CFLAGS = -Wall -Wextra -std=c11

TARGET = CTaskManager

SRC = src/main.c src/process.c

LDFLAGS = -mwindows -lgdi32 -lcomctl32 -lpsapi -lshell32

$(TARGET).exe: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET).exe $(LDFLAGS)

clean:
	del /Q $(TARGET).exe