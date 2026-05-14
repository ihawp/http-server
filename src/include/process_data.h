#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "hash_table.h"

struct process_data {
    int epc, sfd;
    pid_t pid;
    ht *user_states; // hash table to store user states 
    pthread_mutex_t lock;
    pthread_cond_t ready;
};