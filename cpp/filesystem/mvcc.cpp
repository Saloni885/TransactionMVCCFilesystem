#include "mvcc.h"

#include "../common/app_state.h"
#include "../database/database.h"

#include <iostream>
#include <algorithm>

using namespace std;


bool applyChange(int id, const Change &c, int seq) {
    sqlite3_stmt *s = nullptr;

    if (c.operation == "CREATE") {
        s = prep(
            "INSERT INTO files(file_name,current_version,is_deleted) VALUES(?,1,0);"
        );

        if (!s) return false;

        bindText(s, 1, c.name);

        if (!done(s)) return false;

        int fid = (int)sqlite3_last_insert_rowid(DB);

        s = prep(
            "INSERT INTO file_versions(file_id,version_number,content,transaction_id,is_committed,commit_seq) VALUES(?,1,?,?,1,?);"
        );

        if (!s) return false;

        sqlite3_bind_int(s, 1, fid);
        bindText(s, 2, c.content);
        sqlite3_bind_int(s, 3, id);
        sqlite3_bind_int(s, 4, seq);

        return done(s);
    }

    s = prep(
        "SELECT file_id,current_version,is_deleted FROM files WHERE file_name=?;"
    );

    if (!s) return false;

    bindText(s, 1, c.name);

    if (sqlite3_step(s) != SQLITE_ROW) {
        sqlite3_finalize(s);

        cout << "Commit conflict: file '" << c.name
             << "' no longer exists.\n";

        return false;
    }

    int fid = sqlite3_column_int(s, 0);
    int ver = sqlite3_column_int(s, 1) + 1;

    sqlite3_finalize(s);

    if (c.operation == "DELETE") {
        s = prep(
            "UPDATE files SET is_deleted=1,deleted_by_transaction_id=?,deleted_commit_seq=? WHERE file_id=?;"
        );

        if (!s) return false;

        sqlite3_bind_int(s, 1, id);
        sqlite3_bind_int(s, 2, seq);
        sqlite3_bind_int(s, 3, fid);

        return done(s);
    }

    s = prep(
        "INSERT INTO file_versions(file_id,version_number,content,transaction_id,is_committed,commit_seq) VALUES(?,?,?,?,1,?);"
    );

    if (!s) return false;

    sqlite3_bind_int(s, 1, fid);
    sqlite3_bind_int(s, 2, ver);

    bindText(s, 3, c.content);

    sqlite3_bind_int(s, 4, id);
    sqlite3_bind_int(s, 5, seq);

    if (!done(s)) return false;

    s = prep(
        "UPDATE files SET current_version=?,is_deleted=0,deleted_by_transaction_id=NULL,deleted_commit_seq=NULL WHERE file_id=?;"
    );

    if (!s) return false;

    sqlite3_bind_int(s, 1, ver);
    sqlite3_bind_int(s, 2, fid);

    return done(s);
}


bool hasWriteConflict(int id, const vector<Change> &changes) {
    sqlite3_stmt *tx = prep(
        "SELECT start_seq FROM transactions WHERE transaction_id=? AND status='ACTIVE';"
    );

    if (!tx) return true;

    sqlite3_bind_int(tx, 1, id);

    if (sqlite3_step(tx) != SQLITE_ROW) {
        sqlite3_finalize(tx);
        return true;
    }

    int startSeq = sqlite3_column_int(tx, 0);

    sqlite3_finalize(tx);

    // Check each filename once.
    // A committed version newer than this transaction's start sequence means another transaction changed it.

    vector<string> checked;

    for (const auto &c : changes) {
        if (find(checked.begin(), checked.end(), c.name) != checked.end())
            continue;

        checked.push_back(c.name);

        sqlite3_stmt *f = prep(
            "SELECT file_id FROM files WHERE file_name=?;"
        );

        if (!f) return true;

        bindText(f, 1, c.name);

        int fileId = -1;

        if (sqlite3_step(f) == SQLITE_ROW)
            fileId = sqlite3_column_int(f, 0);

        sqlite3_finalize(f);

        if (c.operation == "CREATE") {
            // Creating a name that appeared after this transaction started conflicts.

            if (fileId >= 0) {
                sqlite3_stmt *v = prep(
                    "SELECT MAX(commit_seq) FROM file_versions WHERE file_id=? AND is_committed=1;"
                );

                if (!v) return true;

                sqlite3_bind_int(v, 1, fileId);

                int latest = 0;

                if (sqlite3_step(v) == SQLITE_ROW &&
                    sqlite3_column_type(v, 0) != SQLITE_NULL)
                    latest = sqlite3_column_int(v, 0);

                sqlite3_finalize(v);

                if (latest > startSeq) {
                    cout << "Write conflict: file name '" << c.name
                         << "' was created after transaction " << id
                         << " started.\n";

                    return true;
                }
            }

            continue;
        }

        if (fileId < 0) {
            cout << "Write conflict: file '" << c.name
                 << "' no longer exists.\n";

            return true;
        }

        sqlite3_stmt *v = prep(
            "SELECT MAX(commit_seq) FROM file_versions WHERE file_id=? AND is_committed=1;"
        );

        if (!v) return true;

        sqlite3_bind_int(v, 1, fileId);

        int latest = 0;

        if (sqlite3_step(v) == SQLITE_ROW &&
            sqlite3_column_type(v, 0) != SQLITE_NULL)
            latest = sqlite3_column_int(v, 0);

        sqlite3_finalize(v);

        if (latest > startSeq) {
            cout << "Write conflict: file '" << c.name
                 << "' changed after transaction " << id
                 << " started. Abort this transaction and retry.\n";

            return true;
        }
    }

    return false;
}