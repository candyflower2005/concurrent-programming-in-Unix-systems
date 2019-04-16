#ifndef PROPOSITION_H
#define PROPOSITION_H

#include <stdbool.h>

#define N 1025
#define M 1025

typedef struct Proposition {
    char room_type;
    int needed, owner;
    int type_needed[26];
    bool players_id[N];

    bool curr_players[N];
    bool selected[N];
    bool started;
    int players_inside;

    int prev, next;
}proposition;


#endif //PROPOSITION_H
