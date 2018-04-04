CC=gcc
IDIR =../pa1_starter_code
CFLAGS = -std=c11 -pedantic -Wall -I$(IDIR)
FILES = main functions
CSRC = $(patsubst %, %.c , $(FILES))
BFILES = $(patsubst %, %.o , $(FILES))
TARGET = main
LDK = -o
CDK = -c

main: $(BFILES)
	$(CC) $(LDK) $(TARGET) $(BFILES) $(CFLAGS)

%.o: %.c
	$(CC) $(CDK) $(LDK) $@ $< -O2 $(CFLAGS)

clean:
	rm -f $(BFILES) $(TARGET)