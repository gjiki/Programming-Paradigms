#ifndef _BANK_H
#define _BANK_H

#include <pthread.h>
#include <semaphore.h>

typedef struct Bank
{
    unsigned int branches_num;
    int num_workers_to_finish;
    int workers_num;
    int counter;

    struct Branch * branches;
    struct Report * report;
    
    sem_t for_check;
    sem_t next;
    sem_t look_for_transfer;
    sem_t cnt_sem;
    sem_t * semaphores;
} Bank;

#include "account.h"

int Bank_Balance(Bank * bank, AccountAmount * balance);

Bank * Bank_Init(int branches_num, int accounts_num, AccountAmount init_amount,
			AccountAmount reporting_amount, int workers_num);

int Bank_Validate(Bank * bank);
int Bank_Compare(Bank * bank1, Bank * bank2);

#endif /* _BANK_H */
