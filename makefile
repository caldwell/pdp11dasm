#
# PDP11Dasm makefile for Linux
#

CC = gcc
CFLAGS = -Wall
LIBS = -lm
TARGET = pdp11dasm
OBJS = $(TARGET).o

${TARGET}:  $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)
	strip $(TARGET)

$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o
	rm -f $(TARGET)

# end of file

