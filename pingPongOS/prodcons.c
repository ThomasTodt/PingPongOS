#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ppos.h"
#include "queue.h"

task_t task[5] ;
semaphore_t  s_item, s_buffer, s_vaga ;
queue_t *q_buffer;

typedef struct buff_item
{
  struct buff_item *prev, *next ;		// ponteiros para usar em filas
  int num ;
} buff_item ;

void produtor(void *arg)
{
    while (1)
    {
        task_sleep (1000);

        buff_item *item = malloc(sizeof(buff_item));
        item->num = rand() % 100;
        item->next = item->prev = NULL;
        // printf("%p\n", item);

        sem_down (&s_vaga);

        sem_down (&s_buffer);

        // if(queue_size(q_buffer) < 5)
        // {
            queue_append(&q_buffer, (queue_t*)item);
            printf("%s inseriu %d (tem %d)\n", (char*)arg, item->num, queue_size(q_buffer));
        // }

        sem_up (&s_buffer);

        sem_up (&s_item);

    }
}

void consumidor(void *arg)
{
    task_sleep (1000);
    while (1)
    {
        sem_down(&s_item);

        buff_item *item;

        sem_down(&s_buffer);

        if(queue_size(q_buffer) > 0)
        {
            item = (buff_item*)q_buffer;
            queue_remove(&q_buffer, q_buffer);
            printf("%s consumiu %d (tem %d)\n", (char*)arg, item->num, queue_size(q_buffer));
            free(item);
        }

        sem_up(&s_buffer);

        sem_up(&s_vaga);

        task_sleep(1000);
    }
}


int main()
{
    ppos_init () ;
    srand(time(0));

    sem_create (&s_buffer, 1) ;
    sem_create (&s_item, 0) ;
    sem_create (&s_vaga, 4) ;


    task_create (&task[0], produtor, "P1") ;
    task_create (&task[1], produtor, "  P2") ;
    task_create (&task[2], produtor, "    P3") ;

    task_create (&task[4], consumidor, "                                  C1") ;
    task_create (&task[3], consumidor, "                                    C2") ;

    for (int i=0; i<5; i++)
        task_join (&task[i]) ;

    sem_destroy(&s_buffer);
    sem_destroy(&s_item);
    sem_destroy(&s_vaga);

    task_exit (0) ;

    exit (0) ;
}