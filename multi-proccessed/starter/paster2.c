#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include "./catpng/catpng.h"
#include "./shm/shm_stack.h"

#define BUF_SIZE 10240  /* 1024*10 = 10K */
#define MAX_IMAGES 50
#define SEM_PROC 1
#define NUM_SEMS 2

int main();
int* shm_init();
void producer_pocess(unsigned int N, int prod_num);
RECV_BUF cURL_image(char* url);
void consumer_process(unsigned int X);
void round_robin(char* url, unsigned int *server, unsigned int image_number, unsigned int seq);
void catpng_custom();

ISTACK * p_shm_stack;
RECV_BUF * final_ordered_buffer;

sem_t * sems;
int * p_counter;

int main( int argc, char** argv )
{
    
    if (argc != 6){
        printf("Not Enough Input\n");
        return -1;
    }
    unsigned int B = atoi(argv[1]);
    unsigned int P = atoi(argv[2]);
    unsigned int C = atoi(argv[3]);
    unsigned int X = atoi(argv[4]);
    unsigned int N = atoi(argv[5]);
    
    if(B <= 0 || P <= 0 || C <= 0 || X < 0 || N <= 0 || N > 3){
        printf("Please Enter Valid Arguments %d\n",B);
        return -1;
    }
    
    /* allocate shared memory for buffers initialize them*/
    int* id_array = shm_init(B);
    int shmid = id_array[0];
    int shmid_final = id_array[1];

    /* allocate shared memory for semaphores and counter */
    int shmid_counter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_sems = shmget(IPC_PRIVATE, sizeof(sem_t) * NUM_SEMS, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if (shmid_counter  == -1 || shmid_sems == -1) {
        perror("shmget");
        abort();
    }

    /* attach to shared memory regions */
    sems = shmat(shmid_sems, NULL, 0);
    p_counter = shmat(shmid_counter, NULL, 0);
    
    if (sems == (void *) -1 || p_counter == (void *) -1) {
        perror("shmat");
        abort();
    }

    /* initialize shared memory varaibles */
    if ( sem_init(&sems[0], SEM_PROC, B) != 0 ) { // sems[0] -> spaces_empty
        perror("sem_init(sem[0])");
        abort();
    }
    if ( sem_init(&sems[1], SEM_PROC, 0) != 0 ) { // sems[1] -> items_buff
        perror("sem_init(sem[1])");
        abort();
    }
    //counter to check if all image s licesare downloaded.
    *p_counter = 0;
    
    /* FORKING PROCESS FOR BOTH CONSUMERS AND PRODUCERS */
    pid_t pid = 0;
    
    pid_t cons_cpids[C];
    pid_t prod_cpids[P];
    int prod_state;
    int cons_state;
    
    //record execution time
    double times[2];
    struct timeval tv;
    
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    //producer process
    for (int i = 0; i < P; i++) {
        pid = fork();
        
        if ( pid > 0 ) {        /* parent proc */
            prod_cpids[i] = pid;
        } else if ( pid == 0 ) { /* child proc */
            producer_pocess(N, i);
            break;
        } else {
            perror("producer fork");
            abort();
        }
    }
    
    //consumer process
    for (int i = 0; i < C; i++){
        pid = fork();
        
        if ( pid > 0 ) {        /* parent proc */
            cons_cpids[i] = pid;
        } else if ( pid == 0 ) { /* child proc */
            consumer_process(X);
            break;
        } else {
            perror("consumer fork");
            abort();
        }
    }
    
    if ( pid > 0 ) {            /* parent process */

        for (int  i = 0; i < P; i++) {
            waitpid(prod_cpids[i], &prod_state, 0);
            if (WIFEXITED(prod_state)) {
                printf("Child prod_cpid[%d]=%d terminated with state: %d.\n", i, prod_cpids[i], prod_state);
            }
        }
		
    	printf("exited all producers.");

        for (int i = 0; i < C; i++) {
            waitpid(cons_cpids[i], &cons_state, 0);
            if (WIFEXITED(prod_state)) {
                printf("Child cons_pid[%d]=%d terminated with state: %d.\n", i, prod_cpids[i], cons_state);
            }
        }

		printf("extied all consumers. ");
        // 3. Concatenate the horizontal strips in order
        //        //      to a file called all.png in curr dir
        //        printf("**********************MERGING IMAGES*************************\n");
        catpng_custom(B);
        
        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
        
        times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
        printf("paster2 execution time: %.6lf seconds\n", getpid(),  times[1] - times[0]);
    }
    
    //share memory clean up
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmid_final, IPC_RMID, NULL);
    shmctl(shmid_sems, IPC_RMID, NULL);
    shmctl(shmid_counter, IPC_RMID, NULL);
    
    //sem clean up
    if (sem_destroy(&sems[0]) || sem_destroy(&sems[1])) {
        perror("sem_destroy");
        abort();
    }

    return 0;
}

/**
 * @brief initilaize shared memory buffer
 * @param B unsigned int, specifies num of elements in total buffer ie. amount of RECV_BUF in available.
 */

