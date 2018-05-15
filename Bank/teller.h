#ifndef _TELLER_H
#define _TELLER_H

#include "bank.h"
#include "account.h"
#include "branch.h"

int Teller_DoDeposit(Bank * bank, AccountNumber account_num, AccountAmount amount);

int Teller_DoWithdraw(Bank * bank, AccountNumber account_num, AccountAmount amount);

int Teller_DoTransfer(Bank * bank, AccountNumber src_account_num,
			AccountNumber dst_account_num, AccountAmount amount);

#endif /* _Teller_H */
