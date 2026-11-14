
#include "app_state.h"

sqlite3 *DB = nullptr;
int currentTransactionID = -1;

map<int, vector<Change>> staged;

vector<FileLock> lockTable;
queue<LockRequest> waitingQueue;
