#include "snapshot.h"

#include "../common/app_state.h"
#include "../database/database.h"
#include "../transactions/transaction.h"

#include <iostream>
#include <vector>
#include <string>
#include <utility>

using namespace std;


bool createSnapshot(const string &name, string &message) {

    if (name.empty()) {
        message = "Snapshot name cannot be empty.";
        return false;
    }

    sqlite3_stmt *x = prep(
        "SELECT COUNT(*) FROM transactions WHERE status='ACTIVE';"
    );

    if (!x) {
        message = "Could not check active transactions.";
        return false;
    }

    bool activeTx =
        sqlite3_step(x) == SQLITE_ROW &&
        sqlite3_column_int(x, 0) > 0;

    sqlite3_finalize(x);

    if (activeTx) {
        message = "Commit or abort all active transactions before snapshot.";
        return false;
    }

    string data;

    sqlite3_stmt *s = prep(
        "SELECT f.file_name, fv.content "
        "FROM files f "
        "JOIN file_versions fv ON fv.file_id = f.file_id "
        "WHERE f.is_deleted = 0 AND fv.is_committed = 1 "
        "AND fv.version_number = ("
        "SELECT MAX(v.version_number) "
        "FROM file_versions v "
        "WHERE v.file_id = f.file_id AND v.is_committed = 1"
        ") ORDER BY f.file_name;"
    );

    if (!s) {
        message = "Could not read committed files.";
        return false;
    }

    vector<pair<string, string>> fs;

    while (sqlite3_step(s) == SQLITE_ROW) {
        const char *fileName =
            (const char *)sqlite3_column_text(s, 0);

        const char *content =
            (const char *)sqlite3_column_text(s, 1);

        fs.push_back({
            fileName ? fileName : "",
            content ? content : ""
        });
    }

    sqlite3_finalize(s);

    auto field = [&](const string &v) {
        data += to_string(v.size()) + ":" + v;
    };

    field(to_string(fs.size()));

    for (auto &f : fs) {
        field(f.first);
        field(f.second);
    }

    s = prep(
        "INSERT INTO snapshots(snapshot_name, state_data) VALUES(?,?);"
    );

    if (!s) {
        message = "Could not prepare snapshot insertion.";
        return false;
    }

    bindText(s, 1, name);

    sqlite3_bind_text(
        s, 2, data.data(), (int)data.size(), SQLITE_TRANSIENT
    );

    if (done(s)) {
        message = "Snapshot created: " + name +
                  " (" + to_string(fs.size()) + " files)";
        return true;
    }

    message = "Snapshot creation failed.";
    return false;
}


// Console version: preserves the existing menu functionality
void createSnapshot() {

    string name;

    cout << "Enter snapshot name: ";
    getline(cin >> ws, name);

    string message;

    createSnapshot(name, message);

    cout << message << "\n";
}

void viewSnapshots() {

    sqlite3_stmt *s = prep(
        "SELECT snapshot_name, created_at "
        "FROM snapshots ORDER BY snapshot_id;"
    );

    if (!s) return;

    cout << "\n--- Snapshots ---\n";

    while (sqlite3_step(s) == SQLITE_ROW) {
        cout << sqlite3_column_text(s, 0)
             << " | "
             << (sqlite3_column_text(s, 1)
                 ? (const char *)sqlite3_column_text(s, 1)
                 : "")
             << "\n";
    }

    sqlite3_finalize(s);
}


bool readField(const string &d, size_t &p, string &v) {

    size_t c = d.find(':', p);

    if (c == string::npos) return false;

    size_t n = 0;

    try {
        n = stoull(d.substr(p, c - p));
    }
    catch (...) {
        return false;
    }

    p = c + 1;

    if (n > d.size() - p) return false;

    v = d.substr(p, n);
    p += n;

    return true;
}


