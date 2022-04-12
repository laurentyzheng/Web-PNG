#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <pthread.h>
#include "./catpng/catpng.h"
#include <stdbool.h>

#define ECE252_HEADER "X-Ece252-Fragment: "

// Struct definitions
typedef struct {
    char * data;
    size_t length;
    unsigned int image_number;
} buffer;

typedef struct {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
} int_addres;

struct thread_args              /* thread input parameters struct */
{
    unsigned int image_num;
};

// Function Prototypes
int main();
void retrive(char* url, unsigned int *continue_checking);
void round_robin(char* url, unsigned int *server, unsigned int image_number); // takes url and round robins it
void get_opt(int argc, char argv[],int *t_arg, int *n_arg);
void * main_retrive(void* arg);
void single_retrive(int image_number);
void gen_thread (unsigned int num_threads, unsigned int image_number);

// Input:   -t number of tasks
//          -n image number
// Output:  use 't' threads and output the image to all.png
int main (int argc, char* argv[]) {
    int t = 1;
    int n = 1;
    get_opt(argc, argv, &t, &n);
    if(t <=0 || (n != 1 && n!=2 && n!=3)){
        printf("Please Enter Valid Arguments\n");
        return -1;
    }
    
    gen_thread(t,n);
    //single_retrive(n);
    
    // concatenate the images
    char* directory[50];
    for(int i = 0; i < 50; i++){
        directory[i]  = malloc(sizeof(unsigned int) + strlen("./output/.png") + 1);
        sprintf(directory[i],"%s%d%s","./output/", i, ".png");
    }
    catpng(50,directory);
    for(int i = 0; i < 50; i++){
        free(directory[i]);
    }
    return 0;
}

// Function Definitions

size_t write_callback(char *ptr, size_t size, size_t nmemb, buffer* png_data){
    
    size_t updated_length = png_data->length + size * nmemb;
    
    png_data->data = realloc(png_data->data,updated_length + 1);

    memcpy(png_data->data + png_data->length, ptr, size*nmemb);
    
    png_data->length = updated_length;
    png_data->data[png_data->length] = '\0';

    return size * nmemb;
}

size_t write_header_callback(char *ptr, size_t size, size_t nmemb, buffer* png_data){
    
    size_t updated_length = png_data->length + size * nmemb;
    size_t realsize = size * nmemb;
    png_data->data = realloc(png_data->data,updated_length + 1);

    memcpy(png_data->data + png_data->length, ptr, size*nmemb);
    
    png_data->length = updated_length;
    png_data->data[png_data->length] = '\0';

     if (realsize > strlen(ECE252_HEADER) && strncmp(ptr, ECE252_HEADER, strlen(ECE252_HEADER)) == 0  ) {
            png_data->image_number = atoi(ptr + strlen(ECE252_HEADER));
        }
    return size * nmemb;
}

// URL: http://ece252-n.uwaterloo.ca:2520/image?img=M
//      server: N 1 -3 
//      image:  M 1 - 3



/*

*/
//single thread

unsigned int continue_checking = 0;

void single_retrive(int image_number){
    char* running_url = malloc(strlen(".uwaterloo.ca:2520/image?img=") + strlen("http://ece252-") + 2 * sizeof(image_number));
    unsigned int server = 1;
    sprintf(running_url,"%s%d%s%d","http://ece252-",server, ".uwaterloo.ca:2520/image?img=", image_number);
    /* Array keeps track of the images downloaded
        since there are 50 strips the value of the
        array at the index value represents whether
        the image has been downloaded or not
    */

    /* Run a while loop until all 50 array values are true
    */
    while(continue_checking != 50){
        round_robin(running_url, &server,image_number);
        // new url is round robined and fed into retrieve
        retrive(running_url, &continue_checking);
    }
    free(running_url);
}

pthread_mutex_t lock;

void * main_retrive(void * arg){

    //input from previous to threading
    struct thread_args * p_in = arg;

    unsigned int image_number = p_in->image_num;

    char* running_url = malloc(strlen(".uwaterloo.ca:2520/image?img=") + strlen("http://ece252-") + 2 * sizeof(image_number));
    unsigned int server = 1;
    sprintf(running_url,"%s%d%s%d","http://ece252-",server, ".uwaterloo.ca:2520/image?img=", image_number);
    /* Array keeps track of the images downloaded
        since there are 50 strips the value of the
        array at the index value represents whether
        the image has been downloaded or not
    */


    /* Run a while loop until all 50 array values are true
    */
    while(continue_checking < 50){
        round_robin(running_url, &server,image_number);
        // new url is round robined and fed into retrieve
        retrive(running_url, &continue_checking);
        
    }
    free(running_url);
    
    return NULL;
}


