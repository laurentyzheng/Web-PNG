# Web PNG Verify and Concantenate

## Multi-Threaded
This program cURLs all valid PNG images in a website that are in fragments, Creating a new image through concantenating algorithms that multipulates the IDAT PNG data. 

- Specifying `-t -i` in the execution changes the number of threads used in the curling processes and the max image to be downloaded from the website respectively.


## Multi-Processed
This program uses a producer and consumer skema in a multi-processed manner to retreive and process the images. The producers and consumers has access to a share memory stack of the images uses semaphores to allocate tasks accordingly.

- Specifying `-B -P -C -X` in the execution changes the buffer size, number of producers, number of consumers, and the sleep time between each exchange, respectively.

## Multi-cURLed
This program scrapes a website of all PNG, with curl_multi(), and logs all the valid and non-corrupted images as the output. By using a frontier that adds to curl_multi(), the program eliminates all visited urls with a hashmap.

- Specifying the first argument as the seed url and `-f` for the name of the log file.
