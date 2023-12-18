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
#include <signal.h>

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
int timeout = 0;

void handler(int sig){
    if (sig == SIGALRM){
        printf("Terminating becasue 1 minute has passed\n");
        timeout = 1;
    } else if (sig == SIGINT) {
        printf("Terminating becasue ctrl-c was pressed\n");
        timeout = 1;
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
        key_t key = 290161;
        int shmid;
        SysClock* shm_clock;
        key_t msg_key = 15436;
        int msgid;

        if ((shmid = shmget(key, sizeof(SysClock), IPC_CREAT | 0666)) < 0) {
                perror("shmget failed");
                exit(1);
        }

        if ((shm_clock = (SysClock*) shmat(shmid, NULL, 0)) == (SysClock *)-1) {
                perror("shmat failed");
                exit(1);
        }

        if ((msgid = msgget(msg_key, 0666 | IPC_CREAT)) == -1) {
                perror("Message queue failed to create");
                exit(1);
        }

        signal(SIGALRM, handler);
        signal(SIGINT, handler);
        alarm(60);

        shm_clock->seconds = 0;
        shm_clock->nano_seconds = 0;

        FILE *log_file = fopen(f, "w");
        if (log_file == NULL) {
                perror("Could not open file");
                exit(1);
        }

        int total_workers_launched = 0;
        int active_workers = 0;

        unsigned int last_half_second = 0;
        while(active_workers > 0 || total_workers_launched < n) {
             if (active_workers < s && total_workers_launched < n) {
                unsigned int termTimeS = rand() % t + 1;
                unsigned int termTimeNano = rand() % 1000000000;

                char termTimeSStr[10], termTimeNanoStr[10];
                sprintf(termTimeSStr, "%u", termTimeS);
                sprintf(termTimeNanoStr, "%u", termTimeNano);

                pid_t pid = fork();
                if (pid == 0) {
           
                    execlp("./worker", "worker", termTimeSStr, termTimeNanoStr, (char *) NULL);
                    perror("execlp failed");
                    exit(1);
                } else if (pid > 0) {
            
                    update_PCB(pid, &shm_clock->seconds, &shm_clock->nano_seconds);
                    active_workers++;
                    total_workers_launched++;
                } else {
                    perror("fork failed");
                    exit(1);
                }
            }
            if (timeout){
                for (int i = 0; i < 20; i++){
                    if (pcb[i].occupied){
                        kill(pcb[i].pid, SIGTERM);
                    }
                }
                cleanup(shmid, shm_clock, msgid);
            }

            incrementClock(shm_clock, 0, 50000000);

            if ((shm_clock->nano_seconds - last_half_second >= 500000000) || (shm_clock->nano_seconds < last_half_second)) {

                fprintf(log_file, "OSS PID:%d SysClockS: %u SysclockNano: %u\nProcess Table:\n", getpid(), shm_clock->seconds, shm_clock->nano_seconds);

                printf("OSS PID:%d SysClockS: %u SysclockNano: %u\nProcess Table:\n", getpid(), shm_clock->seconds, shm_clock->nano_seconds);

            for (int i = 0; i < 20; i++) {
                fprintf(log_file, "Entry %d Occupied %d PID %d StartS %d StartN %d\n", i, pcb[i].occupied, pcb[i].pid, pcb[i].start_seconds, pcb[i].start_nano);

                printf("Entry %d Occupied %d PID %d StartS %d StartN %d\n", i, pcb[i].occupied, pcb[i].pid, pcb[i].start_seconds, pcb[i].start_nano);
                }
                last_half_second = shm_clock->nano_seconds;
        }
            for (int i = 0; i < n; i++) {
                    if (pcb[i].occupied) {
                        Message msg;
                        msg.mtype = 1;
                        strcpy(msg.mtext, "Your message here");

                        if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
                                perror("msgsnd failed");
                        } else {
                                fprintf(log_file, "OSS: Sending message to worker %d PID %d at time %u:%u\n", i, pcb[i].pid, shm_clock->seconds, shm_clock->nano_seconds);
                                printf("OSS: Sending message to worker %d PID %d at time %u:%u\n", i, pcb[i].pid, shm_clock->seconds, shm_clock->nano_seconds);
        }

                        if (msgrcv(msgid, &msg, sizeof(msg.mtext), 2, IPC_NOWAIT) != -1) {
                                fprintf(log_file, "OSS: Receiving message from worker %d PID %d at time %u:%u\n", i, pcb[i].pid, shm_clock->seconds, shm_clock->nano_seconds);
                                printf("OSS: Receiving message from worker %d PID %d at time %u:%u\n", i, pcb[i].pid, shm_clock->seconds, shm_clock->nano_seconds);

                        if (strcmp(msg.mtext, "0") == 0) {
                                pcb[i].ready_terminate = 1;
                                }
                        }
                        usleep(100000);
                }
        }
            int status;
            pid_t child_pid;
            while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {

                printf("Worker with PID %d terminated\n", child_pid);

                for (int i = 0; i < n; i++) {
                        if (pcb[i].pid == child_pid) {
                      
                                if (pcb[i].ready_terminate = 1) {
                                        pcb[i].occupied = 0;
                                        pcb[i].ready_terminate = 0; 
                                        active_workers--;
                                        break; 
            }
        }
    }
  }

            usleep(100000);
        }


        fclose(log_file);

        cleanup(shmid, shm_clock, msgid);

        return 0;
}




