// GRR20203910 Thomas Bianchi Todt

#include <stdio.h>
#include "queue.h"

int queue_size(queue_t *queue)
{
    if(queue == NULL)
        return 0;


    queue_t *first = queue;
    queue_t *current = first->next;

    int count = 1;
    while(current != first)
    {
        count++;
        current = current->next;
    }

    return count;
}

void queue_print(char *name, queue_t *queue, void print_elem (void*))
{
    // printf("%s\b\b: ", name); // precisa dessa gambiarra??
    printf("%s: ", name);

    if(queue == NULL)
    {
        printf("[]\n");
        return;
    }


    queue_t *first, *current;
    current = first = queue;

    printf("[");
    do {
        print_elem(current);
        printf(" ");
        current = current->next;
    } while(current != first);
    printf("\b]\n");

    return;
}

int queue_append(queue_t **queue, queue_t *elem)
{
    if(queue == NULL)
    {
        fprintf(stderr, "Fila não existe\n");
        return -1;
    }
    else if(elem == NULL)
    {
        fprintf(stderr, "Elemento não existe\n");
        return -2;
    }
    else if(elem->next != NULL || elem->prev != NULL) // AND? OR? testar soh 1?
    {
        fprintf(stderr, "Elemento já está em uma fila\n");
        return -3;
    }


    if(queue_size(*queue) == 0)
    {
        (*queue) = elem;
        elem->next = elem->prev = elem;
    }
    else
    {
        queue_t *first = *queue;
        queue_t *last = first->prev;

        last->next = first->prev = elem;
        elem->prev = last;
        elem->next = first;
    }

    return 0;
}

// retorna 1 se elem pertence a queue
// 0 caso contrário
// não verifica existências
int belongs_to_queue(queue_t *queue, queue_t *elem)
{
    queue_t *first, *current;
    current = first = queue;

    do {
        if(elem == current)
            return 1;
        current = current->next;
    } while(current != first);

    return 0;
}

int queue_remove(queue_t **queue, queue_t *elem)
{
    if(queue == NULL)
    {
        fprintf(stderr, "Fila não existe\n");
        return -1;
    }
    else if(queue_size(*queue) == 0)
    {
        fprintf(stderr, "Fila vazia\n");
        return -2;
    }
    else if(elem == NULL)
    {
        fprintf(stderr, "Elemento não existe\n");
        return -3;
    }
    else if( !belongs_to_queue(*queue, elem) )
    {
        fprintf(stderr, "Elemento não pertence à fila\n");
        return -4;
    }


    if(queue_size(*queue) == 1)
        *queue = NULL;

    else if((*queue) == elem)
        *queue = elem->next;

    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    elem->prev = elem->next = NULL;

    return 0;
}