void gen_thread (unsigned int num_threads, unsigned int image_number){
    
    //create thread ids and parameters
    pthread_t *p_tids = malloc(sizeof(pthread_t) * num_threads);
    struct thread_args in_params[num_threads];
    
    //init mutual exclusion
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return;
    }
        
    for (int i = 0; i < num_threads; i++) {
        
        in_params[i].image_num = image_number;
        
        int error = pthread_create(p_tids + i, NULL, main_retrive, in_params + i);
        
        if (error != 0)
                printf("\nThread can't be created : [%s]", strerror(error));
        
    }
    
    for (int i=0; i < num_threads; i++) {
        pthread_join(p_tids[i], NULL);
    }
    
    //clean up
    free(p_tids);
    pthread_mutex_destroy(&lock);
}

/* 
    
    retrive(char* url): 
        Retreives an image which is 400 px by 300px
        given the URL of the image. It comes in 50 
        strips each 6px high in PNG format. Assemble them into 
        a PNG file. The http header tells which fragment number
        it is "X-Ece252-Fragment: 17"
    
    Output: 
        An assembeled PNG file. 
        Returns the header?
        . //? TODO: Should save in a region of memory

*/

bool array_checker[50] = {false};
void retrive(char* url, unsigned int *continue_checking){
    // use curl to get the image
    CURL *curl_handle;
    CURLcode res;
    
    
    buffer png_data;
    buffer header_data;
    
    png_data.data = header_data.data = NULL;
    png_data.length = header_data.length = 0;
    header_data.image_number = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl_handle = curl_easy_init();
    
    if (curl_handle) {

        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &png_data);
        
        /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, write_header_callback); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &header_data);

        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK)   {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl_handle);
    }
    // printf("%d\n",header_data.image_number - 1);
    pthread_mutex_lock(&lock);
    if(!array_checker[header_data.image_number]){
        
        array_checker[header_data.image_number] = true;
        *continue_checking += 1;
        
        // write to a file
        char* file_name = malloc(sizeof(unsigned int) + strlen("./output/.png") + 1);
        sprintf(file_name,"%s%d%s","./output/", header_data.image_number, ".png");
        file_name[strlen(file_name)] = '\0';

        FILE* new_image = fopen(file_name,"w"); 
        
        if (new_image == NULL){
            fprintf(stderr,"error = %s\n",strerror(errno));
            return;
        }

        fwrite(png_data.data, 1, png_data.length, new_image);
        
        fclose(new_image);

        printf("%s Strips Downloaded: %d\n",file_name, *continue_checking);
        
        free(file_name);
        
    }
    pthread_mutex_unlock(&lock);
    
    free(png_data.data);
    free(header_data.data);
    curl_global_cleanup();
}

/*
    round_robin(char* url):
        Takes a URL and checks if the server has been used or not and
        Round Robins it to the next server, i.e. 1 -> 2 -> 3 -> 1...
        Storing the new url in the passed in arguments address
*/
void round_robin(char* url, unsigned int *server, unsigned int image_number){
    // loops through the url and increments the server part
    *server = (*server)%3 + 1;
    sprintf(url,"%s%d%s%d","http://ece252-",*server, ".uwaterloo.ca:2520/image?img=", image_number);
}


void get_opt(int argc, char argv[], int *t_arg, int *n_arg){
    int c;
    int t = 1;
    int n = 1;
    char *str = "option requires an argument";
    
    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
        case 't':
	    t = strtoul(optarg, NULL, 10);
	    // printf("option -t specifies a value of %d.\n", t);
        *t_arg = t;
	    if (t <= 0) {
                return;
            }
            break;
        case 'n':
            n = strtoul(optarg, NULL, 10);
	    //printf("option -n specifies a value of %d.\n", n);
            *n_arg = n;
            if (n <= 0 || n > 3) {
                
            return;
            }
            break;
        default:
            return;
        }
    }
}

