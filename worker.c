#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>


typedef struct {
    unsigned int seconds;
    unsigned int nano_seconds;
} SysClock;

typedef struct {
    long mtype;
    char mtext[100];
} Message;

int main() {

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
                exit(EXIT_FAILURE);
        }


    if (msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0) == -1) {
        perror("msgrcv failed");
        exit(EXIT_FAILURE);
    }
    printf("Received from OSS: %s\n", msg.mtext);
    
    msg.mtype = 2; 
    strcpy(msg.mtext, "Acknowledgement from Worker");
    if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("msgsnd failed");
        exit(EXIT_FAILURE);
    }

    printf("Worker: Current Clock Time = %u seconds, %u nanoseconds\n", shm_clock->seconds, shm_clock->nano_seconds);

    
    shmdt(shm_clock);
    return 0;
}

