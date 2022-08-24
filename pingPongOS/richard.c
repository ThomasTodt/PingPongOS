#include <stdio.h>
#include <stdlib.h>
#include "ppos.h"
#include <unistd.h>

int buffer[100];
unsigned int size = 0;

task_t produtorTask1;
task_t produtorTask2;
task_t produtorTask3;
task_t consumidorTask1;
task_t consumidorTask2;

semaphore_t s_vaga;
semaphore_t s_buffer;
semaphore_t s_item;

int item;

void produtor() {
   while (1) {
    task_sleep (1000);
    item = (rand() % 100 + 1);

    sem_down(&s_vaga);

    sem_down(&s_buffer);
    buffer[size] = item;
    size += 1;
    sem_up(&s_buffer);

    printf("PRODUZIU: %d\n", item);
    sem_up(&s_item);

   }
}

void consumidor() {
   task_sleep(1000);
   while (1) {
    sem_down(&s_item);

    sem_down(&s_buffer);
    size -= 1;
    item = buffer[size];
    printf("        CONSUMIU: %d\n", item);
    sem_up(&s_buffer);

    sem_up(&s_vaga);

    task_sleep(1000);
   }
}

int main() {
    printf ("main: inicio\n") ;

    ppos_init () ;

    sem_create(&s_vaga, 5);
    sem_create(&s_buffer, 1);
    sem_create(&s_item, 0);

    task_create(&produtorTask1, produtor, "p1");
    task_create(&produtorTask2, produtor, "p2");
    task_create(&produtorTask3, produtor, "p3");

    task_create(&consumidorTask1, consumidor, "c1");
    task_create(&consumidorTask2, consumidor, "c2");

    task_join(&produtorTask1);
    task_join(&consumidorTask1);

    task_join(&produtorTask2);
    task_join(&consumidorTask2);
    
    task_join(&produtorTask3);


    task_exit(0);

    return 1;
}