#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>


typedef struct {
    unsigned int seconds;
    unsigned int nano_seconds;
} SysClock;

typedef struct {
    long mtype;
    char mtext[100];
} Message;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <termTimeS> <termTimeNano>\n", argv[0]);
        return 1;
    }

    unsigned int termTimeS = atoi(argv[1]);
    unsigned int termTimeNano = atoi(argv[2]);

    key_t key = 290161;
    int shmid;
    SysClock* shm_clock;
    key_t msg_key = 15436;
    int msgid;
    Message msg;

    if ((shmid = shmget(key, sizeof(SysClock), 0666)) < 0) {
                perror("shmget failed");
                exit(1);
        }

    if ((shm_clock = (SysClock*) shmat(shmid, NULL, 0)) == (SysClock *)-1) {
                perror("shmat failed");
                exit(1);
        }
    if ((msgid = msgget(msg_key, 0666)) == -1) {
                perror("Message queue failed to create");
                exit(1);
        }

    unsigned int lastSecondReported = 0;

     printf("WORKER PID:%d PPID:%d Called with oss: TermTimeS: %u TermTimeNano: %u\n--Received message\n",
       getpid(), getppid(), termTimeS, termTimeNano);

    while (1) {
        Message msg;
        if (msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0) == -1) {
                perror("msgrcv failed");
        }

        if (shm_clock->seconds != lastSecondReported) {
                printf("WORKER PID:%d PPID:%d SysClockS: %u SysclockNano: %u TermTimeS: %u TermTimeNano: %u\n",getpid(), getppid(), shm_clock->seconds, shm_clock->nano_seconds, termTimeS, termTimeNano);
                printf("--%u seconds have passed since starting\n", shm_clock->seconds - lastSecondReported);
                lastSecondReported = shm_clock->seconds;
                }


        if (shm_clock->seconds > termTimeS || (shm_clock->seconds == termTimeS && shm_clock->nano_seconds >= termTimeNano)) {
            // Time to terminate
            strcpy(msg.mtext, "0");
        } else {
            // Still working
            strcpy(msg.mtext, "1");
        }

    // Send status message to oss
        msg.mtype = 2; // Set to a common type that oss expects
        if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
            perror("msgsnd failed");
            return 1;
        }

    // Check if it's time to terminate
        if (strcmp(msg.mtext, "0") == 0) {
            break;
        }

        usleep(100000);
    }

    printf("WORKER PID:%d Terminating\n", getpid());

    shmdt(shm_clock);
    return 0;
}



