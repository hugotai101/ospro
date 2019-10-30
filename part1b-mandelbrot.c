/* Filename: part1b-mandelbrot.c
 * Student name: TAI Sze Yat
 * Student no.: 3035401306
 * Date: Oct 31, 2019
 * version: 1.0
 * Development platform: Course VM
 * Compilation: gcc part1b-mandelbrot.c -l SDL2 -lm
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "Mandel.h"
#include "draw.h"
//align to 8-byte boundary
int task[2];
int data[2];
int i;
struct small_set
{
    int cid;
    int low;
    int high;
    float list[];
};
//store the 2D image as a linear array of pixels (in row-major format)
float *pixels;

//pre: Given the start and end time
//post: return the elapsed time in nanoseconds (stored in 64 bits)
unsigned long long getns(struct timespec start, struct timespec end)
{
    return (unsigned long long)(end.tv_sec - start.tv_sec) * 1000000000ULL + (end.tv_nsec - start.tv_nsec);
}
pid_t children_ids[9];
int tasks_per_children[9];
//this is the handler for SIGUSR1. Here the function search(word) is called after reading the word from the task pipe
//then it writes the answer is put in the modified output type structure along with other useful information like process id and written to the data pipe. It also does the related displays
void sigusr1_handler(int signum)
{
    if (signum == SIGUSR1)
    {
        int *start_end = (int *) malloc(sizeof(int) * 2);
        read(task[0], start_end, sizeof(int) * 2);
        struct small_set* result;
        int size_of_result = sizeof *result + 40 * sizeof *result->list;
        result = malloc(size_of_result);
        int j = 0;
        for (int i = start_end[0]; i < start_end[1]; ++i) {
            result->list[j] = Mandelbrot(i % IMAGE_WIDTH, i / IMAGE_WIDTH);
            j++;
        }
        result->cid = (int) getpid();
        result->low = start_end[0];
        result->high = start_end[1];
        write(data[1], result, size_of_result);
    }
}
//This is the handler for SIGINT, it does the termination and the related display
void sigint_handler(int signum)
{
    if (signum == SIGINT)
    {
        printf("Process %d is interrupted by ^C. Finish all task. Bye Bye\n", (int)getpid());
        exit(0);
    }
}
//a function to fill an array with the number of tasks each process did
void fill_tasks(int pid, int size)
{
    for (int i = 0; i < size; i++)
        if (children_ids[i] == pid)
        {
            tasks_per_children[i]++;
            break;
        }
}
int main(int argc, char *argv[])
{
    pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
    struct timespec start, end;
    int children = 8;
    struct sigaction sa;
    pipe(task);
    pipe(data);
    clock_gettime(CLOCK_REALTIME, &start);
    //========installing the above made handlers========
    sigaction(SIGUSR1, NULL, &sa);
    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT, NULL, &sa);
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
    //================================================
    pid_t child_check;
    bool all_done = false;
    int master_idx = 0;
    int read_idx = 0;
    int x = 0;
    //making the desired number of children and assigning an initial word to each
    for (x = 0; x < children; x++)
    {
        if (master_idx > IMAGE_WIDTH * IMAGE_HEIGHT)
        {
            all_done = true;
        }
        child_check = fork();
        children_ids[x] = child_check;
        //write(task[1], &word, MAXLEN*sizeof(char));
        if (child_check < 0)
        {
            printf("error");
        }
        else if (child_check == 0)
        {
            while(1)
                sleep(10);
            exit(0);
        }
        else
        {
            if (!all_done)
            {

                int *start_end = (int *) malloc (sizeof(int) * 2);
                start_end[0] = master_idx;
                start_end[1] = master_idx + 40;
                int abc = write(task[1], start_end, 2 * sizeof(int));
                printf("[%d, ]Parent[148]: idx[%d, %d]\n", abc, start_end[0], start_end[1]);
                master_idx += 40;
                kill(child_check, SIGUSR1);
                printf("Sent %d\n", child_check);
            }
        }
    }
    //a while loop to handle further distribution of the tasks to the child processes/workers and collect results as they are produced
//    while (all_results[line - 1].cid < 0)
    while (read_idx < IMAGE_WIDTH * IMAGE_HEIGHT)
    {
        struct small_set* temp;
        int size_of_temp = sizeof *temp + 40 * sizeof *temp->list;
        temp = malloc(size_of_temp);
        //printf("Going to read: \n");
        if (read_idx < IMAGE_WIDTH * IMAGE_HEIGHT)
        {
            read(data[0], (void *)&temp[0], size_of_temp);
            read_idx+=40;
            for (int j = 0; j < 40; ++j) {
                pixels[temp->low + j] = temp->list[j];
            }
        }
        fill_tasks(temp->cid, children);
        if (!all_done)
        {
            int start_end[2] = {master_idx, master_idx + 40};
            master_idx += 40;
            write(task[1], &start_end, 2 * sizeof(int));
            kill(temp->cid, SIGUSR1);
        }
    }
    //sending SIGINT to the workers
    for (int i = 0; i < children; i++)
        kill(children_ids[i], SIGINT);
    //waiting for the workers to terminate
    for (i = 0; i < children; i++)
        waitpid(children_ids[i], NULL, 0);
    //displaying which worker did how many tasks
    for (i = 0; i < children; i++)
        printf("Child process %d terminated and completed %d tasks\n", children_ids[i], tasks_per_children[i]);
    //word count display
    clock_gettime(CLOCK_REALTIME, &end);
    //displaying elapsed time
    printf("\nTotal elapsed time: %.2lf ms\n", getns(start, end) / 1000000.0);

    DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 10000);
    return 0;
}
