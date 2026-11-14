#include "transaction.h"

#include "../common/app_state.h"
#include "../database/database.h"
#include "../os/lock_manager.h"
#include "../filesystem/mvcc.h"

#include <iostream>
#include <algorithm>

using namespace std;




bool persistChange(int id, const Change &c) {
    sqlite3_stmt *s = prep(
        "INSERT INTO transaction_changes(transaction_id, sequence_no, file_name, operation, content) VALUES(?,?,?,?,?);"
    );

    if (!s) return false;

    sqlite3_bind_int(s, 1, id);
    sqlite3_bind_int(s, 2, (int)staged[id].size());

    bindText(s, 3, c.name);
    bindText(s, 4, c.operation);
    bindText(s, 5, c.content);

    return done(s);
}


void loadPersistedChanges() {
    sqlite3_stmt *s = prep(
        "SELECT tc.transaction_id, tc.file_name, tc.operation, tc.content "
        "FROM transaction_changes tc "
        "JOIN transactions t ON t.transaction_id=tc.transaction_id "
        "WHERE t.status='ACTIVE' "
        "ORDER BY tc.transaction_id, tc.sequence_no;"
    );

    if (!s) return;

    while (sqlite3_step(s) == SQLITE_ROW) {
        int id = sqlite3_column_int(s, 0);

        const unsigned char *name = sqlite3_column_text(s, 1);
        const unsigned char *op = sqlite3_column_text(s, 2);
        const unsigned char *content = sqlite3_column_text(s, 3);

        staged[id].push_back({
            name ? (const char*)name : "",
            op ? (const char*)op : "",
            content ? (const char*)content : ""
        });
    }

    sqlite3_finalize(s);

    if (!staged.empty())
        cout << "Recovered staged changes for " << staged.size()
             << " active transaction(s). Select a transaction to resume.\n";
}


bool active(int id) {
    sqlite3_stmt *s = prep(
        "SELECT 1 FROM transactions WHERE transaction_id=? AND status='ACTIVE';"
    );

    if (!s) return false;

    sqlite3_bind_int(s, 1, id);

    bool yes = sqlite3_step(s) == SQLITE_ROW;

    sqlite3_finalize(s);

    return yes;
}


bool requireTx() {
    if (currentTransactionID < 0 || !active(currentTransactionID)) {
        cout << "Select an active transaction first.\n";
        currentTransactionID = -1;
        return false;
    }

    return true;
}


int latestCommitSeq() {
    sqlite3_stmt *s = prep(
        "SELECT value FROM mvcc_metadata WHERE key='commit_seq';"
    );

    if (!s) return 0;

    int v = 0;

    if (sqlite3_step(s) == SQLITE_ROW)
        v = sqlite3_column_int(s, 0);

    sqlite3_finalize(s);

    return v;
}


void beginTransaction() {
    sqlite3_stmt *s = prep(
        "INSERT INTO transactions(status,start_seq) VALUES('ACTIVE',?);"
    );

    if (!s) return;

    sqlite3_bind_int(s, 1, latestCommitSeq());

    if (!done(s)) return;

    int id = (int)sqlite3_last_insert_rowid(DB);

    staged[id] = {};
    currentTransactionID = id;

    cout << "Logical transaction started. ID: " << id
         << "\nStart sequence: " << latestCommitSeq() << "\n";
}


void selectTransaction() {
    sqlite3_stmt *s = prep(
        "SELECT transaction_id FROM transactions "
        "WHERE status='ACTIVE' ORDER BY transaction_id;"
    );

    if (!s) return;

    vector<int> ids;

    cout << "\n--- Active Transactions ---\n";

    while (sqlite3_step(s) == SQLITE_ROW) {
        int id = sqlite3_column_int(s, 0);

        ids.push_back(id);

        cout << "Transaction ID: " << id
             << (id == currentTransactionID ? " (selected)" : "")
             << "\n";
    }

    sqlite3_finalize(s);

    if (ids.empty()) {
        cout << "No active transactions.\n";
        currentTransactionID = -1;
        return;
    }

    int id;

    cout << "Enter transaction ID: ";

    if (!(cin >> id)) {
        cin.clear();
        cin.ignore(10000, '\n');
        cout << "Invalid input.\n";
        return;
    }

    if (find(ids.begin(), ids.end(), id) != ids.end()) {
        currentTransactionID = id;
        cout << "Selected transaction " << id << ".\n";
    }
    else {
        cout << "That transaction is not active.\n";
    }
}


