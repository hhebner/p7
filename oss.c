#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/wait.h>

typedef struct PCB {
        int occupied;
        pid_t pid;
        int start_seconds;
        int start_nano;
} ProcessControlBlock;

ProcessControlBlock pcb[20];

typedef struct {
        long mtype;
        char mtext[100];
} Message;

typedef struct {
        unsigned int seconds;
        unsigned int nano_seconds;
} SysClock;

void print_PCB(int *seconds_ptr, int *nano_ptr) {
    printf("OSS PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), *seconds_ptr, *nano_ptr);
    printf("Process Table:\nEntry Occupied PID StartS StartN\n");
    for (int i = 0; i < 20; i++) {
        printf("%d %d %d %d %d\n", i, pcb[i].occupied, pcb[i].pid, pcb[i].start_seconds, pcb[i].start_nano);
    }
}

void update_PCB(pid_t pid, int *seconds_ptr, int *nano_ptr) {
    for (int i = 0; i < 20; i++) {
        if (!pcb[i].occupied) {
            pcb[i].occupied = 1;
            pcb[i].pid = pid;
            pcb[i].start_seconds = *seconds_ptr;
            pcb[i].start_nano = *nano_ptr;
            break;
        }
    }
}
void incrementClock(SysClock* clock, unsigned int seconds, unsigned int nano_seconds) {
        clock->nano_seconds += nano_seconds;

        while (clock->nano_seconds >= 1000000000) {
                clock->seconds += 1;
                clock->nano_seconds -= 1000000000;
        }
        clock->seconds += seconds;
}

int allocatePCB() {
        for (int i = 0; i < 20; i++) {
                if (pcb[i].occupied == 0) {
                        pcb[i].occupied = 1;
                        return i;
                }
        }
        return -1;
}

void printSysClock(SysClock *clock) {
    printf("Clock time: %u seconds and %u nanoseconds\n", clock->seconds, clock->nano_seconds);
}

void cleanup(int shmid, SysClock *shm_clock, int msgid) {
    if (shm_clock != NULL) {
        shmdt(shm_clock);
    }

    if (shmid != -1) {
        shmctl(shmid, IPC_RMID, NULL);
    }

    if (msgid != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }
}
int main(int argc, char* argv[]) {
        int opt;
        int n = 1;
        int s = 1;
        int t = 1000000;
        char *f = "logfile.txt";

        // Parse command-line arguments
        while ((opt = getopt(argc, argv, "hn:s:t:f:")) != -1) {
                switch (opt) {
                   case 'h':
                        printf("Usage: %s [-h] [-n proc] [-s simul] [-t timeLimit] [-f logfile]\n", argv[0]);
                        exit(EXIT_SUCCESS);
                        break;
                   case 'n':
                        n = atoi(optarg);
                        break;
                   case 's':
                        s = atoi(optarg);
                        break;
                   case 't':
                        t = atoi(optarg);
                        break;
                   case 'f':
                        f = optarg;
                        break;
                   default:
                        fprintf(stderr, "Usage: %s [-h] [-n proc] [-s simul] [-t timeToLaunchNewChild] [-f logfile]\n", argv[0]);
                        exit(EXIT_FAILURE);
                        break;
                }
        }


        printf("-n value = %d\n", n);
        printf("-s value = %d\n", s);
        printf("-t value = %d\n", t);
        printf("-f value = %s\n", f);

        key_t key = 290161;
        int shmid;
        SysClock* shm_clock;
        int msgid;
        Message msg;
if ((shmid = shmget(key, sizeof(SysClock), IPC_CREAT | 0666)) < 0) {
                perror("shmget failed");
                exit(1);
        }

        if ((shm_clock = (SysClock*) shmat(shmid, NULL, 0)) == (SysClock *)-1) {
                perror("shmat failed");
                exit(1);
        }

        if ((msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT)) == -1) {
                perror("Message queue failed to create");
                exit(EXIT_FAILURE);
        }

        shm_clock->seconds = 5;
        shm_clock->nano_seconds = 2000000;

        pid_t pid;
        int total_workers_launched = 0;
        int active_workers = 0;

        pid = fork();
        if (pid == 0) {
                execlp("./worker", "worker", NULL);
                perror("execlp failed");
                exit(EXIT_FAILURE);
        } else if (pid < 0) {

                perror("fork failed");
                exit(EXIT_FAILURE);
    }
        wait(NULL);


        cleanup(shmid, shm_clock, msgid);

        return 0;
}
