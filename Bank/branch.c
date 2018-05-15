#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include "teller.h"
#include "account.h"
#include "error.h"
#include "debug.h"

#include "branch.h"

/*
 * allocate and initialize each branch.
 */
int
Branch_Init(Bank * bank, int branches_num, int accounts_num,
			AccountAmount init_amount)
{
	bank->branches_num = branches_num;
	bank->branches = malloc(branches_num * sizeof(Branch));

	if (bank->branches == NULL) return -1;
	int accounts_per_branch = accounts_num / branches_num;

	for (int i = 0; i < branches_num; i++)
	{
		Branch * branch = &bank->branches[i];

		// Initialize branch
		branch->branchID = i;
		branch->balance = 0;
		branch->accounts_num = accounts_per_branch;
		branch->accounts = (Account *)malloc(accounts_per_branch * sizeof(Account));
		sem_init(&(branch->branch_lock), 0, 1);

		if (branch->accounts == NULL) return -1;

		// Initialize accounts
		for (int a = 0; a < accounts_per_branch; a++)
		{
			Account_Init(bank, &branch->accounts[a], a, i, init_amount);
			branch->balance += branch->accounts[a].balance;
		}
	}

	return 0;
}

/*
 * update the balance of a branch.
 */
int
Branch_UpdateBalance(Bank * bank, BranchID branchID, AccountAmount change)
{
	assert(bank->branches);
	
	Y;
	if (branchID >= bank->branches_num) return -1;

	AccountAmount oldBalance = bank->branches[branchID].balance;
	Y;

	bank->branches[branchID].balance = oldBalance + change;
	Y;

	return 0;
}

/*
 * get the balance of the branch
 */
int
Branch_Balance(Bank * bank, BranchID branchID, AccountAmount * balance)
{
	assert(bank->branches);

	if (branchID >= bank->branches_num) return -1;

	*balance = bank->branches[branchID].balance;
	Y;
	/* It should be the case that the balance of a branch matches the sum 
   * of all the accounts in the branch.  The following routine validates 
   * this assumption but is far too expense to run in normal operation. 
   */
	/* assert(Branch_Validate(bank, branchID) == 0);  */
	return 0;
}

/*
 * validate the branch by checking its branchID and making sure that
 * its balance equals the sum of balances of all accounts inside
 * the branch.
 */
int
Branch_Validate(Bank * bank, BranchID branchID)
{
	assert(bank->branches);

	if (branchID >= bank->branches_num) return -1;

	Branch *branch = &bank->branches[branchID];
	AccountAmount total = 0;

	for (int a = 0; a < branch->accounts_num; a++)
	{
		total += branch->accounts[a].balance;
	}

	if (total != branch->balance)
	{
		fprintf(stderr, "Branch balance mismatch. "
						"Computer value is %" PRId64 ", but stored value is %" PRId64 "\n",
				total, branch->balance);
		return -1;
	}

	return 0;
}

/*
 * Compare all data inside two branches to see if they are exactly the same.
 */
int
Branch_Compare(Branch * branch1, Branch * branch2)
{
	int err = 0;

	BranchID branch1ID = branch1->branchID;
	BranchID branch2ID = branch2->branchID;

	if (branch1->accounts_num != branch2->accounts_num)
	{
		fprintf(stderr, "Branches %" PRIu64 " and %" PRIu64 " mismatch in numberAccounts "
						"(%d and %d, respectively).\n",
				branch1ID, branch2ID,
				branch1->accounts_num,
				branch2->accounts_num);
		err = -1;
	}

	if (branch1->balance != branch2->balance)
	{
		fprintf(stderr, "Branches %" PRIu64 " and %" PRIu64 " mismatch in balance "
						"(%" PRId64 " and %" PRId64 ", respectively).\n",
				branch1ID, branch2ID,
				branch1->balance, branch2->balance);
		err = -1;
	}

	for (int i = 0; i < branch1->accounts_num; i++)
	{

		assert(branch1->accounts[i].account_number ==
			   branch2->accounts[i].account_number);

		if (branch1->accounts[i].balance != branch2->accounts[i].balance)
		{
			fprintf(stderr,
					"Branch %" PRIu64 " and %" PRIu64 " mismatch in account 0x%" PRIx64 " balance "
					"(%" PRId64 " and %" PRId64 ", respectively).\n",
					branch1ID, branch2ID,
					branch1->accounts[i].account_number,
					branch1->accounts[i].balance,
					branch2->accounts[i].balance);
			err = -1;
		}
	}

	return err;
}
