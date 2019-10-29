/* Filename: part1a-mandelbrot.c
 * Student name: TAI Sze Yat
 * Student no.: 3035401306
 * Date: Oct 31, 2019
 * version: 1.0
 * Development platform: Course VM
 * Compilation: gcc part1a-mandelbrot.c –l SDL2 -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "Mandel.h"
#include "draw.h"

int main(int argc, char *args[]) {
    //data structure to store the start and end times of the whole program
    struct timespec start_time, end_time;
    //get the start time
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    //data structure to store the start and end times of the computation
    struct timespec start_compute, end_compute;
    clock_t start, end;

    //generate mandelbrot image and store each pixel for later display
    //each pixel is represented as a value in the range of [0,1]

    //store the 2D image as a linear array of pixels (in row-major format)
    float *pixels;

    //allocate memory to store the pixels
    pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
    if (pixels == NULL) {
        printf("Out of memory!!\n");
        exit(1);
    }
    //get number of processors
    int number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);
    int rowsize = sizeof(float) * (IMAGE_WIDTH + 1);
    //compute the mandelbrot image
    //keep track of the execution time - we are going to parallelize this part
    printf("Start the computation ...\n");
    clock_gettime(CLOCK_MONOTONIC, &start_compute);
    start = clock();
    int tube1[2];
    if (pipe(tube1) == -1) {
        perror("pipe");
        exit(1);
    }
    int x, y;
    float difftime;
    int pid;
    for (int i = 0; i < number_of_processors; ++i) {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            clock_gettime(CLOCK_MONOTONIC, &start_compute);
            printf("Child(%d): Start the computation ...\n", getpid());
            close(tube1[0]);
            for (int j = i; j < IMAGE_HEIGHT; j += number_of_processors) {
                float *temp = (float *) malloc(rowsize);
                for (int k = 0; k < IMAGE_WIDTH; ++k) {
                    temp[k + 1] = Mandelbrot(k, j);
                }
                temp[0] = j;
                write(tube1[1], temp, rowsize);
            }
            clock_gettime(CLOCK_MONOTONIC, &end_compute);
            difftime = (end_compute.tv_nsec - start_compute.tv_nsec) / 1000000.0 +
                       (end_compute.tv_sec - start_compute.tv_sec) * 1000.0;
            printf("Child(%d):... completed. Elapse time = %.3f ms\n", getpid(), difftime);
            exit(0);
        }
    }
    close(tube1[1]);
    for (int i = 0; i < IMAGE_HEIGHT; i++) {
        float fdx;
        read(tube1[0], &fdx, sizeof(float));
        int idx = fdx;
        read(tube1[0], &pixels[idx * IMAGE_HEIGHT], sizeof(float) * IMAGE_WIDTH);
    }
    for (int j = 0; j < number_of_processors; j++) {
        wait(0);
    }
    printf("All Child processes have completed\n");
    clock_gettime(CLOCK_MONOTONIC, &end_compute);
    difftime = (end_compute.tv_nsec - start_compute.tv_nsec) / 1000000.0 +
               (end_compute.tv_sec - start_compute.tv_sec) * 1000.0;
    printf(" ... completed. Elapse time = %.3f ms\n", difftime);

    //Report timing
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    difftime = (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0 + (end_time.tv_sec - start_time.tv_sec) * 1000.0;
    end = clock();
    float cpu_time_parent = 1000.0 * (end - start) / CLOCKS_PER_SEC;
    printf("Total elapse time measured by parent process = %.3f ms\n", difftime);
    printf("Total elapse cpu time measured by parent process = %.3f ms\n", cpu_time_parent);

    printf("Draw the image\n");
    //Draw the image by using the SDL2 library
    DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 10000);
    return 0;
}
