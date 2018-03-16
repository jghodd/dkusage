PRG=dkusage

SRCS=dkusage.c 
OBJS=${SRCS:.c=.o}

CFLAGS:=-s -Wall -O -ggdb 

${PRG}: ${OBJS}

clean:
	rm -f ${OBJS} ${PRG} *~ 

install: ${PRG}
	cp ${PRG} /usr/bin

