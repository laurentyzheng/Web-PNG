#include <stdio.h>
#include "catpng.h"

const U8 png_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

void set_signature(char* png_file){
    FILE * allpng;
    allpng = fopen(png_file, "a+b"); // appending to the file
    fwrite(png_signature,8,1,allpng);
    fclose(allpng);
}

/* Print out file name and dimentsion of PNG */
U64 get_dest_length(FILE* fd, unsigned int * new_width, unsigned int * new_height){
    unsigned int width = 0;
    unsigned int height = 0;
    
    // (4) Get the width and height of image
    fread(&width, sizeof(width), 1, fd);
    width = ntohl(width);
    fread(&height, sizeof(height), 1, fd);
    height = ntohl(height);
    
    *new_width = width;
    *new_height += height;
    
    return height*(width*4 + 1);
}

void handle_ihdr(char* png_file, unsigned int width, unsigned int height){
    FILE * allpng;
    allpng = fopen(png_file, "a+b"); // appending to the file
    // skip over the existing signature
    fseek(allpng,8,SEEK_SET);
    // Set the Length
    unsigned int chunk_length = htonl(13);
    fwrite(&chunk_length, 4, 1, allpng);
    
    // Set the Type
    unsigned char* type = "IHDR";
    fwrite(type,4,1,allpng);
    
    // Set the updated width and height
    width = htonl(width);
    height = htonl(height);
    fwrite(&width,4,1,allpng);
    fwrite(&height,4,1,allpng);
    
    // set the bit depth and colour type
    unsigned int temp_value =  htonl(0x08060000);
    fwrite(&temp_value,2,1,allpng);
    temp_value = htonl(0x0000);
    fwrite(&temp_value,1,1,allpng);
    fwrite(&temp_value,1,1,allpng);
    fwrite(&temp_value,1,1,allpng);
    
    // Set the CRC
    unsigned char* data_buff = malloc(17);
    
    fseek(allpng,12,SEEK_SET); // just before the type
    fread(data_buff,17,1,allpng);
    
    unsigned int long crc_result = crc(data_buff, 17);
    crc_result = htonl(crc_result);

    fwrite(&crc_result,4,1,allpng);
    free(data_buff);
    fclose(allpng);
}

void handle_idat(char* png_file, U8* deflated_data, unsigned int data_length){
    FILE * allpng;
    allpng = fopen(png_file, "a+b"); // appending to the file
    
    // Seek till just after the IHDR chunk
    fseek(allpng, 33 ,SEEK_SET);

    // Write the length
    data_length = htonl(data_length);
    fwrite(&data_length,4,1,allpng);
    
    // Write the type
    unsigned char* type = "IDAT";
    fwrite(type,4,1,allpng);

    // Write the data
    fwrite(deflated_data, ntohl(data_length), 1, allpng);
    
    // Write the CRC
    unsigned char* data_buff = malloc(ntohl(data_length) + 4);
    fseek(allpng,37,SEEK_SET); // just before the Type

    fread(data_buff,ntohl(data_length) + 4, 1,allpng);

    unsigned int long crc_result = crc(data_buff, ntohl(data_length) + 4);
    crc_result = htonl(crc_result);

    fwrite(&crc_result,4,1,allpng);
    free(data_buff);
    fclose(allpng);
}

void handle_iend(char* png_file,unsigned int data_length){
    FILE * allpng;
    allpng = fopen(png_file, "a+b"); // appending to the file
    fseek(allpng, 45 + data_length,SEEK_SET);

    // Write IEND length i.e. 0 
    unsigned int chunk_length = 0;
    fwrite(&chunk_length, 4, 1, allpng);
    
    // Write IEND TYPE
    unsigned char* type = "IEND";
    fwrite(type,4,1,allpng); 

    // Write IEND CRC
    unsigned int long crc_result = htonl(crc(type, 4));
    
    fwrite(&crc_result,4,1,allpng);

    fclose(allpng);
}
 
/*
    catpng: Takes in a 'argc' which is the number of 
            series of strings pointing to cropped
            images which this function will concatenate vertically
    output: a file 'all.png' which is the concatenated images
*/
void catpng(int argc, char* directory[]) {
    unsigned char* inflated_data[argc -1];
    int inflated_data_size[argc-1];

    unsigned int new_width = 0;
    unsigned int new_height = 0;
    
    U64 inflated_data_length = 0;
    U64 recent_inflated_data_length = 0;


    for (int i = 1; i < argc; i++) {

        FILE* fd = fopen(directory[i],"r");

        if (fd == NULL){
            fprintf(stderr,"error = %s\n",strerror(errno));
            exit(errno);
        }

        unsigned int chunk_length = 0;
        unsigned char chunk_type[4];
        fseek(fd,8,SEEK_SET); 


        while (fread(&chunk_length, 4, 1, fd)) { // CHUNK_LENGTH
            // CHUNK_TYPE 
            fread(chunk_type, sizeof(chunk_type), 1, fd);
            chunk_type[4] = '\0'; // if you dont add this you get giberish at the end eg IDA ?pNg ?d

            chunk_length = ntohl(chunk_length); // converts from network to host
            
            U8* data_buff = malloc(chunk_length);
            
            if(chunk_type[3] == 'R'){
                // i.e. IHDR
                // (4) Get dest length from width and length
                recent_inflated_data_length = get_dest_length(fd, &new_width, &new_height);
                inflated_data_length =  new_height*(new_width*4 + 1);
                fseek(fd, chunk_length + 20, SEEK_SET);
            }
            else if (chunk_type[3] == 'T') { 
                // i.e. IDAT
                // update the idat data length

                unsigned char* deflated_data_read = malloc(chunk_length);
                fread(deflated_data_read, chunk_length, 1, fd);

                inflated_data[i - 1] = malloc(recent_inflated_data_length);
                inflated_data_size[i-1] = recent_inflated_data_length;
              
                U8* inflated_data_buff = malloc(recent_inflated_data_length);

                mem_inf(inflated_data[i - 1], &recent_inflated_data_length, deflated_data_read, chunk_length);

                free(deflated_data_read);
                free(inflated_data_buff);               

                fseek(fd, 4, SEEK_CUR); // skipping over the crc section
            }
            else {
                free(data_buff);
                break;
            }
            free(data_buff);
        }
        fclose(fd);
    }

    unsigned char* total_inflated = malloc(inflated_data_length);
    unsigned int running_pos = 0;
    for(int i = 0; i < argc - 1; i++){
        memcpy(total_inflated + running_pos,inflated_data[i],inflated_data_size[i]);
        running_pos+=inflated_data_size[i];
        free(inflated_data[i]);
    }
    U8* total_deflated = malloc(inflated_data_length);
    // ->   Deflate this stacked data using 'mem_def'
    mem_def(total_deflated, &inflated_data_length, total_inflated , inflated_data_length, Z_DEFAULT_COMPRESSION);

    //! SET THE SIGNATURE
    set_signature("./all.png");

    //! Update the IHDR chunk
    handle_ihdr("./all.png", new_width, new_height);

    //! Update the IDAT chunk
    handle_idat("./all.png", total_deflated, inflated_data_length);
    
    //! Update the IEND chunk
    handle_iend("all.png",inflated_data_length);

    free(total_deflated);
    free(total_inflated);
}
