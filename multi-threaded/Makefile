CFLAGS?=-std=c99 -D_GNU_SOURCE -Wall -O2 
CC=gcc

default: all

all: paster

bin:
	mkdir -p bin/
	
paster: paster.c  
	$(CC) $(CFLAGS)  ./png_util/zutil.c ./png_util/crc.c ./catpng/catpng.c $< -o paster -lcurl -lz -lpthread

clean:
	rm -rf paster *.o all.png ./output/*
