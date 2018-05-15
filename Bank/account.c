
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "teller.h"
#include "account.h"
#include "error.h"
#include "debug.h"

#include "branch.h"
#include "report.h"

/*
 * initialize the account based on the passed-in information.
 */
void
Account_Init(Bank * bank, Account * account, int id, int branch,
            AccountAmount init_amount)
{
    sem_init(&(account->acc_lock), 0, 1);
    extern int testfailurecode;

    account->account_number = Account_MakeAccountNum(branch, id);
    account->balance = init_amount;
    
    if (testfailurecode)
    {
        // To test failures, we initialize every 4th account with a negative value
        if ((id & 0x3) == 0) account->balance = -1;
    }
}

/*
 * get the ID of the branch which the account is in.
 */
BranchID
AccountNum_GetBranchID(AccountNumber account_num)
{
    Y;
    return (BranchID)(account_num >> 32);
}

/*
 * get the branch-wide subaccount number of the account.
 */
int
AcountNum_Subaccount(AccountNumber account_num)
{
    Y;
    return (account_num & 0x7ffffff);
}

/*
 * find the account address based on the accountNum.
 */
Account *
Account_LookupByNumber(Bank * bank, AccountNumber account_num)
{
    BranchID branchID = AccountNum_GetBranchID(account_num);
    int branchIndex = AcountNum_Subaccount(account_num);
    return &(bank->branches[branchID].accounts[branchIndex]);
}

/*
 * adjust the balance of the account.
 */
void
Account_Adjust(Bank * bank, Account * account, AccountAmount amount,
            int update_branch)
{
    account->balance = Account_Balance(account) + amount;
    if (update_branch)
    {
        Branch_UpdateBalance(bank, AccountNum_GetBranchID(account->account_number), amount);
    }
    Y;
}
/*
 * return the balance of the account.
 */
AccountAmount
Account_Balance(Account * account)
{
    AccountAmount balance = account->balance;
    Y;
    return balance;
}

/*
 * make the account number based on the branch number and
 * the branch-wise subaccount number.
 */
AccountNumber
Account_MakeAccountNum(int branch, int subaccount)
{
    AccountNumber num;

    num = subaccount;
    num |= ((uint64_t)branch) << 32;
    Y;
    return num;
}

/*
 * Test to see if two accounts are in the same branch.
 */

int
Account_IsSameBranch(AccountNumber account_num1, AccountNumber account_num2)
{
    return (AccountNum_GetBranchID(account_num1) ==
            AccountNum_GetBranchID(account_num2));
}
