// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.4 -- Janeiro de 2022

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
  struct task_t *prev, *next ;		// ponteiros para usar em filas
  int id ;				// identificador da tarefa
  ucontext_t context ;			// contexto armazenado da tarefa
  short status ;			// pronta, rodando, suspensa, ...
  short preemptable ;			// pode ser preemptada?
  short pd, pe ; // prioridade estatica e dinamica
  int quantum ;
  unsigned int nascimento ;
  unsigned int tempo_proc ;
  unsigned int ativacoes ;
  int encerramento ;
  struct task_t *esperando ;
  unsigned int acordar ;
   // ... (outros campos serão adicionados mais tarde)
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  // preencher quando necessário
  int contador;
  task_t *fila;
  int existe;
  int lock;
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

typedef struct mitem_t
{
  struct mitem_t *prev, *next ;		// ponteiros para usar em filas
  void *msg ;
} mitem_t ;

// estrutura que define uma fila de mensagens
typedef struct mqueue_t
{
  // struct mqueue_t *prev, *next ;		// ponteiros para usar em filas
  // int capacidade ;
  int msg_size ;
  // void *msg ;
  semaphore_t *vagas, *itens, *caixa ;
  mitem_t *fila ; 
  // preencher quando necessário
} mqueue_t ;

#endif

