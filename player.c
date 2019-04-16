#include <stdlib.h>
#include <stdbool.h>
#include "shared_variables.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>


#define N 1025

typedef struct Player {
    int id;
    char pref_type;
    int games_played;
    FILE* in; FILE* out;
    sv* data;
}player;

player* ptr;

bool noPropLeft(player* p) {
    return feof(p->in);
}

player* init(int id) {
    player* p = malloc(sizeof(player));
    if(p == NULL) fatal("malloc");
    p->id = id;
    ptr = p;

    int fd_memory = -1;
    if((fd_memory = shm_open("sv", O_RDWR, S_IRUSR | S_IWUSR))== -1)
        syserr("shm_open");


    int prot = PROT_READ | PROT_WRITE, flags = MAP_SHARED;
    p->data = (sv*) mmap(NULL, sizeof(sv), prot, flags, fd_memory, 0);
    if(p->data == MAP_FAILED)
        syserr("mmap");

    close(fd_memory);

    char buf[16];
    sprintf(buf, "player-%d.in", id);
    if((p->in = fopen(buf, "r")) == NULL)
        fatal("fopen");
    sprintf(buf, "player-%d.out", id);
    if((p->out = fopen(buf, "a")) == NULL)
        fatal("fopen");

    char* line = NULL;
    size_t len = 0;
    if(getline(&line, &len, p->in) == -1)
        fatal("read");
    p->pref_type = line[0];

    sem_wait(&p->data->sem_guard);

    if(!noPropLeft(p))
        p->data->players_with_prop++;
    p->data->player_type[id] = p->pref_type;
    p->data->free_players[id] = true;
    p->data->free_types[p->pref_type - 'A']++;
    p->data->total[p->pref_type - 'A']++;
    p->data->add_next[id] = true;

    sem_post(&p->data->sem_guard);

    return p;
}

void finish() {
    if(munmap(ptr->data, sizeof(sv)) == -1)
        syserr("munmap");
	fclose(ptr->in);
	fclose(ptr->out);
    free(ptr);
}

void invalidGame(player* p, char* line) {
    line[strlen(line) - 1] = 0;
    fprintf(p->out, "Invalid game \"%s\"\n", line);
}

void clearProp(sv* data, int ind) {
    proposition* prop = &data->prop[ind];
    prop->needed = 0;
    for(int i = 1; i <= data->n; i++) {
        prop->players_id[i] = false;
        prop->selected[i] = false;
        prop->curr_players[i] = false;
    }
    for(int i = 0; i < 26; i++) {
        prop->type_needed[i] = 0;
    }
    prop->started = false;

    if(prop->prev != -1) data->prop[prop->prev].next = prop->next;
    if(prop->next != -1) data->prop[prop->next].prev = prop->prev;
    if(ind == data->head) data->head = prop->next;
    if(ind == data->tail) data->tail = prop->prev;
    prop->prev = -1;
    prop->next = -1;
}

int giveRoom(sv* data, proposition* prop) {
    int nr = -1; int mini = -1;
    for(int i = 1; i <= data->m; i++) {
        if(data->room[i].type == prop->room_type && data->room[i].free) {
            int s = data->room[i].capacity;
            if(prop->needed > s) continue;
            if(mini == -1 || mini > s) {
                nr = i;
                mini = s;
            }
        }
    }
    return nr;
}

bool addProposition(player* p) {
    char* line = NULL;
    size_t len = 0;
    if(getline(&line, &len, p->in) == -1) return false;
    char* s = line;
    sv* data = p->data;

    int ind;
    for(ind = 1; ind <= data->n; ind++) {
        if(data->prop[ind].needed == 0)
            break;
    }

    proposition* prop = &data->prop[ind];

    prop->prev = data->tail;
    data->prop[data->tail].next = ind;
    data->tail = ind;


    prop->room_type = line[0];
    line++;
    prop->players_id[p->id] = true;
    prop->type_needed[p->pref_type - 'A'] = 1;
    prop->needed = 1;
    prop->owner = p->id;

    while(*line != '\n') {
        if(*line == ' ') {
            line++;
            continue;
        }
        if('A' <= *line && *line <= 'Z') {
            prop->type_needed[*line - 'A']++;
            line++;
        }
        else {
            int nr = strtol(line, &line, 0);
            if(nr > data->n || prop->players_id[nr]) {
                // IDENTYFIKATOR JEST ZA DUŻY/JUŻ WYSTĄPIŁ
                invalidGame(p, s);
                clearProp(data, ind);
                return false;
            }
            prop->players_id[nr] = true;
            prop->type_needed[data->player_type[nr] - 'A']++;
        }
        prop->needed++;
    }

    // NIE ISTNIEJE POKÓJ DANEGO TYPU I ROZMIARU:
    bool ex = false;
    for(int i = 1; i <= data->m; i++) {
        if(prop->room_type == data->room[i].type &&
        prop->needed <= data->room[i].capacity) {
            ex = true;
            break;
        }
    }
    if(!ex) {
        invalidGame(p, s);
        clearProp(data, ind);
        return false;
    }

    for(int i = 0; i < 26; i++) {
        if(prop->type_needed[i] > data->total[i]) {
            invalidGame(p, s);
            clearProp(data, ind);
            return false;
        }
    }

    return true;
}

