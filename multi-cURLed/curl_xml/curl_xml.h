#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define URL_SIZE 256

typedef struct recv_buf2 {
    char buf[BUF_SIZE];       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/ /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header  <0 indicates an invali number*/
    char original_url[URL_SIZE];
} RECV_BUF;

typedef struct front {
    char frontier[10240][256];
    unsigned int size;
} FRONTIER;

typedef struct png_urls{
    char png_urls[10240][256];
    unsigned int size;
} PNG_URLS;

struct thread_args{
    char * url;
    int index;
};

htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url, FRONTIER* frontier);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, char * url);
int recv_buf_cleanup(RECV_BUF *ptr);
void cleanup(CURL *curl, RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
CURL *easy_handle_init(RECV_BUF *ptr, const char *url);
int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf, FRONTIER* frontier);
int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf);
int process_curl_msg (CURLMsg * message, FRONTIER * frontier, PNG_URLS * png_urls, const int max_png);
int valid_png(RECV_BUF recv_buf);
void png_add(char* url, PNG_URLS * png_urls);
