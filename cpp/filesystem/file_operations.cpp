#include "file_operations.h"

#include "../common/app_state.h"
#include "../database/database.h"
#include "../transactions/transaction.h"
#include "../os/lock_manager.h"
#include "../memory/paging.h"


#include <iostream>
#include <string>
#include <vector>

using namespace std;


// CREATE FILE

void createFile() {
    if(!requireTx()) return;

    string name, content;
    cout << "Enter file name: ";
    getline(cin >> ws, name);

    cout << "Enter file content (type END on a new line to finish):\n";

    string line;

    while(getline(cin, line) && line != "END") {
        if(!content.empty())
            content += "\n";

        content += line;
    }

    sqlite3_stmt *s = prep(
        "SELECT file_id FROM files WHERE file_name=?;"
    );

    if(!s) return;

    bindText(s, 1, name);

    bool exists = sqlite3_step(s) == SQLITE_ROW;

    sqlite3_finalize(s);

    bool stagedExists = false;

    for(auto &c : staged[currentTransactionID]) {
        if(c.name == name && c.operation != "DELETE")
            stagedExists = true;
    }

    if(exists || stagedExists) {
        cout << "File name already exists.\n";
        return;
    }

    if(!acquireLock(currentTransactionID, name))
        return;

    staged[currentTransactionID].push_back(
        {name, "CREATE", content}
    );

    if(!persistChange(currentTransactionID,
                      staged[currentTransactionID].back())) {
        staged[currentTransactionID].pop_back();
        return;
    }

    logOp(currentTransactionID, "CREATE", name);

    cout << "File staged. It becomes visible to other transactions only after commit.\n";
}


// READ FILE


void readFile() {
    string name;

    
   

    cout << "Enter file name: ";
    getline(cin >> ws, name);

    if(currentTransactionID >= 0 && active(currentTransactionID)) {
        auto &v = staged[currentTransactionID];

        for(auto it = v.rbegin(); it != v.rend(); ++it) {
            if(it->name == name) {
                if(it->operation == "DELETE") {
                    cout << "File is deleted in this transaction.\n";
                }
                else {
                    cout << "Content (your staged change): "
                         << it->content << "\n";

                    // Load staged content into memory
                    pagingManager.loadFile(name, it->content);
                   pagingManager.displayFrames();
                  pagingManager.displayStatistics();
                }

                return;
            }
        }
    }

    sqlite3_stmt *s = prep(
        "SELECT f.file_id,f.is_deleted,fv.content,fv.version_number "
        "FROM files f JOIN file_versions fv ON fv.file_id=f.file_id "
        "WHERE f.file_name=? AND fv.is_committed=1 "
        "AND fv.version_number=(SELECT MAX(v.version_number) "
        "FROM file_versions v WHERE v.file_id=f.file_id "
        "AND v.is_committed=1) "
        "ORDER BY fv.version_number DESC LIMIT 1;"
    );

    if(!s) return;

    bindText(s, 1, name);

    if(sqlite3_step(s) == SQLITE_ROW) {
        if(sqlite3_column_int(s, 1)) {
            cout << "File is deleted.\n";
        }
        else {
            const unsigned char *c = sqlite3_column_text(s, 2);

            string content = c ? (const char*)c : "";

            cout << "Version: " << sqlite3_column_int(s, 3)
                 << "\nContent: " << content << "\n";

            // Load committed file content into memory
            pagingManager.loadFile(name, content);
               pagingManager.displayFrames();
              pagingManager.displayStatistics();
        }
    }
    else {
        cout << "File not found.\n";
    }

    sqlite3_finalize(s);
}

// UPDATE FILE

void updateFile() {
    if(!requireTx()) return;

    string name, content;

    cout << "Enter file name to update: ";
    getline(cin >> ws, name);

    cout << "Enter new content (type END on a new line to finish):\n";

    string line;

    while(getline(cin, line) && line != "END") {
        if(!content.empty())
            content += "\n";

        content += line;
    }

    bool exists = false;

    sqlite3_stmt *s = prep(
        "SELECT f.is_deleted FROM files f WHERE f.file_name=?;"
    );

    if(!s) return;

    bindText(s, 1, name);

    if(sqlite3_step(s) == SQLITE_ROW)
        exists = sqlite3_column_int(s, 0) == 0;

    sqlite3_finalize(s);

    for(auto &c : staged[currentTransactionID]) {
        if(c.name == name) {
            exists = c.operation != "DELETE";
            break;
        }
    }

    if(!exists) {
        cout << "File not found.\n";
        return;
    }

    if(!acquireLock(currentTransactionID, name))
        return;

    staged[currentTransactionID].push_back(
        {name, "UPDATE", content}
    );

    if(!persistChange(currentTransactionID,
                      staged[currentTransactionID].back())) {
        staged[currentTransactionID].pop_back();
        return;
    }

    logOp(currentTransactionID, "UPDATE", name);

    cout << "Update staged and saved durably.\n";
}
bool stageUpdateFile(
    const std::string &name,
    const std::string &content,
    std::string &message
)
{
    if (!requireTx()) {
        message = "Start a transaction first.";
        return false;
    }

    int fileId = -1;

    sqlite3_stmt *stmt = nullptr;

    const char *sql =
        "SELECT file_id FROM files "
        "WHERE file_name = ? AND is_deleted = 0;";

    if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        message = "Database error while finding file.";
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        fileId = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (fileId == -1) {
        message = "File not found.";
        return false;
    }

    if (!acquireLock(currentTransactionID, name)) {
        message = "Could not acquire file lock.";
        return false;
    }

    Change change = {name, "UPDATE", content};

    staged[currentTransactionID].push_back(change);

    if (!persistChange(currentTransactionID, change)) {
        staged[currentTransactionID].pop_back();
        message = "Could not save staged update.";
        return false;
    }

    logOp(currentTransactionID, "UPDATE", name);

    message = "File update staged successfully.";
    return true;
}


