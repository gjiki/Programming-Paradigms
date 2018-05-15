#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "error.h"

#include "bank.h"
#include "branch.h"
#include "account.h"
#include "report.h"

/*
 * allocate the bank structure and initialize the branches.
 */
Bank *
Bank_Init(int branches_num, int accounts_num, AccountAmount init_amount,
			AccountAmount reporting_amount, int workers_num)
{
	Bank * bank = malloc(sizeof(Bank));
	if (bank == NULL) return bank;

	// Initialize bank
	bank->num_workers_to_finish = workers_num;
	bank->workers_num = workers_num;
	bank->counter = workers_num;

	// Initialize semaphores
	sem_init(&(bank->for_check), 0, 1);
	sem_init(&(bank->next), 0, 0);
	sem_init(&(bank->look_for_transfer), 0, 1);
	sem_init(&(bank->cnt_sem), 0, 1);

	bank->semaphores = malloc(sizeof(sem_t) * bank->workers_num);
	for (int i = 0; i < bank->workers_num; i++)
	{
		sem_init(&(bank->semaphores[i]), 0, 0);
	}

	// Initialize branch and report
	Branch_Init(bank, branches_num, accounts_num, init_amount);
	Report_Init(bank, reporting_amount, workers_num);

	return bank;
}

/*
 * get the balance of the entire bank by adding up all the balances in
 * each branch.
 */
int
Bank_Balance(Bank * bank, AccountAmount * balance)
{
	assert(bank->branches);

	AccountAmount bankTotal = 0;
	for (unsigned int branch = 0; branch < bank->branches_num; branch++)
	{
		sem_wait(&((bank->branches[branch]).branch_lock));
		AccountAmount branchBalance;
		int err = Branch_Balance(bank, bank->branches[branch].branchID, &branchBalance);

		if (err < 0)
		{
			for (unsigned int i = 0; i < branch; i++)
			{
				sem_post(&((bank->branches[i]).branch_lock));
			}
			return err;
		}

		bankTotal += branchBalance;
	}

	*balance = bankTotal;
	for (unsigned int branch = 0; branch < bank->branches_num; branch++)
	{
		sem_post(&((bank->branches[branch]).branch_lock));
	}

	return 0;
}

/*
 * tranverse and validate each branch.
 */
int
Bank_Validate(Bank * bank)
{
	assert(bank->branches);

	int err = 0;
	for (unsigned int branch = 0; branch < bank->branches_num; branch++)
	{
		int berr = Branch_Validate(bank, bank->branches[branch].branchID);
		if (berr < 0) err = berr;
	}

	return err;
}

/*
 * compare the data inside two banks and see they are exactly the same;
 * it is called in BankTest.
 */
int
Bank_Compare(Bank * bank1, Bank * bank2)
{
	int err = 0;

	if (bank1->branches_num != bank2->branches_num)
	{
		fprintf(stderr, "Bank num branches mismatch\n");
		return -1;
	}

	for (unsigned int branch = 0; branch < bank1->branches_num; branch++)
	{
		int berr = Branch_Compare(&bank1->branches[branch],
					&bank2->branches[branch]);
		if (berr < 0) err = berr;
	}

	int cerr = Report_Compare(bank1, bank2);
	if (cerr < 0) err = cerr;

	return err;
}
