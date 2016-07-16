# Fairly generic cross-compilation makefile for simple programs
CC=$(CROSSTOOL)/$(ARM)/bin/gcc
NAME=aurora
TARGET=$(NAME).new
all: $(TARGET)
OBJS=$(NAME).o common.o sbus.o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) 
	$(CROSSTOOL)/$(ARM)/bin/strip $(TARGET)

$(NAME).o: $(NAME).c common.h $(NAME).h
common.o: common.c common.h

clean:
	rm -f $(NAME) $(OBJS)