// DELETE FILE

void deleteFile() {
    if(!requireTx()) return;

    string name;

    cout << "Enter file name to delete: ";
    getline(cin >> ws, name);

    bool exists = false;

    sqlite3_stmt *s = prep(
        "SELECT is_deleted FROM files WHERE file_name=?;"
    );

    if(!s) return;

    bindText(s, 1, name);

    if(sqlite3_step(s) == SQLITE_ROW)
        exists = sqlite3_column_int(s, 0) == 0;

    sqlite3_finalize(s);

    for(auto &c : staged[currentTransactionID]) {
        if(c.name == name) {
            exists = c.operation != "DELETE";
            break;
        }
    }

    if(!exists) {
        cout << "File not found.\n";
        return;
    }

    if(!acquireLock(currentTransactionID, name))
        return;

    staged[currentTransactionID].push_back(
        {name, "DELETE", ""}
    );

    if(!persistChange(currentTransactionID,
                      staged[currentTransactionID].back())) {
        staged[currentTransactionID].pop_back();
        return;
    }

    logOp(currentTransactionID, "DELETE", name);

    cout << "Deletion staged and saved durably.\n";
}


// VIEW FILES

void viewFiles() {
    sqlite3_stmt *s = prep(
        "SELECT f.file_name, f.current_version "
        "FROM files f WHERE f.is_deleted=0 ORDER BY f.file_name;"
    );

    if(!s) return;

    cout << "\n--- Available committed files ---\n";

    bool found = false;

    while(sqlite3_step(s) == SQLITE_ROW) {
        found = true;

        cout << sqlite3_column_text(s, 0)
             << " | Version: "
             << sqlite3_column_int(s, 1)
             << "\n";
    }

    sqlite3_finalize(s);

    if(!found)
        cout << "No files found.\n";

    if(currentTransactionID >= 0 &&
       staged.count(currentTransactionID)) {

        cout << "\nYour staged operations:\n";

        for(auto &c : staged[currentTransactionID])
            cout << c.name << " | " << c.operation << "\n";
    }
}


// VIEW VERSION HISTORY

void viewVersionHistory() {
    string name;

    cout << "Enter file name: ";
    getline(cin >> ws, name);

    sqlite3_stmt *s = prep(
        "SELECT fv.version_number,fv.content,fv.transaction_id,"
        "fv.is_committed,fv.commit_seq "
        "FROM files f JOIN file_versions fv ON f.file_id=fv.file_id "
        "WHERE f.file_name=? ORDER BY fv.version_number;"
    );

    if(!s) return;

    bindText(s, 1, name);

    bool found = false;

    cout << "\n--- Version History ---\n";

    while(sqlite3_step(s) == SQLITE_ROW) {
        found = true;

        cout << "Version: " << sqlite3_column_int(s, 0)
             << " | Content: "
             << (const char*)sqlite3_column_text(s, 1)
             << " | Transaction: ";

        if(sqlite3_column_type(s, 2) == SQLITE_NULL)
            cout << "None";
        else
            cout << sqlite3_column_int(s, 2);

        cout << " | Status: "
             << (sqlite3_column_int(s, 3) ? "COMMITTED" : "UNCOMMITTED")
             << " | Commit sequence: ";

        if(sqlite3_column_type(s, 4) == SQLITE_NULL)
            cout << "None";
        else
            cout << sqlite3_column_int(s, 4);

        cout << "\n";
    }

    sqlite3_finalize(s);

    if(!found)
        cout << "No versions found.\n";
}


// VIEW TRANSACTION HISTORY

void viewTransactions() {
    sqlite3_stmt *s = prep(
        "SELECT transaction_id,status,start_seq,commit_seq,"
        "started_at,completed_at "
        "FROM transactions ORDER BY transaction_id;"
    );

    if(!s) return;

    cout << "\n--- Transaction History ---\n";

    while(sqlite3_step(s) == SQLITE_ROW) {
        cout << "ID: " << sqlite3_column_int(s, 0)
             << " | Status: " << sqlite3_column_text(s, 1)
             << " | Start seq: " << sqlite3_column_int(s, 2)
             << " | Commit seq: ";

        if(sqlite3_column_type(s, 3) == SQLITE_NULL)
            cout << "None";
        else
            cout << sqlite3_column_int(s, 3);

        cout << " | Started: "
             << (sqlite3_column_text(s, 4)
                 ? (const char*)sqlite3_column_text(s, 4) : "")
             << " | Completed: "
             << (sqlite3_column_text(s, 5)
                 ? (const char*)sqlite3_column_text(s, 5) : "-")
             << "\n";
    }

    sqlite3_finalize(s);
}