void restoreSnapshot() {

    sqlite3_stmt *cnt = prep(
        "SELECT COUNT(*) FROM transactions WHERE status='ACTIVE';"
    );

    if (!cnt) return;

    bool has =
        sqlite3_step(cnt) == SQLITE_ROW &&
        sqlite3_column_int(cnt, 0) > 0;

    sqlite3_finalize(cnt);

    if (has) {
        cout << "Commit or abort all active transactions first.\n";
        return;
    }

    string name;

    cout << "Enter snapshot name: ";
    getline(cin >> ws, name);

    sqlite3_stmt *s = prep(
        "SELECT state_data FROM snapshots WHERE snapshot_name=?;"
    );

    if (!s) return;

    bindText(s, 1, name);

    if (sqlite3_step(s) != SQLITE_ROW) {
        sqlite3_finalize(s);
        cout << "Snapshot not found.\n";
        return;
    }

    string d = (const char *)sqlite3_column_text(s, 0);

    sqlite3_finalize(s);

    size_t p = 0;
    string ntext;

    if (!readField(d, p, ntext)) {
        cout << "Invalid snapshot data.\n";
        return;
    }

    size_t n;

    try {
        n = stoull(ntext);
    }
    catch (...) {
        cout << "Invalid snapshot data.\n";
        return;
    }

    vector<pair<string, string>> fs;

    for (size_t i = 0; i < n; i++) {

        string a, b;

        if (!readField(d, p, a) || !readField(d, p, b)) {
            cout << "Invalid snapshot data.\n";
            return;
        }

        fs.push_back({a, b});
    }

    if (!execSQL("BEGIN IMMEDIATE;")) return;

    bool ok = true;

    int seq = latestCommitSeq() + 1;

    sqlite3_stmt *t = prep(
        "INSERT INTO transactions(status, start_seq) VALUES('ACTIVE',?);"
    );

    if (!t) ok = false;

    int id = -1;

    if (ok) {
        sqlite3_bind_int(t, 1, seq - 1);
        ok = done(t);

        if (ok) {
            id = (int)sqlite3_last_insert_rowid(DB);
        }
    }

    if (ok) {

        s = prep(
            "UPDATE files SET is_deleted=1, "
            "deleted_by_transaction_id=?, deleted_commit_seq=? "
            "WHERE is_deleted=0;"
        );

        if (!s) {
            ok = false;
        }
        else {
            sqlite3_bind_int(s, 1, id);
            sqlite3_bind_int(s, 2, seq);
            ok = done(s);
        }
    }

    for (auto &f : fs) {

        if (!ok) break;

        s = prep(
            "SELECT file_id, current_version "
            "FROM files WHERE file_name=?;"
        );

        if (!s) {
            ok = false;
            break;
        }

        bindText(s, 1, f.first);

        int fid = -1;
        int ver = 1;

        if (sqlite3_step(s) == SQLITE_ROW) {
            fid = sqlite3_column_int(s, 0);
            ver = sqlite3_column_int(s, 1) + 1;
        }

        sqlite3_finalize(s);

        if (fid < 0) {

            s = prep(
                "INSERT INTO files(file_name,current_version,is_deleted) "
                "VALUES(?,1,0);"
            );

            if (!s) {
                ok = false;
                break;
            }

            bindText(s, 1, f.first);

            ok = done(s);

            if (!ok) break;

            fid = (int)sqlite3_last_insert_rowid(DB);
            ver = 1;
        }
        else {

            s = prep(
                "UPDATE files SET current_version=?, is_deleted=0, "
                "deleted_by_transaction_id=NULL, deleted_commit_seq=NULL "
                "WHERE file_id=?;"
            );

            if (!s) {
                ok = false;
                break;
            }

            sqlite3_bind_int(s, 1, ver);
            sqlite3_bind_int(s, 2, fid);

            ok = done(s);

            if (!ok) break;
        }

        s = prep(
            "INSERT INTO file_versions("
            "file_id,version_number,content,transaction_id,is_committed,commit_seq"
            ") VALUES(?,?,?,?,1,?);"
        );

        if (!s) {
            ok = false;
            break;
        }

        sqlite3_bind_int(s, 1, fid);
        sqlite3_bind_int(s, 2, ver);
        bindText(s, 3, f.second);
        sqlite3_bind_int(s, 4, id);
        sqlite3_bind_int(s, 5, seq);

        ok = done(s);
    }

    if (ok) {

        s = prep(
            "UPDATE mvcc_metadata SET value=? WHERE key='commit_seq';"
        );

        if (!s) {
            ok = false;
        }
        else {
            sqlite3_bind_int(s, 1, seq);
            ok = done(s);
        }
    }

    if (ok) {

        s = prep(
            "UPDATE transactions SET status='COMMITTED', "
            "commit_seq=?, completed_at=CURRENT_TIMESTAMP "
            "WHERE transaction_id=?;"
        );

        if (!s) {
            ok = false;
        }
        else {
            sqlite3_bind_int(s, 1, seq);
            sqlite3_bind_int(s, 2, id);
            ok = done(s);
        }
    }

    if (ok) {
        ok = execSQL("COMMIT;");
    }

    if (!ok) {
        execSQL("ROLLBACK;");
        cout << "Snapshot restore failed; changes rolled back.\n";
    }
    else {
        cout << "Snapshot restored. Commit sequence: "
             << seq << "\n";
    }
}