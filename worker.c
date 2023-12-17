#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

typedef struct {
    unsigned int seconds;
    unsigned int nano_seconds;
} SysClock;

int main() {

    key_t key = 290161;
    int shmid;
    SysClock* shm_clock;

    if ((shmid = shmget(key, sizeof(SysClock), 0666)) < 0) {
                perror("shmget failed");
                exit(1);
        }

    if ((shm_clock = (SysClock*) shmat(shmid, NULL, 0)) == (SysClock *)-1) {
                perror("shmat failed");
                exit(1);
        }


    printf("Worker: Current Clock Time = %u seconds, %u nanoseconds\n", shm_clock->seconds, shm_clock->nano_seconds);

    shmdt(shm_clock);
    return 0;
}
