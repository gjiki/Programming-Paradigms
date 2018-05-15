#ifndef _ACCOUNT_H
#define _ACCOUNT_H

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include "bank.h"

typedef uint64_t BranchID;
typedef uint64_t AccountNumber;
typedef int64_t AccountAmount;

typedef struct Account
{
    AccountNumber account_number;
    AccountAmount balance;
    sem_t acc_lock;
} Account;

Account * Account_LookupByNumber(Bank * bank,
            AccountNumber account_num);

void Account_Adjust(Bank * bank, Account * account, AccountAmount amount,
            int update_branch);

AccountAmount Account_Balance(Account * account);

AccountNumber Account_MakeAccountNum(int branch, int subaccount);

int Account_IsSameBranch(AccountNumber account_num1, AccountNumber account_num2);

void Account_Init(Bank * bank, Account * account, int id, int branch,
            AccountAmount init_amount);

BranchID AccountNum_GetBranchID(AccountNumber account_num);

#endif /* _ACCOUNT_H */
