//
// Created by ubuntu on 16.01.19.
//

#ifndef PROJECT_ROOM_H
#define PROJECT_ROOM_H

#include <stdbool.h>
#include <semaphore.h>

#define M 1025

typedef struct Room {
    int capacity;
    char type;
    bool free;
    sem_t sem_entrance;
    int prop_id;
}room;

#endif //PROJECT_ROOM_H
