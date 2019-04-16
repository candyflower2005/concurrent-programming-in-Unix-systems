#ifndef SHARED_VARIABLES_H
#define SHARED_VARIABLES_H

#define N 1025
#define M 1025

#include <stdbool.h>
#include "err.h"
#include "proposition.h"
#include "room.h"
#include <semaphore.h>

typedef struct shared_variables {
    int n, m;
    sem_t sem_guard, sem_mgr;
    sem_t sem_waiting[N];
    sem_t sem_entrance1, sem_entrance2;

    proposition prop[N];
    int head, tail;

    char player_type[N];
    bool free_players[N];
    int free_types[26];
    int inside[N];

    int total[26];

    int players_with_prop;
    int initialized;


    bool add_next[N];
    bool busy[N];

    room room[M];

    int fin_id, fin_played;

    pid_t pids[N + 1];
}sv;

#endif //SHARED_VARIABLES_H
