
#ifndef LOCK_MANAGER_H
#define LOCK_MANAGER_H

#include <string>

using namespace std;

bool acquireLock(int transactionID, const string &fileName);
void releaseLocks(int transactionID);
void viewLocks();

#endif
