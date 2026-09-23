CC = gcc
WINDRES = windres

CFLAGS = -Wall -Wextra -std=c11

TARGET = CTaskManager

SRC = src/main.c src/process.c
RES = resources/resource.o

LDFLAGS = -mwindows -lgdi32 -lcomctl32 -lpsapi -lshell32

$(TARGET).exe: $(SRC) $(RES)
	$(CC) $(CFLAGS) $(SRC) $(RES) -o $(TARGET).exe $(LDFLAGS)

resources/resource.o: resources/resource.rc resources/task.ico
	$(WINDRES) resources/resource.rc -O coff -o resources/resource.o

clean:
	del /Q $(TARGET).exe resources\resource.o