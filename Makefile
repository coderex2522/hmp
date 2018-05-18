.PHONY:clean

CC :=gcc
CFLAGS := -Wall -g
LDFLAGS	:= ${LDFLAGS} -lxml2

all:client

hmp_log.o:hmp_log.c
	gcc -Wall -g	-c -o $@ $^ 

hmp_config.o:hmp_config.c
	gcc -Wall -g	-c -o $@ $^ -I /usr/local/include/libxml2/ ${LDFLAGS}
	
client:hmp_log.o hmp_config.o
	gcc -Wall -g $^ -o $@  ${LDFLAGS}
	
clean:
	rm -rf *.o