int* shm_init(unsigned int B){
    static int result[2];
    int shmid;
    int shmid_final;

    //size of idividual slice buffers * number of elements
    int total_shm_size = sizeof_shm_stack(B);
    int final_buff_size = sizeof(RECV_BUF) * MAX_IMAGES;
    //shmid init
    // shmget takes in a key and allocates a block of memory with size total_shm_size
    shmid = shmget(IPC_PRIVATE, total_shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    shmid_final = shmget(IPC_PRIVATE, final_buff_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if ( shmid == -1 || shmid_final == -1) {
        perror("shmget");
        abort();
    }
    
    // Assign memory block to pointer so we can manipulate it
    p_shm_stack = shmat(shmid, NULL, 0);
    final_ordered_buffer = shmat(shmid_final, NULL, 0);
    //initialize stack of size B
    init_shm_stack(p_shm_stack, B);

    for (int i = 0; i < MAX_IMAGES; i++) {
        // init idividual slice buffers 
        shm_recv_buf_init(final_ordered_buffer + i);
    }
    result[0] = shmid;
    result[1] = shmid_final;
    return result;
}

void producer_pocess(unsigned int N, int prod_num){
    // Request server and fetch
    //      all 50 image segments.
    //      Place image in buffer with max size 'B'
    //      shared with consumer tasks.
    //      When capacity reached, i.e. B items in buffer
    //      producer stops!
    unsigned int server_number = 1;
    char* url = malloc (sizeof(char) * 55);
    round_robin (url, &server_number, N, prod_num);
    url[strlen(url)] = '\0';

    while (1) {
        printf("About to fetch from url: %s\n", url);
        RECV_BUF temp_buff = cURL_image(url);
        // Critical section which pushes the received data on
        //      the stack
		if(temp_buff.seq < 0){
			free(url);
			break;
		}

		printf("curled image into temp, seq: %d\n", temp_buff.seq);
        int sem0_value;
		sem_getvalue(&sems[0], &sem0_value);
		printf("WAITING FOR THERE TO BE SPACES IN THE BUFFER, sem[0] = %d\n", sem0_value);
		sem_wait(&sems[0]);

		printf("**************** IN PRODUCER **********************\n");
        if (push(p_shm_stack, temp_buff) == 0){
            printf("pushed to shm stack, seq: %d\n", p_shm_stack->items[p_shm_stack->pos].seq);
			printf("counter = %d\n", *p_counter);
            if (*p_counter < 49){
                (*p_counter)++;
                round_robin(url, &server_number, N, *p_counter);
                url[strlen(url)] = '\0';
            } else {
				sem_post(&sems[1]);
                free(url);
				printf("**************** OUT PRODUCER **********************\n");
                break;
            }
         } else {
             fprintf(stderr, "stack push failed, stack is full.\n");
             return;
         }
        printf("**************** OUT PRODUCER **********************\n");  
        sem_post(&sems[1]);
    }
    
    if ( shmdt(p_counter) || shmdt(p_shm_stack) || shmdt(final_ordered_buffer) || shmdt(sems)) {
        perror("shmdt");
        abort();
    }
    exit(0);
}

RECV_BUF cURL_image(char* url){

    printf("CURL: Getting the Image\n");
    RECV_BUF temp_buff;
    shm_recv_buf_init(&temp_buff);
    // copied from starter code
    CURL *curl_handle;
    CURLcode res;
        
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return temp_buff;
    }
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);

    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&temp_buff);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);

    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&temp_buff);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* get it! */
    res = curl_easy_perform(curl_handle);

    if( res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
        printf("%lu bytes received in memory %p, seq=%d.\n",  \
        temp_buff.size, temp_buff.buf, temp_buff.seq);
    }

    /* cleaning up */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    
    return temp_buff;
}

void consumer_process(unsigned int X){
    
    while (1){
        // Critical section which pops the received data in the buffer
        //      stack
		int sem1_value;
		sem_getvalue(&sems[1], &sem1_value);		
        printf("WAITING FOR THERE TO BE ITEMS IN THE BUFFER, sem[1] = %d\n", sem1_value);
        sem_wait(&sems[1]);
       
	    printf("******************** IN CONSUMER ****************************\n");
        // cosume the data
        RECV_BUF temp_buff;
        shm_recv_buf_init(&temp_buff);
        
        int pop_status = pop(p_shm_stack, &temp_buff);
        //Sleep for X milliseconds;
        usleep(X * 1000);
        // add it to proper place in memory
        unsigned int index = temp_buff.seq;

        //Prcoess the image data and store it in final buffer
        if (pop_status != 1) {
			printf("Poped off the stack: seq: %d\n", index);
            final_ordered_buffer[index] = temp_buff;
            unsigned int chunk_length = 0;
            char* chunk_type = malloc(5);
            chunk_type[4] = '\0'; // if you dont add this you get giberish at the end eg IDA ?pNg ?d

            unsigned int start_index = 0;

            // skip over the signature
            start_index+=8;
    
            // CHUNK_LENGTH
            memcpy(&chunk_length, final_ordered_buffer[index].buf + start_index, 4);
            chunk_length = ntohl(chunk_length);

            // CHUNK_TYPE
            start_index+=4;
            memcpy(chunk_type, final_ordered_buffer[index].buf + start_index, 4);
            
            printf("%s %d\n", chunk_type, chunk_length);
			if(index == 49){
				printf("!!!!!!!!!!!FINAL BUFFER IS FILLED!!!!!!!!!!!!!\n");
				sem_post(&sems[1]);
				break;
			}
        }else{
			printf("pop failed, terminating consumer.");
			sem_post(&sems[1]);
			break;
		}

		printf("******************** OUT CONSUMER ****************************\n");
        sem_post(&sems[0]);
    }
    
    if ( shmdt(p_counter) || shmdt(p_shm_stack) || shmdt(final_ordered_buffer) || shmdt(sems)) {
        perror("shmdt");
        abort();
    }
    
    exit(0);
}