bool enoughPlayers(sv* data, proposition* prop) {
    for(int i = 1; i <= data->n; i++) {
        if(prop->players_id[i] && !data->free_players[i]) {
            return false;
        }
    }
    for(int i = 0; i < 26; i++) {
        if(prop->type_needed[i] > data->free_types[i]) {
            return false;
        }
    }
    return true;
}

void findPlayers(sv* data, int ind, int room) {
    proposition* prop = &data->prop[ind];

    for(int i = 1; i <= data->n; i++) {
        char t = data->player_type[i];
        if(prop->players_id[i]) {
            data->free_players[i] = false;
            data->free_types[t - 'A']--;
            prop->selected[i] = true;
            data->inside[i] = room;
            prop->type_needed[t - 'A']--;
        }
    }

    for(int i = 1; i <= data->n; i++) {
        char t = data->player_type[i];
        if(prop->type_needed[t - 'A'] > 0 && data->free_players[i]) {
            data->free_players[i] = false;
            data->free_types[t - 'A']--;
            prop->selected[i] = true;
            data->inside[i] =  room;
            prop->type_needed[t - 'A']--;
        }
    }
}

bool noPropForPlayer(sv* data, char type) {
    bool ex = true;
    for(int i = 1; i <= data->n; i++) {
        if(data->prop[i].type_needed[type - 'A'] > 0){
            ex = false;
            break;
        }
    }
    return ex;
}

void prepareGame(player* p, int ind, int room) {
    proposition* prop = &p->data->prop[ind];
    fprintf(p->out, "Game defined by %d is going to start: room %d, players (", prop->owner, room);
    bool first = true;
    for(int i = 1; i <= p->data->n; i++) {
        if(prop->selected[i]) {
            if(first) {
                first = false;
                fprintf(p->out, "%d", i);
            }
            else fprintf(p->out, ", %d", i);
        }
    }
    fprintf(p->out, ")\n");
}

void startGame(player* p, int room, proposition* prop) {
    fprintf(p->out, "Entered room %d, game defined by %d, waiting for players (", room, prop->owner);
    bool first = true;
    for(int i = 1; i <= p->data->n; i++) {
        if(prop->selected[i] && !prop->curr_players[i]) {
            if(first) {
                first = false;
                fprintf(p->out, "%d", i);
            }
            else fprintf(p->out, ", %d", i);
        }
    }
    fprintf(p->out, ")\n");
}

void leaveRoom(player* p, int room) {
    fprintf(p->out, "Left room %d\n", room);
}



