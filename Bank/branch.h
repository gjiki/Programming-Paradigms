#ifndef _BRANCH_H
#define _BRANCH_H

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include "account.h"

typedef uint64_t BranchID;

typedef struct Branch
{
	BranchID branchID;
	AccountAmount balance;
	int accounts_num;
	Account * accounts;
	sem_t branch_lock;
} Branch;

int Branch_Balance(Bank * bank, BranchID branchID,
			AccountAmount * balance);
int Branch_UpdateBalance(Bank * bank, BranchID branchID,
			AccountAmount change);

int Branch_Init(Bank * bank, int branches_num, int accounts_num,
			AccountAmount init_amount);

int Branch_Validate(Bank * bank, BranchID branchID);
int Branch_Compare(Branch * branch1, Branch * branch2);

#endif /* _BRANCH_H */
