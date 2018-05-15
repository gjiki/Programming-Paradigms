#ifndef _REPORT_H
#define _REPORT_H

#include <stdint.h>

#define MAX_NUM_REPORTS 8		// Maximum number of reports we can store.
#define MAX_LOG_ENTRIES 1024 	// Maximum number of transfer records we can store per report.

typedef struct Report
{
	int numReports; // Number of complete reports filled in

	struct
	{						   			// A report consist of:

		AccountAmount balance; 			// The overall bank balance at the report time
		int hasOverflowed;	 			// Overflow state - 0 if transfer log hasn't overflowed, 1 otherwise
		int numLogEntries;	 			// The number of entries in the log
		
		struct TransferLog
		{ 	// The transfer log contains the accountNum and transfer size

			AccountNumber accountNum;
			AccountAmount transferSize;

		} transferLog[MAX_LOG_ENTRIES];

	} dailyData[MAX_NUM_REPORTS];
	
} Report;

int Report_Init(Bank * bank, AccountAmount report_amount, int max_num_workers);

int Report_DoReport(Bank * bank, int worker_num);
int Report_Transfer(Bank * bank, int worker_num, AccountNumber account_num,
			AccountAmount amount);

int Report_Compare(struct Bank * bank1, struct Bank * bank2);

#endif /* _REPORT_H */
