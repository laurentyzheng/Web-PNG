//findpng3.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h> // Hash Table
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <curl/multi.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include "./curl_xml/curl_xml.h"

#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define MAP_SIZE 10240
#define MAX_WAIT_MSECS 30*1000 /* Wait max. 30 seconds */

FRONTIER frontier_var; // stores all the urls obtained from the html pages
PNG_URLS png_urls_var; // stores unique png_urls

int max_png = 50;
int max_concurrent = 1;
char log_fname[256];
char seed_url[256];

//function declarautions
void multi_curl(CURLM * cm);
int process_SEED();
void add_to_multi( CURLM *cm, RECV_BUF* buf, char* url);
void get_opt(int argc, char * argv[], int *t_arg, int *m_arg, char *v_arg);
int is_png(RECV_BUF temp);
void write_to_log();
void clear_frontier();
void add_all_frontier_to_cm (CURLM * cm, RECV_BUF * buf_list);

int main( int argc, char* argv[] )
{
    /* HASHMAP
        key: url
        value: 'png' or 'non-png'
    */
    hcreate(MAP_SIZE); // MAP RESULTS -> 'png', VISITED -> 'non-png'

    frontier_var.size = 0;
    png_urls_var.size = 0;

    if (argc == 1) {
        strcpy(seed_url, SEED_URL);
    } else {
        //set argc as the last argument if available.
        strcpy(seed_url, argv[argc-1]);
    }
    printf("\nURL is %s\n", seed_url);
    
    get_opt(argc, argv, &max_concurrent, &max_png, log_fname);
    
    if(*log_fname != 0){
        if ( strcmp((log_fname + strlen(log_fname) - 4), ".txt") ){
            printf("log file name needs to end with .txt\n");
            exit(1);
        }
    }
    
    printf("max_concurrent: %d\nmax_png: %d\nlog_fname: %s\n", max_concurrent, max_png, log_fname);

    //record execution time
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    //do multi_curl
    CURLM *cm = curl_multi_init();
    curl_multi_setopt(cm, CURLMOPT_MAX_TOTAL_CONNECTIONS, max_concurrent);
	printf("start multi.\n");    
    multi_curl(cm);
    
    curl_multi_cleanup(cm);
    
    printf("******* ******* ********* ******** PRINTING URLS TO TEXT FILE ******** ******** ******* ******* \n");
    
    FILE *fptr;
    fptr = fopen("png_url.txt", "a");
    
    // print out all the pngs

    for ( int i = 0; i < png_urls_var.size; i++){
        fprintf(fptr, "%s\n",png_urls_var.png_urls[i]);
    }
    
    
    //get time at the end of exe
    if (gettimeofday(&tv, NULL) != 0) {
         perror("gettimeofday");
         abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("findpng3 execution time: %.6lf seconds\n", getpid(),  times[1] - times[0]);
    
    hdestroy();
    return 0;
}

void multi_curl(CURLM* cm){
    
    int still_running, msgs_left = 0;
    CURLMsg *msg = NULL;
    
    curl_global_init(CURL_GLOBAL_ALL);
    //add first set of URLs from SEED to frontier
    if (process_SEED() == -1){
        printf("seed url couldn't be curled.");
		return;
	}

    while(png_urls_var.size < max_png && frontier_var.size > 0){

		RECV_BUF * temp_buffer_list = malloc(sizeof(RECV_BUF) * frontier_var.size);
        add_all_frontier_to_cm(cm, temp_buffer_list);
        clear_frontier();
        
        curl_multi_perform(cm, &still_running);
        printf("started to multiperform.\n");

        /* -------------check for error only, no logic implemented------------- */
        do {
            int numfds = 0;
            int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);

            if(res != CURLM_OK) {
                fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
                return;
            } else {
//				printf("something happend waiting for cm, numfds = %d, # of running handles: %d.\n", numfds, still_running);
			}
			

            curl_multi_perform(cm, &still_running);
        } while(still_running);
        /* -------------------------------------------------------------------- */
        
        while ((msg = curl_multi_info_read(cm, &msgs_left))){
            RECV_BUF * temp_buf = process_curl_msg (msg, &frontier_var, &png_urls_var, max_png)
            if (valid_png(temp_buf->buf) && (hsearch(temp_buf->original_url, FIND) == NULL)){
                png_add(temp_buf->original_url, &png_urls_var);
            }
            free(temp_buf);
        }

		printf("finished proc multi queue, frontier size is: %d.\n", frontier_var.size);
       
		//cleanup
		free(temp_buffer_list);
        
    } //stop until no more handle or reach max png

}

int process_SEED(){
    RECV_BUF buffer;
    CURLcode res;
    long response_code;

    CURL *curl_handle = easy_handle_init(&buffer, seed_url);
    
    res = curl_easy_perform(curl_handle);

    if( res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
	//	printf("seed CURLE_OK\n");
	}
    
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( response_code >= 400 ) {
        fprintf(stderr, "Error.\n");
        return -1;
    } else {
		printf("response from seed: %d\n", response_code);
	}

    //adds urls from SEED to frontier
    int success = process_html(curl_handle, &buffer, &frontier_var);

	if (success == 0){
		printf("seed url proc: %d\n", success);
    	ENTRY e;
    	e.key = seed_url;
    	hsearch(e, ENTER);
	}

    //clean up
    curl_easy_cleanup(curl_handle);
    return success;
}

void add_to_multi(CURLM *cm, RECV_BUF* buf, char* url){
	CURL *eh = easy_handle_init(buf, url);
    curl_multi_add_handle(cm, eh);
	printf("added handle with url = %s to multi.\n", url);
}
 
void clear_frontier(){
    for ( int i = 0; i < frontier_var.size; i++){
        memset(frontier_var.frontier[i], 0, 256);
    }
    frontier_var.size = 0;
}

void add_all_frontier_to_cm (CURLM * cm, RECV_BUF * buf_list){
    for (int i = 0; i < frontier_var.size; i++){
	    ENTRY e;
        e.key = frontier_var.frontier[i];
        int entry_not_exist_in_hash_map = hsearch(e, FIND) == NULL;
        add_to_multi(cm, &buf_list[i], frontier_var.frontier[i]);

        if(entry_not_exist_in_hash_map){
            // add in the map and add to cm
            hsearch(e, ENTER);
        } else {
            //printf("!!!visited: %s", frontier_var.frontier[i]);
            memset(frontier_var.frontier[i], 0, 256); //flag the url as visited, so dont write to log twice.
        }
    }

    if(strlen(log_fname) != 0){
        write_to_log();
    }
}

void write_to_log(){
    FILE *n_fptr;
    n_fptr = fopen(log_fname, "a");

    // print out all the pngs
	printf("*******clearing frontier and writing to log**********\n");
    for ( int i = 0; i < frontier_var.size; i++){
        if(frontier_var.frontier[i]){
            fprintf(n_fptr, "%s\n", frontier_var.frontier[i]);
        }
    }
    fclose(n_fptr);
}

void get_opt(int argc, char * argv[], int *t_arg, int *m_arg, char *v_arg){
    int c;
    int t = 1;
    int m = 1;
    
    while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
        switch (c) {
        case 't':
            t = strtoul(optarg, NULL, 10);
            *t_arg = t;
                    
            if (t <= 0)
                return;
                    
            break;
        case 'm':
            m = strtoul(optarg, NULL, 10);
            *m_arg = m;
                
            if (m <= 0)
                return;
                
            break;
        case 'v':
            strcpy(v_arg, optarg);

            if (!optarg)
                return;

           break;
        default:
            return;
        }
    }
}
