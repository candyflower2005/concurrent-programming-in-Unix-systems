#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "shared_variables.h"

#define URODZINY 16

typedef struct manager {
    int finished;
    sv* data;
}manager;

manager* ptr;

void finish() {
    if(munmap(ptr->data, sizeof(sv)) == -1)
        syserr("munmap");
    shm_unlink("sv");
    free(ptr);
}

manager* init(int n, int m) {
    manager* mgr = malloc(sizeof(manager));
    if(mgr == NULL) fatal("malloc");
    ptr = mgr;

    int fd_memory = -1;
    if((fd_memory = shm_open("sv", O_CREAT |O_RDWR, S_IRUSR | S_IWUSR))== -1)
        syserr("shm_open");

    if (ftruncate(fd_memory, sizeof(sv)) == -1)
        syserr("ftruncate");

    int prot = PROT_READ | PROT_WRITE, flags = MAP_SHARED;
    mgr->data = (sv*) mmap(NULL, sizeof(sv), prot, flags, fd_memory, 0);
    if(mgr->data == MAP_FAILED)
        syserr("mmap");

    mgr->data->n = n;
    mgr->data->m = m;
    for(int i = 1; i <= m; i++) {
        char t; int s;
        getchar();
        scanf("%c %d", &t, &s);
        mgr->data->room[i].type = t;
        mgr->data->room[i].capacity = s;
        mgr->data->room[i].free = true;
    }

    for(int i = 1; i <= n; i++) {
        mgr->data->prop[i].prev = -1;
        mgr->data->prop[i].next = -1;
    }

    if(sem_init(&mgr->data->sem_entrance1, 1, 0) == -1) syserr("sem_init");
    if(sem_init(&mgr->data->sem_entrance2, 1, 0) == -1) syserr("sem_init");
    if(sem_init(&mgr->data->sem_mgr, 1, 0) == -1) syserr("sem_init");
    if(sem_init(&mgr->data->sem_guard, 1, 1) == -1) syserr("sem_init");
    for(int i = 1; i <= n; i++) {
        if(sem_init(&mgr->data->sem_waiting[i], 1, 0) == -1) syserr("sem_init");
    }
    for(int i = 1; i <= m; i++) {
        if(sem_init(&mgr->data->room[i].sem_entrance, 1, 0) == -1) syserr("sem_init");
    }
    return mgr;
}

void work(int n, int m) {
    manager* mgr = init(n, m);

    char id[12];

    mgr->data->pids[0] = getpid();

    for(int i = 1; i <= n; i++) {
        switch(fork()) {
            case -1:
                syserr("fork");
            case 0:
                mgr->data->pids[i] = getpid();
                sprintf(id, "%d", i);
                if (execl("./player", "player", id, NULL) == -1)
                    syserr("execl");
                exit(0);
            default:
                continue;
        }
    }

    mgr->finished = 0;
    while(mgr->finished < mgr->data->n) {
        sem_wait(&mgr->data->sem_mgr);
        // dziedziczenie sekcji krytycznej od gracza,
        // który skończył właśnie grać

        printf("Player %d left after %d game(s)\n",
                mgr->data->fin_id, mgr->data->fin_played);

        mgr->finished++;

        sem_post(&mgr->data->sem_guard);
    }
}

int main() {

    atexit(finish);

    int n, m;
    scanf("%d%d", &n, &m);

    work(n, m);

    return 0;
}
