TARGET: slicz slijent

CC = cc
CFLAGS = -O2
LFLAGS = 

slijent: slijent.o err.o
	$(CC) $(LFLAGS) $^ -o $@
	
slicz: slicz.o err.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm -f slicz slijent *.o *~ *.bak