void catpng_custom(){
    unsigned int argc = MAX_IMAGES;
    unsigned char* inflated_data[argc -1];
    int inflated_data_size[argc-1];

    unsigned int new_width = 0;
    unsigned int new_height = 0;
    
    U64 inflated_data_length = 0;
    U64 recent_inflated_data_length = 0;

    for (int i = 0; i < argc; i++) {
        unsigned int chunk_length = 0;
        char* chunk_type = malloc(5);
        chunk_type[4] = '\0'; // if you dont add this you get giberish at the end eg IDA ?pNg ?d

        unsigned int start_index = 0;

        // skip over the signature
        start_index+=8;
 
        // CHUNK_LENGTH
        memcpy(&chunk_length, final_ordered_buffer[i].buf + start_index, 4);
        chunk_length = ntohl(chunk_length);

        // CHUNK_TYPE
        start_index+=4;
        memcpy(chunk_type, final_ordered_buffer[i].buf + start_index, 4);
        
        printf("%s %d\n", chunk_type, chunk_length);

        //! IHDR SECTION
        // (4) Get dest length from width and length
        unsigned int width = 0;
        unsigned int height = 0;
        
        // (4) Get the width and height of image
        start_index+=4;
        memcpy(&width, final_ordered_buffer[i].buf + start_index, 4);
        
        start_index+=4;
        memcpy(&height, final_ordered_buffer[i].buf + start_index, 4);
        
        width = ntohl(width);
        height = ntohl(height);
        
        new_width = width;
        new_height += height;
        
        recent_inflated_data_length = height*(width*4 + 1);
        inflated_data_length =  new_height*(new_width*4 + 1);

        // skip over to the IDAT section
        start_index += 13;
        printf("%d x %d\n", new_width, new_height);

        //! IDAT SECTION
        // CHUNK_LENGTH
        memcpy(&chunk_length, final_ordered_buffer[i].buf + start_index, 4);
        start_index += 4;
        
        chunk_length = ntohl(chunk_length);

        // CHUNK_TYPE
        memcpy(chunk_type, final_ordered_buffer[i].buf + start_index, 4);
        start_index += 4;

        printf("%s %d\n", chunk_type, chunk_length);

        U8* data_buff = malloc(chunk_length);

        unsigned char* deflated_data_read = malloc(chunk_length);

        memcpy(deflated_data_read, final_ordered_buffer[i].buf + start_index, chunk_length);
        start_index += chunk_length;

        inflated_data[i - 1] = malloc(recent_inflated_data_length);
        inflated_data_size[i-1] = recent_inflated_data_length;
        
        U8* inflated_data_buff = malloc(recent_inflated_data_length);

        mem_inf(inflated_data[i - 1], &recent_inflated_data_length, deflated_data_read, chunk_length);

        free(deflated_data_read);
        free(inflated_data_buff);

        // skip over CRC
        start_index+=4;

        //! IEND SECTION
        // CHUNK_TYPE
        // skip over length field
        start_index+=4;
        memcpy(chunk_type, final_ordered_buffer[i].buf + start_index, 4);

        printf("%s\n", chunk_type);

        free(data_buff);
        free(chunk_type);
    }

    unsigned char* total_inflated = malloc(inflated_data_length);
    unsigned int running_pos = 0;

    for (int i = 0; i < argc - 1; i++) {
        memcpy(total_inflated + running_pos,inflated_data[i],inflated_data_size[i]);
        running_pos += inflated_data_size[i];
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

/*
    round_robin(char* url):
        Takes a URL and checks if the server has been used or not and
        Round Robins it to the next server, i.e. 1 -> 2 -> 3 -> 1...
        Storing the new url in the passed in arguments address
*/
void round_robin(char* url, unsigned int *server, unsigned int image_number, unsigned int seq){
    // loops through the url and increments the server part
    *server = (*server) % 3 + 1;
    sprintf(url,"%s%d%s%d%s%d","http://ece252-",*server, ".uwaterloo.ca:2530/image?img=", image_number, "&part=", seq);
}

