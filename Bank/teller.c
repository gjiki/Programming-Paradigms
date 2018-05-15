#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include "teller.h"
#include "account.h"
#include "error.h"
#include "debug.h"

/*
 * deposit money into an account
 */
int Teller_DoDeposit(Bank * bank, AccountNumber account_num, AccountAmount amount)
{
	assert(amount >= 0);

	DPRINTF('t', ("Teller_DoDeposit(account 0x%" PRIx64 " amount %" PRId64 ")\n",
				  account_num, amount));

	Account * account = Account_LookupByNumber(bank, account_num);

	if (account == NULL) return ERROR_ACCOUNT_NOT_FOUND;

	sem_wait(&(account->acc_lock));
	sem_wait(&((bank->branches[AccountNum_GetBranchID(account->account_number)]).branch_lock));

	Account_Adjust(bank, account, amount, 1);
	
	sem_post(&(account->acc_lock));
	sem_post(&((bank->branches[AccountNum_GetBranchID(account->account_number)]).branch_lock));

	return ERROR_SUCCESS;
}

/*
 * withdraw money from an account
 */
int Teller_DoWithdraw(Bank * bank, AccountNumber account_num, AccountAmount amount)
{
	assert(amount >= 0);

	DPRINTF('t', ("Teller_DoWithdraw(account 0x%" PRIx64 " amount %" PRId64 ")\n",
				  account_num, amount));

	Account * account = Account_LookupByNumber(bank, account_num);

	sem_wait(&(account->acc_lock));
	sem_wait(&((bank->branches[AccountNum_GetBranchID(account->account_number)]).branch_lock));

	if (account == NULL) return ERROR_ACCOUNT_NOT_FOUND;

	if (amount > Account_Balance(account))
	{
		sem_post(&(account->acc_lock));
		sem_post(&((bank->branches[AccountNum_GetBranchID(account->account_number)]).branch_lock));
		return ERROR_INSUFFICIENT_FUNDS;
	}

	Account_Adjust(bank, account, -amount, 1);

	sem_post(&(account->acc_lock));
	sem_post(&((bank->branches[AccountNum_GetBranchID(account->account_number)]).branch_lock));

	return ERROR_SUCCESS;
}

/*
 * do a tranfer from one account to another account
 */
int Teller_DoTransfer(Bank * bank, AccountNumber src_account_num,
			AccountNumber dst_account_num, AccountAmount amount)
{
	assert(amount >= 0);

	DPRINTF('t', ("Teller_DoTransfer(src 0x%" PRIx64 ", dst 0x%" PRIx64
				  ", amount %" PRId64 ")\n",
				  src_account_num, dst_account_num, amount));

	Account * src_account = Account_LookupByNumber(bank, src_account_num);
	if (src_account == NULL) return ERROR_ACCOUNT_NOT_FOUND;

	Account * dst_account = Account_LookupByNumber(bank, dst_account_num);
	int same_branches = Account_IsSameBranch(src_account_num, dst_account_num);

	BranchID src_branchID = AccountNum_GetBranchID(src_account_num);
	BranchID dst_branchID = AccountNum_GetBranchID(dst_account_num);
    
	if (src_account_num == dst_account_num) return ERROR_SUCCESS;

	if (src_account->account_number < dst_account->account_number)
	{
		sem_wait(&(src_account->acc_lock));
		sem_wait(&(dst_account->acc_lock));
	} else {
		sem_wait(&(dst_account->acc_lock));
		sem_wait(&(src_account->acc_lock));
	}


	if (dst_account == NULL)
	{
		sem_post(&(src_account->acc_lock));
		sem_post(&(dst_account->acc_lock));
		return ERROR_ACCOUNT_NOT_FOUND;
	}

	if (amount > Account_Balance(src_account))
	{
		sem_post(&(src_account->acc_lock));
		sem_post(&(dst_account->acc_lock));
		return ERROR_INSUFFICIENT_FUNDS;
	}

	/*
   * If we are doing a transfer within the branch, we tell the Account module to
   * not bother updating the branch balance since the net change for the
   * branch is 0.
   */
	int update_branch = !Account_IsSameBranch(src_account_num, dst_account_num);
   
    if (update_branch)
	{
		if (src_account_num < dst_account_num)
		{
			sem_wait(&((bank->branches[src_branchID]).branch_lock));
			sem_wait(&((bank->branches[dst_branchID]).branch_lock));
		} else {
			sem_wait(&((bank->branches[dst_branchID]).branch_lock));
			sem_wait(&((bank->branches[src_branchID]).branch_lock));
		}
	}

	Account_Adjust(bank, src_account, -amount, update_branch);
	Account_Adjust(bank, dst_account, amount, update_branch);

	if (update_branch)
	{
		sem_post(&((bank->branches[src_branchID]).branch_lock));
		sem_post(&((bank->branches[dst_branchID]).branch_lock));	
	}
    
	sem_post(&(src_account->acc_lock));
	sem_post(&(dst_account->acc_lock));
	return ERROR_SUCCESS;
}