void play(int id) {
    player* p = init(id);
    sv* data = p->data;
    int n = data->n;

    sem_wait(&data->sem_guard);

    data->initialized++;
    if(data->initialized == n){
        for(int i = 0; i < n; i++) {
            sem_post(&data->sem_entrance1);
        }
        data->initialized = 0;
    }

    sem_post(&data->sem_guard);

    // czekam, aż wszyscy zainicjalizują się
    sem_wait(&data->sem_entrance1);

    sem_wait(&data->sem_guard);

    if(!noPropLeft(p)) {
        while(!addProposition(p) && !noPropLeft(p));
        data->add_next[id] = false;
        if(noPropLeft(p)) data->players_with_prop--;
    }

    data->initialized++;

    if(data->initialized == n){
        for(int i = 0; i < n; i++) {
            sem_post(&data->sem_entrance2);
        }
    }

    sem_post(&data->sem_guard);

    // czekam, aż wszyscy dodadzą swoją propozycję
    sem_wait(&data->sem_entrance2);


    /****************************************************/
    /****************************************************/
    /****************************************************/

    while(true) {
        sem_wait(&p->data->sem_guard);
        if(data->busy[id]) {
            data->free_players[id] = true;
            data->free_types[data->player_type[id] - 'A']++;
            data->busy[id] = false;
            int val;
            sem_getvalue(&data->sem_waiting[id], &val);
            if(val == 1) sem_wait(&data->sem_waiting[id]);
        }

        if(!noPropLeft(p) && data->add_next[id]){
            while(!addProposition(p) && !noPropLeft(p));
            data->add_next[id] = false;
            if(noPropLeft(p))
                data->players_with_prop--;
        }

         if(noPropForPlayer(data, p->pref_type) && data->players_with_prop == 0) {
            data->fin_id = id;
            data->fin_played = p->games_played;

            for(int i = 1; i <= n; i++) {
                if(data->inside[i] != 0) continue;
                char type = data->player_type[i];
                if(noPropLeft(p) && noPropForPlayer(data, type)) {
                    // gracz nr i może odejść
                    sem_post(&data->sem_waiting[i]);
                }
            }

            sem_post(&data->sem_mgr);

            return;
        }

        if(data->free_players[id]) { // nie zostałem wzięty w międzyczasie
            for (int i = 1; i <= n; i++) {
                proposition *prop = &data->prop[i];

                if (prop->needed == 0) continue;
                if (prop->started) continue;

                int nr = giveRoom(data, prop);
                if (nr == -1) continue;
                if (!enoughPlayers(data, prop)) continue;

                prop->started = true;
                data->room[nr].free = false;
                data->room[nr].prop_id = i;

                findPlayers(data, i, nr);

                prepareGame(p, i, nr);

                // budzi pierwszego gracza
                for (int j = 1; j <= n; j++) {
                    if (prop->selected[j]) {
                        sem_post(&data->sem_waiting[j]);
                        break;
                    }
                }

                break;
            }
        }

        sem_post(&data->sem_guard);

        sem_wait(&data->sem_waiting[id]);
        sem_wait(&data->sem_guard);

        int ind = data->inside[id];

        if((data->add_next[id] && data->busy[id]) || ind == 0) {
            sem_post(&data->sem_guard);
            continue;
        }

        proposition* prop = &data->prop[data->room[ind].prop_id];
        prop->players_inside++;
        prop->curr_players[id] = true;
        p->games_played++;

        startGame(p, ind, prop);

        bool awake = false;
        // każdy gracz budzi kolejnego w kolejności
        for(int i = id + 1; i <= n; i++) {
            if(prop->selected[i]) {
                awake = true;
                sem_post(&data->sem_waiting[i]);
                break;
            }
        }


        if(!awake) {
            // byłem ostatnim wchodzącym
            for(int i = 0; i < prop->needed; i++) {
                sem_post(&data->room[ind].sem_entrance);
            }
        }

        sem_post(&data->sem_guard);

        // czekam, aż wszyscy wejdą
        sem_wait(&data->room[ind].sem_entrance);
        sem_wait(&data->sem_guard);

        data->free_players[id] = true;
        data->free_types[p->pref_type - 'A']++;
        prop->players_inside--;

        data->inside[id] = 0;
        if(prop->players_inside == 0) {
            int prop_id = data->room[ind].prop_id;
            int owner = data->prop[prop_id].owner;
            data->room[ind].free = true;
            data->room[ind].prop_id = 0;
            data->add_next[owner] = true;
            if(data->inside[owner] == 0 && data->free_players[owner]) {
                data->busy[owner] = true;
                data->free_players[owner] = false;
                data->free_types[data->player_type[owner] - 'A']--;
                sem_post(&data->sem_waiting[owner]);
            }
            clearProp(data, prop_id);
        }

        leaveRoom(p, ind);

        if(data->players_with_prop == 0) {
            for(int i = 1; i <= n; i++) {
                if(data->inside[i] != 0) continue;
                char type = data->player_type[i];
                if(noPropForPlayer(data, type)) {
                    // gracz nr i może odejść
                    sem_post(&data->sem_waiting[i]);
                }
            }
        }

        sem_post(&data->sem_guard);

    }

}



int main(int argc, char* argv[]) {

    atexit(finish);

    int id = atoi(argv[1]);

    play(id);

    return 0;
}