void logOp(int id, const string &op, const string &name) {
    sqlite3_stmt *s = prep(
        "INSERT INTO system_logs(transaction_id,operation,details) VALUES(?,?,?);"
    );

    if (!s) return;

    sqlite3_bind_int(s, 1, id);

    bindText(s, 2, op);
    bindText(s, 3, name);

    done(s);
}


void commitTransaction() {
    if (!requireTx()) return;

    int id = currentTransactionID;
    int seq = latestCommitSeq() + 1;

    if (!execSQL("BEGIN IMMEDIATE;")) return;

    bool ok = true;

    if (hasWriteConflict(id, staged[id])) {
        execSQL("ROLLBACK;");

        cout << "Transaction " << id
             << " was not committed because of a write conflict. "
             << "Its staged changes are retained; abort it before retrying.\n";

        return;
    }

    for (const auto &c : staged[id]) {
        if (!applyChange(id, c, seq)) {
            ok = false;
            break;
        }
    }

    if (ok) {
        sqlite3_stmt *s = prep(
            "UPDATE mvcc_metadata SET value=? WHERE key='commit_seq';"
        );

        if (!s) ok = false;
        else {
            sqlite3_bind_int(s, 1, seq);
            ok = done(s);
        }
    }

    if (ok) {
        sqlite3_stmt *s = prep(
            "UPDATE transactions SET status='COMMITTED',commit_seq=?,"
            "completed_at=CURRENT_TIMESTAMP "
            "WHERE transaction_id=? AND status='ACTIVE';"
        );

        if (!s) ok = false;
        else {
            sqlite3_bind_int(s, 1, seq);
            sqlite3_bind_int(s, 2, id);
            ok = done(s);
        }
    }

    if (ok) {
        sqlite3_stmt *d = prep(
            "DELETE FROM transaction_changes WHERE transaction_id=?;"
        );

        if (!d) ok = false;
        else {
            sqlite3_bind_int(d, 1, id);
            ok = done(d);
        }
    }

    if (ok)
        ok = execSQL("COMMIT;");

    if (!ok) {
        execSQL("ROLLBACK;");
        cout << "Commit failed; staged changes retained.\n";
        return;
    }

    releaseLocks(id);

    staged.erase(id);
    currentTransactionID = -1;

    cout << "Transaction " << id
         << " committed. Commit sequence: " << seq << "\n";
}


void abortTransaction() {
    if (!requireTx()) return;

    int id = currentTransactionID;

    sqlite3_stmt *s = prep(
        "UPDATE transactions SET status='ABORTED',"
        "completed_at=CURRENT_TIMESTAMP "
        "WHERE transaction_id=? AND status='ACTIVE';"
    );

    if (!s) return;

    sqlite3_bind_int(s, 1, id);

    if (done(s)) {
        sqlite3_stmt *d = prep(
            "DELETE FROM transaction_changes WHERE transaction_id=?;"
        );

        if (!d) {
            cout << "Could not clear saved changes.\n";
            return;
        }

        sqlite3_bind_int(d, 1, id);

        if (!done(d)) {
            cout << "Could not clear saved changes.\n";
            return;
        }

        // Remove staged changes only after successful database updates.
        staged.erase(id);

        // Release locks and remove this transaction's waiting requests.
        releaseLocks(id);

        currentTransactionID = -1;

        cout << "Transaction " << id
             << " aborted; staged changes discarded.\n";
    }
}