/**
 * @file: catpng.h
 * @brief: concatenate png from web
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include "../png_util/crc.h"
#include "../png_util/zutil.h"
#include "../png_util/lab_png.h"

#pragma once

void catpng(int argc, char* directory[]);
void set_signature(char* png_file);
U64 get_dest_length(FILE* fd, unsigned int * new_width, unsigned int * new_height);
void handle_ihdr(char* png_file, unsigned int width, unsigned int height);
void handle_idat(char* png_file, U8* deflated_data, unsigned int data_length);
void handle_iend(char* png_file,unsigned int data_length);
