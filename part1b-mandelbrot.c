/* Filename: part1b-mandelbrot.c
 * Student name: TAI Sze Yat
 * Student no.: 3035401306
 * Date: Oct 31, 2019
 * version: 1.0
 * Development platform: Course VM
 * Compilation: gcc part1b-mandelbrot.c -l SDL2 -lm
 */

#include <sys/times.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "Mandel.h"
#include "draw.h"
#define batch_size 40
int task[2];
int data[2];
int time_pipe[2];
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
float getns(struct timespec start, struct timespec end)
{
    return (end.tv_nsec - start.tv_nsec) / 1000000.0 +
           (end.tv_sec - start.tv_sec) * 1000.0;
}
pid_t *children_ids;
int *tasks_per_children;
void sigusr1_handler(int signum)
{
    if (signum == SIGUSR1)
    {
        struct timespec start_compute, end_compute;
        clock_gettime(CLOCK_MONOTONIC, &start_compute);
        printf("Child(%d): Start the computation ...\n", getpid());
        int *start_end = (int *)malloc(sizeof(int) * 2);
        read(task[0], start_end, sizeof(int) * 2);
        struct small_set *result;
        int size_of_result = sizeof *result + batch_size * sizeof *result->list;
        result = malloc(size_of_result);
        int j = 0;
        for (int i = start_end[0]; i < start_end[1]; ++i)
        {
            result->list[j] = Mandelbrot(i % IMAGE_WIDTH, i / IMAGE_WIDTH);
            j++;
        }
        result->cid = (int)getpid();
        result->low = start_end[0];
        result->high = start_end[1];
        write(data[1], result, size_of_result);
        clock_gettime(CLOCK_MONOTONIC, &end_compute);
        printf("Child(%d):... completed. Elapse time = %.3f ms\n", getpid(), getns(start_compute, end_compute));
    }
}
//This is the handler for SIGINT, it does the termination and the related display
void sigint_handler(int signum)
{
    if (signum == SIGINT)
    {
        printf("Process %d is interrupted by ^C. Bye Bye\n", (int)getpid());
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
            return;
        }
}
int main()
{

    struct tms t;
    clock_t dub;
    int tics_per_second;

    tics_per_second = sysconf(_SC_CLK_TCK);
    // use all the cores
    int number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);
    //set up task per children
    tasks_per_children = calloc(number_of_processors, sizeof(int));
    children_ids = calloc(number_of_processors, sizeof(int));
    pixels = (float *)malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);

    struct timespec start, end;
    struct sigaction sa;
    pipe(task);
    pipe(data);
    pipe(time_pipe);
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
    for (x = 0; x < number_of_processors; x++)
    {
        if (master_idx > IMAGE_WIDTH * IMAGE_HEIGHT)
        {
            all_done = true;
        }
        child_check = fork();
        children_ids[x] = child_check;
        if (child_check < 0)
        {
            printf("error");
        }
        else if (child_check == 0)
        {
            printf("Child(%d): Start up, Wait for task!\n", getpid());
            while (1)
                sleep(10);
        }
        else
        {
            if (!all_done)
            {

                int *start_end = (int *)malloc(sizeof(int) * 2);
                start_end[0] = master_idx;
                start_end[1] = master_idx + batch_size;
                write(task[1], start_end, 2 * sizeof(int));
                printf("Parent: idx[%d, %d]\n", start_end[0], start_end[1]);
                master_idx += batch_size;
                kill(child_check, SIGUSR1);
            }
        }
    }
    //while loop for the remaining task
    while (read_idx < IMAGE_WIDTH * IMAGE_HEIGHT)
    {
        struct small_set *temp;
        int size_of_temp = sizeof *temp + batch_size * sizeof *temp->list;
        temp = malloc(size_of_temp);
        if (read_idx < IMAGE_WIDTH * IMAGE_HEIGHT)
        {
            read(data[0], (void *)&temp[0], size_of_temp);
            read_idx += batch_size;
            for (int j = 0; j < batch_size; ++j)
            {
                pixels[temp->low + j] = temp->list[j];
            }
        }
        fill_tasks(temp->cid, number_of_processors);
        if (!all_done)
        {
            int start_end[2] = {master_idx, master_idx + batch_size};
            master_idx += batch_size;
            write(task[1], &start_end, 2 * sizeof(int));
            kill(temp->cid, SIGUSR1);
        }
    }
    //sending SIGINT to the workers
    for (int i = 0; i < number_of_processors; i++)
        kill(children_ids[i], SIGINT);
    //waiting for the workers to terminate
    for (i = 0; i < number_of_processors; i++)
    {
        waitpid(children_ids[i], NULL, WEXITED | WNOWAIT);
        waitpid(children_ids[i], NULL, 0);
    }
    for (i = 0; i < number_of_processors; i++)
    {
    }
    //displaying which worker did how many tasks
    int super_time = 0;
    int total_task = 0;
    for (i = 0; i < number_of_processors; i++)
    {
        printf("Child process %d terminated and completed %d tasks\n", children_ids[i], tasks_per_children[i]);
        total_task += tasks_per_children[i];
    }
    printf("\nAll Child processes have completed\n");
    printf("Total task done: %d\n", total_task);

    //word count display
    clock_gettime(CLOCK_REALTIME, &end);
    //displaying elapsed time
    struct rusage *parentusage = (struct rusage *)malloc(sizeof(struct rusage));
    struct rusage *childusage = (struct rusage *)malloc(sizeof(struct rusage));
    getrusage(RUSAGE_CHILDREN, childusage);
    getrusage(RUSAGE_SELF, parentusage);
    times(&t);
    printf("Total time spent by all child processes in user mode = %ld ms\n", childusage->ru_utime.tv_usec);
    printf("Total time spent by all child processes in system mode = %ld ms\n", childusage->ru_stime.tv_usec);
    printf("Total time spent by parent processes in user mode = %ld ms\n", parentusage->ru_utime.tv_usec);
    printf("Total time spent by parent processes in system mode = %ld ms\n", parentusage->ru_stime.tv_usec);

    float difftime = (end.tv_nsec - start.tv_nsec) / 1000000.0 + (end.tv_sec - start.tv_sec) * 1000.0;
    printf("Total elapse time measured by the process = %.3f ms\n", difftime);

    printf("Draw the image\n");
    DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 10000);
    return 0;
}
