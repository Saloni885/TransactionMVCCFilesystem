
#include "lock_manager.h"
#include "../common/app_state.h"
#include "../database/database.h"
#include <iostream>

using namespace std;

bool acquireLock(int transactionID, const string &fileName)
{
    // Check whether this transaction already owns the lock.
    for (const auto &lock : lockTable) {
        if (lock.fileName == fileName &&
            lock.transactionID == transactionID) {
            return true;
        }
    }

    // Check whether another transaction owns the lock.
    for (const auto &lock : lockTable) {
        if (lock.fileName == fileName &&
            lock.transactionID != transactionID) {

            // Avoid duplicate waiting requests.
            queue<LockRequest> temp = waitingQueue;

            while (!temp.empty()) {
                LockRequest request = temp.front();
                temp.pop();

                if (request.fileName == fileName &&
                    request.transactionID == transactionID) {
                    cout << "Transaction " << transactionID
                         << " is already waiting for file "
                         << fileName << ".\n";
                    return false;
                }
            }

            waitingQueue.push({fileName, transactionID});

            cout << "Transaction " << transactionID
                 << " is waiting for file " << fileName
                 << ", locked by transaction "
                 << lock.transactionID << ".\n";

            return false;
        }
    }

    // No other transaction owns this lock.
    lockTable.push_back({fileName, transactionID});

    cout << "Exclusive lock acquired on " << fileName
         << " by transaction " << transactionID << ".\n";

    return true;
}

void releaseLocks(int transactionID)
{
    // Release all locks owned by this transaction.
    for (auto it = lockTable.begin(); it != lockTable.end();) {
        if (it->transactionID == transactionID) {
            cout << "Released lock on " << it->fileName
                 << " from transaction " << transactionID << ".\n";

            it = lockTable.erase(it);
        }
        else {
            ++it;
        }
    }

    // Remove this transaction's requests from the waiting queue.
    queue<LockRequest> remaining;

    while (!waitingQueue.empty()) {
        LockRequest request = waitingQueue.front();
        waitingQueue.pop();

        if (request.transactionID != transactionID) {
            remaining.push(request);
        }
    }

    waitingQueue = remaining;

    cout << "Checking waiting transactions...\n";

    // Process requests that were waiting.
    int count = (int)waitingQueue.size();

    while (count--) {
        LockRequest request = waitingQueue.front();
        waitingQueue.pop();

        sqlite3_stmt *s = nullptr;

        if (sqlite3_prepare_v2(
                DB,
                "SELECT status FROM transactions WHERE transaction_id=?;",
                -1, &s, nullptr) != SQLITE_OK) {
            waitingQueue.push(request);
            continue;
        }

        sqlite3_bind_int(s, 1, request.transactionID);

        bool isActive = false;

        if (sqlite3_step(s) == SQLITE_ROW) {
            const unsigned char *status = sqlite3_column_text(s, 0);

            if (status != nullptr &&
                string(reinterpret_cast<const char*>(status)) == "ACTIVE") {
                isActive = true;
            }
        }

        sqlite3_finalize(s);

        if (!isActive) {
            cout << "Removed inactive transaction "
                 << request.transactionID
                 << " from the waiting queue.\n";
            continue;
        }

        // Check whether the requested file is still locked.
        bool occupied = false;

        for (const auto &lock : lockTable) {
            if (lock.fileName == request.fileName) {
                occupied = true;
                break;
            }
        }

        // Grant lock if available.
        if (!occupied) {
            lockTable.push_back(
                {request.fileName, request.transactionID}
            );

            cout << "Transaction " << request.transactionID
                 << " acquired the lock on "
                 << request.fileName
                 << ". Retry its waiting operation.\n";
        }
        else {
            waitingQueue.push(request);
        }
    }
}

void viewLocks()
{
    cout << "\n--- Lock Table ---\n";

    if (lockTable.empty()) {
        cout << "No locks held.\n";
    }
    else {
        for (const auto &lock : lockTable) {
            cout << "File: " << lock.fileName
                 << " | Owner: T" << lock.transactionID << "\n";
        }
    }

    cout << "\n--- Waiting Queue ---\n";

    if (waitingQueue.empty()) {
        cout << "No waiting transactions.\n";
    }
    else {
        queue<LockRequest> temp = waitingQueue;

        while (!temp.empty()) {
            LockRequest request = temp.front();
            temp.pop();

            cout << "T" << request.transactionID
                 << " waiting for " << request.fileName << "\n";
        }
    }
}
