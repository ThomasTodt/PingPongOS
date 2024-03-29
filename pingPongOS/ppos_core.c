// GRR20203910 Thomas Bianchi Todt

#define _POSIX_SOURCE 
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "ppos.h"
#include "ppos_data.h"
#include "queue.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */
#define PRONTA 0
#define TERMINADA 1
#define SUSPENSA 2
#define ALPHA -1
#define QUANTUM 20
#define CHECA_DORM 100
#define ACORDADA -1

int id = 0;
task_t *currentTask;//, *oldTask;
task_t taskMain, taskDispatcher;
queue_t *q_prontas, *q_suspensas, *q_dormitorio;
unsigned int relogio = 0;

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer ;

// para o TSL do semaforo
int lock = 0;

void dispatcher();
task_t *scheduler();
void task_setprio (task_t *task, int prio);
void treat_tick(int signum);
unsigned int systime();
void task_yield();
void gerencia_dormitorio();
void enter_cs (int *lock);
void leave_cs (int *lock);

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init()
{
    //inicializa taskMain (global???)
    // task_t taskMain;
    getcontext(&(taskMain.context));
    taskMain.id = id; // contador global?
    id++;

    // taskMain.next = taskMain.prev = &taskMain;   
    taskMain.status = PRONTA;
    taskMain.preemptable = 1; // de usuario
    taskMain.nascimento = systime(); 

    queue_append(&q_prontas, (queue_t*)&taskMain);

    currentTask = &taskMain;

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

    task_create(&(taskDispatcher), &dispatcher, 0);
    // taskDispatcher.preemptable = 0 ; // passar isso como argumento da funcao task_create??

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


    // task->preemptable = 1; // task de usuario

    task->nascimento = systime();

    task_setprio(task, 0);
    // task->pd = task->pe;

    if(id>2) // nao eh main nem dispatcher?
        queue_append(&q_prontas, (queue_t*)task);

    if(task == &taskDispatcher)
    {
        task->preemptable = 0;
        task_switch(task);
    }
    else
        task->preemptable = 1;

    return task->id; 
}			

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code)
{
    if(currentTask == &taskDispatcher)
    {
        printf("Task %d exit: execution time %u ms, processor time %4u ms, %u activations\n",
                            taskDispatcher.id, (systime() - taskDispatcher.nascimento), taskDispatcher.tempo_proc, taskDispatcher.ativacoes);
        // task_switch(&taskMain);
        return; // exit_code;
    }

    currentTask->encerramento = exit_code;
    currentTask->status = TERMINADA;

    queue_t *tmp = q_suspensas;
    for(int i=0; i < queue_size(q_suspensas); i++)
    {
        if(((task_t*)tmp)->esperando == currentTask)
            task_resume((task_t*)tmp, (task_t**)&q_suspensas);
        tmp = tmp->next;
    }

    task_yield();

    return;
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task)
{
    task_t *oldTask;
    
    oldTask = currentTask;
    currentTask = task;

    task->ativacoes++;

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
    unsigned int aux = systime();
    while ( queue_size(q_prontas) > 0 
            || queue_size(q_suspensas) > 0
            || queue_size(q_dormitorio) > 0)
    {
        task_t *nextTask = scheduler();

        if (nextTask != NULL)
        {
            // nextTask->ativacoes++;
            taskDispatcher.tempo_proc += systime() - aux;
            aux = systime();
            task_switch(nextTask);
            nextTask->tempo_proc += systime() - aux;
            aux = systime();

            switch(nextTask->status)
            {
                case(PRONTA):

                    break;

                case(TERMINADA):
                    printf("Task %d exit: execution time %u ms, processor time %u ms, %u activations\n",
                            nextTask->id, (systime() - nextTask->nascimento), nextTask->tempo_proc, nextTask->ativacoes);
                    queue_remove(&q_prontas, (queue_t*)nextTask);
                    free(nextTask->context.uc_stack.ss_sp);
                    break;

                case(SUSPENSA):
                
                    break;
            }
        }

        if( !(systime() % CHECA_DORM) ) // a cada quanto tempo checa o dormitorio
            gerencia_dormitorio();
    }

    task_exit(0);
}

task_t *scheduler()
{
    if(!q_prontas)
        return NULL;

    task_t *next = (task_t*)q_prontas;

    task_t *aux = (task_t*)q_prontas; // retorna task na primeira posicao

    do
    {
        if(next->pd >= aux->pd)
            next = aux;
        
        aux->pd += ALPHA;
        aux = aux->next;

    } while((task_t*)q_prontas != aux);
    

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
    relogio++;

    if(currentTask->preemptable)
    {
        currentTask->quantum-- ;

        if(currentTask->quantum == 0)
            task_yield();
    }

}

unsigned int systime()
{
    return relogio;
}

int task_join (task_t *task)
{
    if(task == NULL || task->status == TERMINADA)
        return -1;

    currentTask->esperando = task; // soh pode esperar uma de cada vez mesmo
    task_suspend((task_t**)&q_suspensas);
    return task->encerramento;
}

void task_suspend(task_t **queue)
{
    enter_cs(&lock);

    queue_remove(&q_prontas, (queue_t*)currentTask);
    currentTask->status = SUSPENSA;
    queue_append((queue_t**)queue, (queue_t*)currentTask);

    leave_cs(&lock);

    task_yield();
}

void task_resume(task_t *task, task_t **queue)
{
    if(queue)
    {
        queue_remove((queue_t**)queue, (queue_t*)task);     
        task->status = PRONTA;
        queue_append(&q_prontas, (queue_t*)task);
    }
}

void task_sleep(int t)
{
    currentTask->acordar = systime() + t;
    task_suspend((task_t**)&q_dormitorio);
}

