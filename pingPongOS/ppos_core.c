// GRR20203910 Thomas Bianchi Todt

#define _POSIX_SOURCE 
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include "ppos.h"
#include "ppos_data.h"
#include "queue.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */
#define PRONTA 0
#define TERMINADA 1
#define SUSPENSA 2
#define ALPHA -1
#define QUANTUM 20

int id = 0;
task_t *currentTask;//, *oldTask;
task_t taskMain, taskDispatcher;
queue_t *userQueue;

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer ;

void dispatcher();
task_t *scheduler();
void task_setprio (task_t *task, int prio);
void treat_tick(int signum);

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
    taskDispatcher.preemptable = 0 ; // passar isso como argumento da funcao task_create??

    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0);


    // ajusta valores do temporizador
    timer.it_value.tv_usec = 1000 ;      // primeiro disparo, em micro-segundos
    timer.it_value.tv_sec  = 0 ;      // primeiro disparo, em segundos
    timer.it_interval.tv_usec = 1000 ;   // disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec  = 0 ;   // disparos subsequentes, em segundos

    // arma o temporizador ITIMER_REAL (vide man setitimer)
    if (setitimer (ITIMER_REAL, &timer, 0) < 0)
    {
        perror ("Erro em setitimer: ") ;
        exit (1) ;
    }


    // registra a ação para o sinal de timer SIGALRM
    action.sa_handler = treat_tick ;
    sigemptyset (&action.sa_mask) ;
    action.sa_flags = 0 ;
    if (sigaction (SIGALRM, &action, 0) < 0)
    {
        perror ("Erro em sigaction: ") ;
        exit (1) ;
    }


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
    task->preemptable = 1; // task de usuario

    task_setprio(task, 0);
    // task->pd = task->pe;

    if(id>2)
        queue_append(&userQueue, (queue_t*)task);

    return task->id; 
}			

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code)
{
    if(currentTask == &taskDispatcher)
    {
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

        if (nextTask != NULL)
        {
            task_switch(nextTask);

            switch(nextTask->status)
            {
                case(PRONTA):

                    break;

                case(TERMINADA):
                    queue_remove(&userQueue, (queue_t*)nextTask);
                    free(nextTask->context.uc_stack.ss_sp);
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
    task_t *next = (task_t*)userQueue;

    task_t *aux = (task_t*)userQueue; // retorna task na primeira posicao

    do
    {
        if(next->pd >= aux->pd)
            next = aux;
        
        aux->pd += ALPHA;
        aux = aux->next;

    } while((task_t*)userQueue != aux);
    

    next->pd = next->pe; // reseta o envelhecimento
    next->quantum = QUANTUM; // quantum inicial

    return next;
}

void task_setprio (task_t *task, int prio)
{
    if(prio > 20 || prio < -20)
        return;

    if(task == NULL)
        currentTask->pe = currentTask->pd = prio;
    
    else
        task->pe = task->pd = prio;
}

int task_getprio (task_t *task)
{
    if(task == NULL)
        return currentTask->pd;

    return task->pd; 
}

void treat_tick (int signum)
{
    if(currentTask->preemptable)
    {
        currentTask->quantum-- ;

        if(currentTask->quantum == 0)
            task_yield();
    }

}