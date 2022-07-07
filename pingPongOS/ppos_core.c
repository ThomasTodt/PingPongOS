// GRR20203910 Thomas Bianchi Todt

#include <stdlib.h>
#include <stdio.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */
#define PRONTA 0
#define TERMINADA 1
#define SUSPENSA 2

int id = 0;
task_t *currentTask;//, *oldTask;
task_t taskMain, taskDispatcher;
queue_t *userQueue;

void dispatcher();
task_t *scheduler();

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init()
{
    //inicializa taskMain (global???)
    // task_t taskMain;
    getcontext(&(taskMain.context));
    taskMain.id = id; // contador global?
    id++;

    taskMain.next = taskMain.prev = &taskMain;    

    currentTask = &taskMain;


    task_create(&(taskDispatcher), &dispatcher, 0);

    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0);

    return;
}

// gerência de tarefas =========================================================

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create(task_t *task, void (*start_func)(void *), void *arg)
{
    getcontext(&(task->context));

    char *stack;
    stack = malloc(STACKSIZE);

    if(stack)
    {
        task->context.uc_stack.ss_sp = stack ;
        task->context.uc_stack.ss_size = STACKSIZE ;
        task->context.uc_stack.ss_flags = 0 ;
        task->context.uc_link = 0 ;
    }
    else
    {
        perror ("Erro na criação da pilha: ") ;
        return -1;
    }

    makecontext(&(task->context), (void*)start_func, 1, arg);

    task->id = id;
    id++;

    task->status = PRONTA;

    if(id>2)
        queue_append(&userQueue, (queue_t*)task);

    // printf("%d\n", task->id);
    return task->id; 
}			

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code)
{
    // printf("%d\n", currentTask->id);
    if(currentTask == &taskDispatcher)
    {
        // printf("oi\n");
        task_switch(&taskMain);
        return;
    }

    currentTask->status = TERMINADA;
    task_switch(&taskDispatcher);

    return;
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task)
{
    task_t *oldTask;
    
    oldTask = currentTask;
    currentTask = task;

    return swapcontext(&(oldTask->context), &(task->context));
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id ()
{
    return (currentTask->id);
}

void task_yield()
{
    task_switch(&taskDispatcher);
}

void dispatcher()
{
    while ( queue_size(userQueue) > 0 )
    {
        task_t *nextTask = scheduler();
        // printf("%d <- %d -> %d\n\n", ((task_t*)(userQueue->prev))->id, ((task_t*)(userQueue))->id, ((task_t*)(userQueue->next))->id);

        if (nextTask != NULL)
        {
            task_switch(nextTask);

            switch(nextTask->status)
            {
                case(PRONTA):

                    break;

                case(TERMINADA):
                    // printf("remover\n");
                    queue_remove(&userQueue, (queue_t*)nextTask);
                    // printf("%d <- %d -> %d\n\n", ((task_t*)(userQueue->prev))->id, ((task_t*)(userQueue))->id, ((task_t*)(userQueue->next))->id);
                    free(nextTask->context.uc_stack.ss_sp); // e a stack do dispatcher?
                    break;

                case(SUSPENSA):
                
                    break;
            }
        }
    }

    task_exit(0);
}

task_t *scheduler()
{
    task_t *next = (task_t*)userQueue; // retorna task na primeira posicao
    userQueue = userQueue->next; // avanca a fila

    return next;
}