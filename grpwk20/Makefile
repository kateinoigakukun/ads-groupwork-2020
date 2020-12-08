PROGS = gen enc syn seq dec eval
DATA = gendata encdata syndna seqdata decdata orgdata
CC = gcc
CFLAGS = -Wall

all: gen enc syn seq dec eval

.c:
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm  $(PROGS) $(DATA)
