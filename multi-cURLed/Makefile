# FINDPNG3 MAKEFILE --Laurent Zheng

CC = gcc 
CFLAGS_LIBS = $(shell pkg-config --cflags libxml-2.0 libcurl)
CFLAGS = -Wall $(CFLAGS_LIBS) -std=gnu99 -g
LD = gcc
LDFLAGS = -std=gnu99 -g 
LDLIBS = $(shell pkg-config --libs libxml-2.0 libcurl)

default: all

all: findpng3
	
findpng3: findpng3.c
	$(CC) $(CFLAGS) ./curl_xml/curl_xml.c $< -o findpng3 $(LDLIBS) $(LDFLAGS) -lz

clean:
	rm -rf findpng3 *.o *.png *.html png_url.txt output_*
