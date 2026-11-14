
#ifndef APP_STATE_H
#define APP_STATE_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include <queue>

using namespace std;

extern sqlite3 *DB;
extern int currentTransactionID;

struct Change {
    string name, operation, content;
};

extern map<int, vector<Change>> staged;

struct FileLock {
    string fileName;
    int transactionID;
};

struct LockRequest {
    string fileName;
    int transactionID;
};

extern vector<FileLock> lockTable;
extern queue<LockRequest> waitingQueue;

#endif
