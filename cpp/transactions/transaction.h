#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <vector>
#include "../common/app_state.h"

using namespace std;

bool persistChange(int id, const Change &c);
void loadPersistedChanges();

bool active(int id);
bool requireTx();
int latestCommitSeq();

void beginTransaction();
void selectTransaction();
void logOp(int id, const string &op, const string &name);

void commitTransaction();
void abortTransaction();

#endif