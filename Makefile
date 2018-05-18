.PHONY:clean

CC :=gcc
CFLAGS := -Wall -g
LDFLAGS	:= ${LDFLAGS} -lxml2 -lrdmacm -libverbs -lpthread

all:test

hmp_log.o:hmp_log.c
	gcc -Wall -g	-c -o $@ $^ 

hmp_config.o:hmp_config.c
	gcc -Wall -g	-c -o $@ $^ -I /usr/local/include/libxml2/ -lxml2

hmp_context.o:hmp_context.c
	gcc -Wall -g	-c -o $@ $^ -lpthread

hmp_transport.o:hmp_transport.c
	gcc -Wall -g	-c -o $@ $^ ${LDFLAGS}
	
hmp_node.o:hmp_node.c
	gcc -Wall -g	-c -o $@ $^ -lpthread
	
test:hmp_log.o hmp_config.o hmp_context.o hmp_transport.o hmp_node.o test.o
	gcc -Wall -g $^ -o $@  ${LDFLAGS}
	
clean:
	rm -rf *.o