void gerencia_dormitorio()
{
    queue_t *tmp = q_dormitorio;
    for(int i=0; i < queue_size(q_dormitorio); i++)
    {
        if(((task_t*)tmp)->acordar <= systime()
            && ((task_t*)tmp)->acordar != ACORDADA) // se ja ta na hora ou passou de acordar
        {
            queue_remove(&q_dormitorio, tmp);
            queue_append(&q_prontas, tmp);
            ((task_t*)tmp)->acordar = ACORDADA;
        }
        tmp = tmp->next;
    }
}

void enter_cs (int *lock)
{
    // printf("        entrou lock\n");
     // atomic OR (Intel macro for GCC)
    while (__sync_fetch_and_or (lock, 1)) ;   // busy waiting
    // printf("        saiu lock\n");
}
 
void leave_cs (int *lock)
{
    // printf("        entrou leave\n");
    (*lock) = 0 ;
    // printf("        saiu leave\n");
}

int sem_create (semaphore_t *s, int value)
{
    // printf("entrou create\n");

    if(!s || s->existe == 1)
    {
        return -1;
    } 


    s->contador = value;
    s->fila = NULL;

    s->existe = 1; // jeito melhor de checar se existe?

    // s->lock = 0;

    // enter_cs(lock);
    if (s->contador != value)
        return -1;

    // leave_cs(&(lock));
    // printf("saiu create\n");
    return 0;
}

int sem_down (semaphore_t *s)
{
    // printf("    entrou down\n");
    enter_cs(&(lock));

    if (!s || s->existe != 1)
    {
        // printf("erro down\n");
        leave_cs(&lock);
        return -1;
    }
    // printf("contador: %d\n", s->contador);
    int conta = s->contador;
    leave_cs(&lock);

    if ((conta - 1) < 0)
    {
        task_suspend(&(s->fila));
    }

    enter_cs(&lock);
    s->contador--;
    // printf("contador: %d\n", s->contador);

    if (!s || !(s->existe)) //semaforo foi destruido
    {
        leave_cs(&lock);
        return -1;
    }

    leave_cs(&(lock));
    // printf("    saiu down\n");

    return 0;
}

int sem_up (semaphore_t *s)
{
    // printf("    entrou up: %d\n", lock);
    enter_cs(&(lock));
    // printf("aqui\n");
    if (!s || s->existe != 1)
    {
        leave_cs(&lock);
        printf("erro up\n");
        return -1;
    }

    // printf("aqui\n");
    s->contador++;
    // printf("contador: %d\n", s->contador);
    // printf("aqui\n");
    if (queue_size((queue_t*)s->fila))
        task_resume(s->fila, &(s->fila));
    // printf("depois\n");
    leave_cs(&(lock));

    // printf("    saiu up\n");
    return 0;
}

int sem_destroy (semaphore_t *s)
{
    // printf("entrou destroy\n");
    enter_cs(&(lock));

    if (!s || s->existe != 1)
        return -1;

    s->existe = 0; // util pra se a tarefa perder o processador, mas ja tiver acordado uma outra antes de setar s como NULL

    while (s->fila) // acorda cada tarefa na fila do semaforo
    {
        task_resume(s->fila, &(s->fila));
    }

    if (s->fila)
        return -1;


    s = NULL;
    if (s)
        return -1;

    leave_cs(&(lock));
    return 0;
}

int mqueue_create (mqueue_t *queue, int max_msgs, int msg_size)
{
    queue->msg_size   = msg_size;
    queue->fila = NULL;

    queue->vagas = malloc(sizeof(semaphore_t));
    queue->itens = malloc(sizeof(semaphore_t));
    queue->caixa = malloc(sizeof(semaphore_t));

    if(sem_create(queue->vagas, max_msgs-1)) return -1;
    if(sem_create(queue->itens, 0)) return -1;
    if(sem_create(queue->caixa, 1)) return -1;

    return 0;
}

int mqueue_send (mqueue_t *queue, void *msg)
{
    if(sem_down(queue->vagas)) return -1;
    if(sem_down(queue->caixa)) return -1;

    mitem_t *item = malloc(sizeof(mitem_t));
    item->msg = malloc(queue->msg_size);

    if(!item) return -1;
    if(!(item->msg)) return -1;

    memcpy(item->msg, msg, queue->msg_size);

    if(queue_append((queue_t**)&queue->fila, (queue_t*)item)) return -1;

    if(sem_up(queue->caixa)) return -1;
    if(sem_up(queue->itens)) return -1;

    return 0;
}

int mqueue_recv (mqueue_t *queue, void *msg)
{
    if(sem_down(queue->itens)) return -1;
    if(sem_down(queue->caixa)) return -1;

    memcpy(msg, queue->fila->msg, queue->msg_size);

    if(queue_remove((queue_t**)&queue->fila, (queue_t*)queue->fila)) return -1;


    if(sem_up(queue->caixa)) return -1;
    if(sem_up(queue->vagas)) return -1;

    return 0;
}

int mqueue_destroy (mqueue_t *queue)
{
    while(mqueue_msgs(queue) > 0)
    {
        if(queue_remove((queue_t**)&queue->fila, (queue_t*)queue->fila)) return -1;
    }

    if(sem_destroy(queue->itens)) return -1; // tenta destruir sempre mais de uma vez, testar sem->existe
    if(sem_destroy(queue->vagas)) return -1;
    if(sem_destroy(queue->caixa)) return -1;

    return 0;
}

int mqueue_msgs (mqueue_t *queue)
{
    int size = queue_size((queue_t*)queue->fila);

    if(size == 1 && !(queue->fila))
        return 0;

    return size;
}