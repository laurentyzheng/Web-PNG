CFLAGS?=-std=c99 -D_GNU_SOURCE -Wall -O2 
CC=gcc

default: all

all: paster2

bin:
	mkdir -p bin/
	
paster2: paster2.c  
	$(CC) $(CFLAGS) ./shm/shm_stack.c ./cURL_IPC/main_2proc.c ./png_util/zutil.c ./png_util/crc.c ./catpng/catpng.c $< -o paster2 -lcurl -lz -lpthread

clean:
	rm -rf paster2 *.o all.png

