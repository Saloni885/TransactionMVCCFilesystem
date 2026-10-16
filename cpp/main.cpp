#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sqlite3.h>
#include<queue>
#include<set>
#include<functional>
#include <sstream>
using namespace std;
void abortTransaction();

sqlite3 *DB = nullptr;
int currentTransactionID = -1;
struct Change { string name, operation, content; };
map<int, vector<Change>> staged;
struct FileLock {
    string fileName;
    int transactionID;
};

struct LockRequest {
    string fileName;
    int transactionID;
};

vector<FileLock> lockTable;
queue<LockRequest> waitingQueue;

void detectDeadlock()
{
    cout << "\n===== DEADLOCK DETECTION =====\n";

    map<int, vector<int>> graph;

    // Build the Wait-For Graph from waiting requests and lock owners.
    queue<LockRequest> temp = waitingQueue;

    while(!temp.empty())
    {
        LockRequest request = temp.front();
        temp.pop();

        for(const auto &lock : lockTable)
        {
            if(lock.fileName == request.fileName &&
               lock.transactionID != request.transactionID)
            {
                graph[request.transactionID].push_back(
                    lock.transactionID
                );
            }
        }
    }

    // 0 = unvisited, 1 = currently visiting, 2 = completed
    map<int, int> color;
    vector<int> path;
    vector<int> cycle;

    function<bool(int)> dfs = [&](int node)
    {
        color[node] = 1;
        path.push_back(node);

        for(int next : graph[node])
        {
            if(color[next] == 0)
            {
                if(dfs(next))
                    return true;
            }
            else if(color[next] == 1)
            {
                // A back edge means a cycle exists.
                auto it = find(path.begin(), path.end(), next);

                if(it != path.end())
                {
                    cycle.assign(it, path.end());
                    cycle.push_back(next);
                }

                return true;
            }
        }

        path.pop_back();
        color[node] = 2;

        return false;
    };

    bool deadlock = false;

    for(const auto &entry : graph)
    {
        if(color[entry.first] == 0)
        {
            if(dfs(entry.first))
            {
                deadlock = true;
                break;
            }
        }
    }

    if(deadlock)
    {
        cout << "Deadlock detected!\n";
        cout << "Cycle: ";

        for(int i = 0; i < (int)cycle.size(); i++)
        {
            cout << "T" << cycle[i];

            if(i + 1 < (int)cycle.size())
                cout << " -> ";
        }

        cout << "\n";
    }
    else
    {
        cout << "No deadlock detected.\n";
    }
}

bool acquireLock(int transactionID, const string &fileName)
{
    // The transaction already owns this file's lock.
    for (const auto &lock : lockTable) {
        if (lock.fileName == fileName &&
            lock.transactionID == transactionID) {
            return true;
        }
    }

    // Another transaction owns the lock.
    for (const auto &lock : lockTable) {
        if (lock.fileName == fileName &&
            lock.transactionID != transactionID) {

            // Avoid adding the same request repeatedly.
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
    // 1. Release all locks owned by this transaction
    for(auto it = lockTable.begin(); it != lockTable.end();)
    {
        if(it->transactionID == transactionID)
        {
            cout << "Released lock on " << it->fileName
                 << " from transaction " << transactionID << ".\n";

            it = lockTable.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 2. Remove this transaction's requests from the waiting queue
    queue<LockRequest> remaining;

    while(!waitingQueue.empty())
    {
        LockRequest request = waitingQueue.front();
        waitingQueue.pop();

        if(request.transactionID != transactionID)
        {
            remaining.push(request);
        }
    }

    waitingQueue = remaining;

    cout << "Checking waiting transactions...\n";

    // 3. Process requests that were waiting when this function started
    int count = (int)waitingQueue.size();

    while(count--)
    {
        LockRequest request = waitingQueue.front();
        waitingQueue.pop();

        // Check whether the waiting transaction is still ACTIVE
        sqlite3_stmt *s = nullptr;

        if(sqlite3_prepare_v2(
               DB,
               "SELECT status FROM transactions WHERE transaction_id=?;",
               -1, &s, nullptr) != SQLITE_OK)
        {
            waitingQueue.push(request);
            continue;
        }

        sqlite3_bind_int(s, 1, request.transactionID);

        bool isActive = false;

        if(sqlite3_step(s) == SQLITE_ROW)
        {
            const unsigned char *status = sqlite3_column_text(s, 0);

            if(status != nullptr &&
               string(reinterpret_cast<const char*>(status)) == "ACTIVE")
            {
                isActive = true;
            }
        }

        sqlite3_finalize(s);

        // Skip transactions that have already ended
        if(!isActive)
        {
            cout << "Removed inactive transaction "
                 << request.transactionID
                 << " from the waiting queue.\n";

            continue;
        }

        // 4. Check whether the requested file is still locked
        bool occupied = false;

        for(const auto &lock : lockTable)
        {
            if(lock.fileName == request.fileName)
            {
                occupied = true;
                break;
            }
        }

        // 5. Grant the lock if the file is available
        if(!occupied)
        {
            lockTable.push_back(
                {request.fileName, request.transactionID}
            );

            cout << "Transaction " << request.transactionID
                 << " acquired the lock on "
                 << request.fileName
                 << ". Retry its waiting operation.\n";
        }
        else
        {
            // File is still locked; keep the request waiting
            waitingQueue.push(request);
        }
    }
}

void recoverDeadlock()
{
    cout << "\n===== DEADLOCK RECOVERY =====\n";

    map<int, vector<int>> graph;

    // Build the Wait-For Graph
    queue<LockRequest> temp = waitingQueue;

    while (!temp.empty())
    {
        LockRequest request = temp.front();
        temp.pop();

        for (const auto &lock : lockTable)
        {
            if (lock.fileName == request.fileName &&
                lock.transactionID != request.transactionID)
            {
                graph[request.transactionID].push_back(
                    lock.transactionID
                );
            }
        }
    }

    map<int, int> color;
    vector<int> path;
    vector<int> cycle;

    // Find a cycle using DFS
    function<bool(int)> dfs = [&](int node)
    {
        color[node] = 1;
        path.push_back(node);

        for (int next : graph[node])
        {
            if (color[next] == 0)
            {
                if (dfs(next))
                    return true;
            }
            else if (color[next] == 1)
            {
                auto it = find(path.begin(), path.end(), next);
                cycle.assign(it, path.end());
                cycle.push_back(next);
                return true;
            }
        }

        path.pop_back();
        color[node] = 2;
        return false;
    };

    bool found = false;

    for (const auto &entry : graph)
    {
        if (color[entry.first] == 0)
        {
            if (dfs(entry.first))
            {
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        cout << "No deadlock found. Recovery not required.\n";
        return;
    }

    // Select the first transaction in the cycle as victim
    int victim = cycle[0];

    cout << "Deadlock cycle: ";

    for (size_t i = 0; i < cycle.size(); i++)
    {
        cout << "T" << cycle[i];

        if (i + 1 < cycle.size())
            cout << " -> ";
    }

    cout << "\nVictim transaction: T" << victim << "\n";

    // Preserve the currently selected transaction
    int previousTransactionID = currentTransactionID;

    // Abort the victim using the existing function
    currentTransactionID = victim;
    abortTransaction();

    // Restore the previous selection if it is a different transaction
    if (previousTransactionID != victim)
        currentTransactionID = previousTransactionID;
}

void viewWaitForGraph()
{
    cout << "\n===== WAIT-FOR GRAPH =====\n";

    bool found = false;

    queue<LockRequest> temp = waitingQueue;

    while(!temp.empty())
    {
        LockRequest request = temp.front();
        temp.pop();

        for(const auto &lock : lockTable)
        {
            if(lock.fileName == request.fileName &&
               lock.transactionID != request.transactionID)
            {
                cout << "T" << request.transactionID
                     << " -> T" << lock.transactionID
                     << " (waiting for " << request.fileName << ")\n";

                found = true;
            }
        }
    }

    if(!found)
    {
        cout << "No waiting edges in the graph.\n";
    }
}

void viewLocks()
{
    cout << "\n--- Lock Table ---\n";

    if (lockTable.empty()) {
        cout << "No locks held.\n";
    } else {
        for (const auto &lock : lockTable) {
            cout << "File: " << lock.fileName
                 << " | Owner: T" << lock.transactionID << "\n";
        }
    }

    cout << "\n--- Waiting Queue ---\n";

    if (waitingQueue.empty()) {
        cout << "No waiting transactions.\n";
    } else {
        queue<LockRequest> temp = waitingQueue;

        while (!temp.empty()) {
            LockRequest request = temp.front();
            temp.pop();

            cout << "T" << request.transactionID
                 << " waiting for " << request.fileName << "\n";
        }
    }
}

bool execSQL(const string &sql) {
    char *err = nullptr;
    int rc = sqlite3_exec(DB, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) { cout << "SQL Error: " << (err ? err : sqlite3_errmsg(DB)) << endl; sqlite3_free(err); return false; }
    return true;
}
sqlite3_stmt *prep(const string &sql) {
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(DB, sql.c_str(), -1, &s, nullptr) != SQLITE_OK) { cout << "SQL Error: " << sqlite3_errmsg(DB) << endl; return nullptr; }
    return s;
}
bool done(sqlite3_stmt *s) { bool ok = sqlite3_step(s) == SQLITE_DONE; if (!ok) cout << "SQL Error: " << sqlite3_errmsg(DB) << endl; sqlite3_finalize(s); return ok; }
void bindText(sqlite3_stmt *s, int i, const string &v) { sqlite3_bind_text(s, i, v.c_str(), -1, SQLITE_TRANSIENT); }

bool persistChange(int id, const Change &c) {
    sqlite3_stmt *s = prep("INSERT INTO transaction_changes(transaction_id, sequence_no, file_name, operation, content) VALUES(?,?,?,?,?);");
    if (!s) return false;
    sqlite3_bind_int(s, 1, id);
    sqlite3_bind_int(s, 2, (int)staged[id].size());
    bindText(s, 3, c.name);
    bindText(s, 4, c.operation);
    bindText(s, 5, c.content);
    return done(s);
}

void loadPersistedChanges() {
    sqlite3_stmt *s = prep("SELECT tc.transaction_id, tc.file_name, tc.operation, tc.content FROM transaction_changes tc JOIN transactions t ON t.transaction_id=tc.transaction_id WHERE t.status='ACTIVE' ORDER BY tc.transaction_id, tc.sequence_no;");
    if (!s) return;
    while (sqlite3_step(s) == SQLITE_ROW) {
        int id = sqlite3_column_int(s, 0);
        const unsigned char *name = sqlite3_column_text(s, 1);
        const unsigned char *op = sqlite3_column_text(s, 2);
        const unsigned char *content = sqlite3_column_text(s, 3);
        staged[id].push_back({name ? (const char*)name : "", op ? (const char*)op : "", content ? (const char*)content : ""});
    }
    sqlite3_finalize(s);
    if (!staged.empty()) cout << "Recovered staged changes for " << staged.size() << " active transaction(s). Select a transaction to resume.\n";
}


bool active(int id) {
    sqlite3_stmt *s = prep("SELECT 1 FROM transactions WHERE transaction_id=? AND status='ACTIVE';");
    if (!s) return false; sqlite3_bind_int(s,1,id); bool yes=sqlite3_step(s)==SQLITE_ROW; sqlite3_finalize(s); return yes;
}
bool requireTx() {
    if (currentTransactionID < 0 || !active(currentTransactionID)) { cout << "Select an active transaction first.\n"; currentTransactionID=-1; return false; }
    return true;
}
int latestCommitSeq() {
    sqlite3_stmt *s=prep("SELECT value FROM mvcc_metadata WHERE key='commit_seq';"); if(!s) return 0;
    int v=0; if(sqlite3_step(s)==SQLITE_ROW) v=sqlite3_column_int(s,0); sqlite3_finalize(s); return v;
}
void beginTransaction() {
    sqlite3_stmt *s=prep("INSERT INTO transactions(status,start_seq) VALUES('ACTIVE',?);"); if(!s) return;
    sqlite3_bind_int(s,1,latestCommitSeq()); if(!done(s)) return;
    int id=(int)sqlite3_last_insert_rowid(DB); staged[id]={}; currentTransactionID=id;
    cout << "Logical transaction started. ID: " << id << "\nStart sequence: " << latestCommitSeq() << "\n";
}
void selectTransaction() {
    sqlite3_stmt *s=prep("SELECT transaction_id FROM transactions WHERE status='ACTIVE' ORDER BY transaction_id;"); if(!s)return;
    vector<int> ids; cout << "\n--- Active Transactions ---\n";
    while(sqlite3_step(s)==SQLITE_ROW){int id=sqlite3_column_int(s,0);ids.push_back(id);cout<<"Transaction ID: "<<id<<(id==currentTransactionID?" (selected)":"")<<"\n";} sqlite3_finalize(s);
    if(ids.empty()){cout<<"No active transactions.\n";currentTransactionID=-1;return;}
    int id; cout<<"Enter transaction ID: "; if(!(cin>>id)){cin.clear();cin.ignore(10000,'\n');cout<<"Invalid input.\n";return;}
    if(find(ids.begin(),ids.end(),id)!=ids.end()){currentTransactionID=id;cout<<"Selected transaction "<<id<<".\n";}else cout<<"That transaction is not active.\n";
}
void logOp(int id,const string &op,const string &name) {
    sqlite3_stmt *s=prep("INSERT INTO system_logs(transaction_id,operation,details) VALUES(?,?,?);"); if(!s)return;
    sqlite3_bind_int(s,1,id);bindText(s,2,op);bindText(s,3,name);done(s);
}
void createFile() {
    if(!requireTx())return; string name,content; cout<<"Enter file name: ";getline(cin>>ws,name);cout<<"Enter file content: ";getline(cin,content);
    sqlite3_stmt *s=prep("SELECT file_id FROM files WHERE file_name=?;");if(!s)return;bindText(s,1,name);bool exists=sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);
    bool stagedExists=false; for(auto &c:staged[currentTransactionID])if(c.name==name&&c.operation!="DELETE")stagedExists=true;
    if(exists||stagedExists){cout<<"File name already exists.\n";return;}
     if(!acquireLock(currentTransactionID, name))
        return;
    staged[currentTransactionID].push_back({name,"CREATE",content});if(!persistChange(currentTransactionID, staged[currentTransactionID].back())){staged[currentTransactionID].pop_back();return;}logOp(currentTransactionID,"CREATE",name);cout<<"File staged. It becomes visible to other transactions only after commit.\n";
}
void readFile() {
    string name;cout<<"Enter file name: ";getline(cin>>ws,name);
    if(currentTransactionID>=0&&active(currentTransactionID)){
        auto &v=staged[currentTransactionID];for(auto it=v.rbegin();it!=v.rend();++it)if(it->name==name){if(it->operation=="DELETE")cout<<"File is deleted in this transaction.\n";else cout<<"Content (your staged change): "<<it->content<<"\n";return;}
    }
    sqlite3_stmt *s=prep("SELECT f.file_id,f.is_deleted,fv.content,fv.version_number FROM files f JOIN file_versions fv ON fv.file_id=f.file_id WHERE f.file_name=? AND fv.is_committed=1 AND fv.version_number=(SELECT MAX(v.version_number) FROM file_versions v WHERE v.file_id=f.file_id AND v.is_committed=1) ORDER BY fv.version_number DESC LIMIT 1;");if(!s)return;bindText(s,1,name);
    if(sqlite3_step(s)==SQLITE_ROW){if(sqlite3_column_int(s,1))cout<<"File is deleted.\n";else {const unsigned char *c=sqlite3_column_text(s,2);cout<<"Version: "<<sqlite3_column_int(s,3)<<"\nContent: "<<(c?(const char*)c:"")<<"\n";}}else cout<<"File not found.\n";sqlite3_finalize(s);
}
void updateFile() {
    if(!requireTx())return;string name,content;cout<<"Enter file name to update: ";getline(cin>>ws,name);cout<<"Enter new content: ";getline(cin,content);
    bool exists=false;sqlite3_stmt *s=prep("SELECT f.is_deleted FROM files f WHERE f.file_name=?;");if(!s)return;bindText(s,1,name);if(sqlite3_step(s)==SQLITE_ROW)exists=sqlite3_column_int(s,0)==0;sqlite3_finalize(s);
    for(auto &c:staged[currentTransactionID])if(c.name==name){exists=c.operation!="DELETE";break;}
    if(!exists){cout<<"File not found.\n";return;}
    if(!acquireLock(currentTransactionID, name))return;
    staged[currentTransactionID].push_back({name,"UPDATE",content});if(!persistChange(currentTransactionID, staged[currentTransactionID].back())){staged[currentTransactionID].pop_back();return;}logOp(currentTransactionID,"UPDATE",name);cout<<"Update staged and saved durably.\n";
}
void deleteFile() {
    if(!requireTx())return;string name;cout<<"Enter file name to delete: ";getline(cin>>ws,name);
    bool exists=false;sqlite3_stmt *s=prep("SELECT is_deleted FROM files WHERE file_name=?;");if(!s)return;bindText(s,1,name);if(sqlite3_step(s)==SQLITE_ROW)exists=sqlite3_column_int(s,0)==0;sqlite3_finalize(s);
    for(auto &c:staged[currentTransactionID])if(c.name==name){exists=c.operation!="DELETE";break;}
    if(!exists){cout<<"File not found.\n";return;}
    if(!acquireLock(currentTransactionID, name))return;
    staged[currentTransactionID].push_back({name,"DELETE",""});if(!persistChange(currentTransactionID, staged[currentTransactionID].back())){staged[currentTransactionID].pop_back();return;}logOp(currentTransactionID,"DELETE",name);cout<<"Deletion staged and saved durably.\n";
}
bool applyChange(int id,const Change &c,int seq) {
    sqlite3_stmt *s=nullptr;
    if(c.operation=="CREATE"){
        s=prep("INSERT INTO files(file_name,current_version,is_deleted) VALUES(?,1,0);");if(!s)return false;bindText(s,1,c.name);if(!done(s))return false;
        int fid=(int)sqlite3_last_insert_rowid(DB);s=prep("INSERT INTO file_versions(file_id,version_number,content,transaction_id,is_committed,commit_seq) VALUES(?,1,?,?,1,?);");if(!s)return false;sqlite3_bind_int(s,1,fid);bindText(s,2,c.content);sqlite3_bind_int(s,3,id);sqlite3_bind_int(s,4,seq);return done(s);
    }
    s=prep("SELECT file_id,current_version,is_deleted FROM files WHERE file_name=?;");if(!s)return false;bindText(s,1,c.name);if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);cout<<"Commit conflict: file '"<<c.name<<"' no longer exists.\n";return false;}
    int fid=sqlite3_column_int(s,0),ver=sqlite3_column_int(s,1)+1;sqlite3_finalize(s);
    if(c.operation=="DELETE") {s=prep("UPDATE files SET is_deleted=1,deleted_by_transaction_id=?,deleted_commit_seq=? WHERE file_id=?;");if(!s)return false;sqlite3_bind_int(s,1,id);sqlite3_bind_int(s,2,seq);sqlite3_bind_int(s,3,fid);return done(s);}
    s=prep("INSERT INTO file_versions(file_id,version_number,content,transaction_id,is_committed,commit_seq) VALUES(?,?,?,?,1,?);");if(!s)return false;sqlite3_bind_int(s,1,fid);sqlite3_bind_int(s,2,ver);bindText(s,3,c.content);sqlite3_bind_int(s,4,id);sqlite3_bind_int(s,5,seq);if(!done(s))return false;
    s=prep("UPDATE files SET current_version=?,is_deleted=0,deleted_by_transaction_id=NULL,deleted_commit_seq=NULL WHERE file_id=?;");if(!s)return false;sqlite3_bind_int(s,1,ver);sqlite3_bind_int(s,2,fid);return done(s);
}
bool hasWriteConflict(int id, const vector<Change> &changes) {
    sqlite3_stmt *tx = prep("SELECT start_seq FROM transactions WHERE transaction_id=? AND status='ACTIVE';");
    if (!tx) return true;
    sqlite3_bind_int(tx, 1, id);
    if (sqlite3_step(tx) != SQLITE_ROW) { sqlite3_finalize(tx); return true; }
    int startSeq = sqlite3_column_int(tx, 0);
    sqlite3_finalize(tx);

    // Check each filename once. A committed version newer than this
    // transaction's start sequence means another transaction changed it.
    vector<string> checked;
    for (const auto &c : changes) {
        if (find(checked.begin(), checked.end(), c.name) != checked.end()) continue;
        checked.push_back(c.name);

        sqlite3_stmt *f = prep("SELECT file_id FROM files WHERE file_name=?;");
        if (!f) return true;
        bindText(f, 1, c.name);
        int fileId = -1;
        if (sqlite3_step(f) == SQLITE_ROW) fileId = sqlite3_column_int(f, 0);
        sqlite3_finalize(f);

        if (c.operation == "CREATE") {
            // Creating a name that appeared after this transaction started conflicts.
            if (fileId >= 0) {
                sqlite3_stmt *v = prep("SELECT MAX(commit_seq) FROM file_versions WHERE file_id=? AND is_committed=1;");
                if (!v) return true;
                sqlite3_bind_int(v, 1, fileId);
                int latest = 0;
                if (sqlite3_step(v) == SQLITE_ROW && sqlite3_column_type(v,0) != SQLITE_NULL)
                    latest = sqlite3_column_int(v,0);
                sqlite3_finalize(v);
                if (latest > startSeq) {
                    cout << "Write conflict: file name '" << c.name << "' was created after transaction " << id << " started.\n";
                    return true;
                }
            }
            continue;
        }

        if (fileId < 0) {
            cout << "Write conflict: file '" << c.name << "' no longer exists.\n";
            return true;
        }
        sqlite3_stmt *v = prep("SELECT MAX(commit_seq) FROM file_versions WHERE file_id=? AND is_committed=1;");
        if (!v) return true;
        sqlite3_bind_int(v, 1, fileId);
        int latest = 0;
        if (sqlite3_step(v) == SQLITE_ROW && sqlite3_column_type(v,0) != SQLITE_NULL)
            latest = sqlite3_column_int(v,0);
        sqlite3_finalize(v);
        if (latest > startSeq) {
            cout << "Write conflict: file '" << c.name << "' changed after transaction " << id << " started. Abort this transaction and retry.\n";
            return true;
        }
    }
    return false;
}

void commitTransaction() {
    if(!requireTx())return;int id=currentTransactionID;int seq=latestCommitSeq()+1;
    if(!execSQL("BEGIN IMMEDIATE;"))return;bool ok=true;
    if (hasWriteConflict(id, staged[id])) {
        execSQL("ROLLBACK;");
        cout << "Transaction " << id << " was not committed because of a write conflict. Its staged changes are retained; abort it before retrying.\n";
        return;
    }
    for(const auto &c:staged[id])if(!applyChange(id,c,seq)){ok=false;break;}
    if(ok){sqlite3_stmt *s=prep("UPDATE mvcc_metadata SET value=? WHERE key='commit_seq';");if(!s)ok=false;else{sqlite3_bind_int(s,1,seq);ok=done(s);}}
    if(ok){sqlite3_stmt *s=prep("UPDATE transactions SET status='COMMITTED',commit_seq=?,completed_at=CURRENT_TIMESTAMP WHERE transaction_id=? AND status='ACTIVE';");if(!s)ok=false;else{sqlite3_bind_int(s,1,seq);sqlite3_bind_int(s,2,id);ok=done(s);}}
    if(ok){sqlite3_stmt *d=prep("DELETE FROM transaction_changes WHERE transaction_id=?;");if(!d)ok=false;else{sqlite3_bind_int(d,1,id);ok=done(d);}}
    if(ok)ok=execSQL("COMMIT;");if(!ok){execSQL("ROLLBACK;");cout<<"Commit failed; staged changes retained.\n";return;}
    releaseLocks(id);
    staged.erase(id);currentTransactionID=-1;cout<<"Transaction "<<id<<" committed. Commit sequence: "<<seq<<"\n";
}

void abortTransaction() {
    if(!requireTx()) return;

    int id = currentTransactionID;

    sqlite3_stmt *s = prep(
        "UPDATE transactions SET status='ABORTED',"
        "completed_at=CURRENT_TIMESTAMP "
        "WHERE transaction_id=? AND status='ACTIVE';"
    );

    if(!s) return;

    sqlite3_bind_int(s, 1, id);

    if(done(s)) {
        sqlite3_stmt *d = prep(
            "DELETE FROM transaction_changes WHERE transaction_id=?;"
        );

        if(!d) {
            cout << "Could not clear saved changes.\n";
            return;
        }

        sqlite3_bind_int(d, 1, id);

        if(!done(d)) {
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

void viewFiles() {
    sqlite3_stmt *s=prep("SELECT f.file_name, f.current_version FROM files f WHERE f.is_deleted=0 ORDER BY f.file_name;");if(!s)return;cout<<"\n--- Available committed files ---\n";bool found=false;while(sqlite3_step(s)==SQLITE_ROW){found=true;cout<<sqlite3_column_text(s,0)<<" | Version: "<<sqlite3_column_int(s,1)<<"\n";}sqlite3_finalize(s);if(!found)cout<<"No files found.\n";
    if(currentTransactionID>=0&&staged.count(currentTransactionID)){cout<<"\nYour staged operations:\n";for(auto &c:staged[currentTransactionID])cout<<c.name<<" | "<<c.operation<<"\n";}
}
void viewVersionHistory() {
    string name;cout<<"Enter file name: ";getline(cin>>ws,name);sqlite3_stmt *s=prep("SELECT fv.version_number,fv.content,fv.transaction_id,fv.is_committed,fv.commit_seq FROM files f JOIN file_versions fv ON f.file_id=fv.file_id WHERE f.file_name=? ORDER BY fv.version_number;");if(!s)return;bindText(s,1,name);bool found=false;cout<<"\n--- Version History ---\n";while(sqlite3_step(s)==SQLITE_ROW){found=true;cout<<"Version: "<<sqlite3_column_int(s,0)<<" | Content: "<<(const char*)sqlite3_column_text(s,1)<<" | Transaction: ";if(sqlite3_column_type(s,2)==SQLITE_NULL)cout<<"None";else cout<<sqlite3_column_int(s,2);cout<<" | Status: "<<(sqlite3_column_int(s,3)?"COMMITTED":"UNCOMMITTED")<<" | Commit sequence: ";if(sqlite3_column_type(s,4)==SQLITE_NULL)cout<<"None";else cout<<sqlite3_column_int(s,4);cout<<"\n";}sqlite3_finalize(s);if(!found)cout<<"No versions found.\n";
}
void viewTransactions() {
    sqlite3_stmt *s=prep("SELECT transaction_id,status,start_seq,commit_seq,started_at,completed_at FROM transactions ORDER BY transaction_id;");if(!s)return;cout<<"\n--- Transaction History ---\n";while(sqlite3_step(s)==SQLITE_ROW){cout<<"ID: "<<sqlite3_column_int(s,0)<<" | Status: "<<sqlite3_column_text(s,1)<<" | Start seq: "<<sqlite3_column_int(s,2)<<" | Commit seq: ";if(sqlite3_column_type(s,3)==SQLITE_NULL)cout<<"None";else cout<<sqlite3_column_int(s,3);cout<<" | Started: "<<(sqlite3_column_text(s,4)?(const char*)sqlite3_column_text(s,4):"")<<" | Completed: "<<(sqlite3_column_text(s,5)?(const char*)sqlite3_column_text(s,5):"-")<<"\n";}sqlite3_finalize(s);
}
void createSnapshot() {
    if(sqlite3_stmt *x=prep("SELECT COUNT(*) FROM transactions WHERE status='ACTIVE';")){bool activeTx=sqlite3_step(x)==SQLITE_ROW&&sqlite3_column_int(x,0)>0;sqlite3_finalize(x);if(activeTx){cout<<"Commit or abort all active transactions before snapshot.\n";return;}}
    string name;cout<<"Enter snapshot name: ";getline(cin>>ws,name);string data;
    sqlite3_stmt *s=prep("SELECT f.file_name,fv.content FROM files f JOIN file_versions fv ON fv.file_id=f.file_id WHERE f.is_deleted=0 AND fv.is_committed=1 AND fv.version_number=(SELECT MAX(v.version_number) FROM file_versions v WHERE v.file_id=f.file_id AND v.is_committed=1) ORDER BY f.file_name;");if(!s)return;vector<pair<string,string>> fs;while(sqlite3_step(s)==SQLITE_ROW)fs.push_back({(const char*)sqlite3_column_text(s,0),(const char*)sqlite3_column_text(s,1)});sqlite3_finalize(s);
    auto field=[&](const string &v){data+=to_string(v.size())+":"+v;};field(to_string(fs.size()));for(auto &f:fs){field(f.first);field(f.second);}s=prep("INSERT INTO snapshots(snapshot_name,state_data) VALUES(?,?);");if(!s)return;bindText(s,1,name);sqlite3_bind_text(s,2,data.data(),(int)data.size(),SQLITE_TRANSIENT);if(done(s))cout<<"Snapshot created: "<<name<<" ("<<fs.size()<<" files)\n";
}
void viewSnapshots() {sqlite3_stmt *s=prep("SELECT snapshot_name,created_at FROM snapshots ORDER BY snapshot_id;");if(!s)return;cout<<"\n--- Snapshots ---\n";while(sqlite3_step(s)==SQLITE_ROW)cout<<sqlite3_column_text(s,0)<<" | "<<(sqlite3_column_text(s,1)?(const char*)sqlite3_column_text(s,1):"")<<"\n";sqlite3_finalize(s);}
bool readField(const string &d,size_t &p,string &v){size_t c=d.find(':',p);if(c==string::npos)return false;size_t n=0;try{n=stoull(d.substr(p,c-p));}catch(...){return false;}p=c+1;if(n>d.size()-p)return false;v=d.substr(p,n);p+=n;return true;}
void restoreSnapshot() {
    sqlite3_stmt *cnt=prep("SELECT COUNT(*) FROM transactions WHERE status='ACTIVE';");if(!cnt)return;bool has=sqlite3_step(cnt)==SQLITE_ROW&&sqlite3_column_int(cnt,0)>0;sqlite3_finalize(cnt);if(has){cout<<"Commit or abort all active transactions first.\n";return;}
    string name;cout<<"Enter snapshot name: ";getline(cin>>ws,name);sqlite3_stmt *s=prep("SELECT state_data FROM snapshots WHERE snapshot_name=?;");if(!s)return;bindText(s,1,name);if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);cout<<"Snapshot not found.\n";return;}string d=(const char*)sqlite3_column_text(s,0);sqlite3_finalize(s);size_t p=0;string ntext;if(!readField(d,p,ntext)){cout<<"Invalid snapshot data.\n";return;}size_t n;try{n=stoull(ntext);}catch(...){cout<<"Invalid snapshot data.\n";return;}vector<pair<string,string>> fs;for(size_t i=0;i<n;i++){string a,b;if(!readField(d,p,a)||!readField(d,p,b)){cout<<"Invalid snapshot data.\n";return;}fs.push_back({a,b});}
    if(!execSQL("BEGIN IMMEDIATE;"))return;bool ok=true;int seq=latestCommitSeq()+1;sqlite3_stmt *t=prep("INSERT INTO transactions(status,start_seq) VALUES('ACTIVE',?);");if(!t)ok=false;int id=-1;if(ok){sqlite3_bind_int(t,1,seq-1);ok=done(t);if(ok)id=(int)sqlite3_last_insert_rowid(DB);}if(ok){s=prep("UPDATE files SET is_deleted=1,deleted_by_transaction_id=?,deleted_commit_seq=? WHERE is_deleted=0;");if(!s)ok=false;else{sqlite3_bind_int(s,1,id);sqlite3_bind_int(s,2,seq);ok=done(s);}}
    for(auto &f:fs){if(!ok)break;s=prep("SELECT file_id,current_version FROM files WHERE file_name=?;");if(!s){ok=false;break;}bindText(s,1,f.first);int fid=-1,ver=1;if(sqlite3_step(s)==SQLITE_ROW){fid=sqlite3_column_int(s,0);ver=sqlite3_column_int(s,1)+1;}sqlite3_finalize(s);if(fid<0){s=prep("INSERT INTO files(file_name,current_version,is_deleted) VALUES(?,1,0);");if(!s){ok=false;break;}bindText(s,1,f.first);ok=done(s);if(!ok)break;fid=(int)sqlite3_last_insert_rowid(DB);ver=1;}else{s=prep("UPDATE files SET current_version=?,is_deleted=0,deleted_by_transaction_id=NULL,deleted_commit_seq=NULL WHERE file_id=?;");if(!s){ok=false;break;}sqlite3_bind_int(s,1,ver);sqlite3_bind_int(s,2,fid);ok=done(s);if(!ok)break;}s=prep("INSERT INTO file_versions(file_id,version_number,content,transaction_id,is_committed,commit_seq) VALUES(?,?,?,?,1,?);");if(!s){ok=false;break;}sqlite3_bind_int(s,1,fid);sqlite3_bind_int(s,2,ver);bindText(s,3,f.second);sqlite3_bind_int(s,4,id);sqlite3_bind_int(s,5,seq);ok=done(s);}
    if(ok){s=prep("UPDATE mvcc_metadata SET value=? WHERE key='commit_seq';");if(!s)ok=false;else{sqlite3_bind_int(s,1,seq);ok=done(s);}}if(ok){s=prep("UPDATE transactions SET status='COMMITTED',commit_seq=?,completed_at=CURRENT_TIMESTAMP WHERE transaction_id=?;");if(!s)ok=false;else{sqlite3_bind_int(s,1,seq);sqlite3_bind_int(s,2,id);ok=done(s);}}if(ok)ok=execSQL("COMMIT;");if(!ok){execSQL("ROLLBACK;");cout<<"Snapshot restore failed; changes rolled back.\n";}else cout<<"Snapshot restored. Commit sequence: "<<seq<<"\n";
}

string jsonEscape(const string &value) {
    string out;
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c >= 32) out += (char)c;
    }
    return out;
}
void guiState() {
    cout << "{\"ok\":true,\"locks\":[";
    for (size_t i=0; i<lockTable.size(); ++i) {
        if(i) cout << ",";
        cout << "{\"file\":\"" << jsonEscape(lockTable[i].fileName)
             << "\",\"transaction_id\":" << lockTable[i].transactionID << "}";
    }
    cout << "],\"waiting\":[";
    queue<LockRequest> q=waitingQueue;
    bool first=true;
    while(!q.empty()) {
        auto r=q.front(); q.pop();
        if(!first) cout << ",";
        first=false;
        cout << "{\"file\":\"" << jsonEscape(r.fileName)
             << "\",\"transaction_id\":" << r.transactionID << "}";
    }
    cout << "],\"edges\":[";
    bool edgeFirst=true;
    q=waitingQueue;
    while(!q.empty()) {
        auto r=q.front(); q.pop();
        for(const auto &l:lockTable) {
            if(l.fileName==r.fileName && l.transactionID!=r.transactionID) {
                if(!edgeFirst) cout << ",";
                edgeFirst=false;
                cout << "{\"from\":" << r.transactionID << ",\"to\":" << l.transactionID
                     << ",\"file\":\"" << jsonEscape(r.fileName) << "\"}";
            }
        }
    }
    cout << "]}" << endl;
}
void guiProtocol() {
    string command;
    while(getline(cin, command)) {
        if(command=="STATE") {
            guiState();
        } else if(command=="DETECT" || command=="RECOVER") {
            ostringstream captured;
            streambuf *old=cout.rdbuf(captured.rdbuf());
            if(command=="DETECT") detectDeadlock();
            else recoverDeadlock();
            cout.rdbuf(old);
            cout << "{\"ok\":true,\"message\":\"" << jsonEscape(captured.str()) << "\"}" << endl;
        } else if(command=="QUIT") {
            cout << "{\"ok\":true,\"message\":\"Backend stopped.\"}" << endl;
            break;
        } else {
            cout << "{\"ok\":false,\"message\":\"Unknown command.\"}" << endl;
        }
    }
}

int main(int argc, char *argv[]){if(sqlite3_open("database/project.db",&DB)!=SQLITE_OK){cout<<"Database connection failed: "<<(DB?sqlite3_errmsg(DB):"Unknown error")<<"\n";if(DB)sqlite3_close(DB);return 1;}sqlite3_busy_timeout(DB,5000);execSQL("PRAGMA foreign_keys=ON;");
    if(!execSQL("CREATE TABLE IF NOT EXISTS transaction_changes (transaction_id INTEGER NOT NULL, sequence_no INTEGER NOT NULL, file_name TEXT NOT NULL, operation TEXT NOT NULL, content TEXT NOT NULL DEFAULT '', PRIMARY KEY(transaction_id, sequence_no));")){sqlite3_close(DB);return 1;}
    if(argc > 1 && string(argv[1]) == "--gui") {
        ostringstream startupOutput;
        streambuf *oldOutput = cout.rdbuf(startupOutput.rdbuf());
        loadPersistedChanges();
        cout.rdbuf(oldOutput);
        guiProtocol();
        sqlite3_close(DB);
        return 0;
    }
    loadPersistedChanges();
    int choice;
 do{cout<<"\n===== Transactional MVCC Filesystem =====\n1. Create File\n2. Read File\n3. Update File\n4. Delete File\n5. View Files\n6. View Version History\n7. Begin Transaction\n8. Commit Selected Transaction\n9. Abort Selected Transaction\n10. View Transaction History\n11. Create Snapshot\n12. Restore Snapshot\n13. View Snapshots\n14. Select Transaction\n15. View Locks and Waiting Queue \n16. View Wait-For Graph \n17. Detect Deadlock\n18. Recover Deadlock\n0. Exit\nEnter choice: ";if(!(cin>>choice)){cin.clear();cin.ignore(10000,'\n');cout<<"Please enter a valid number.\n";continue;}switch(choice){case 1:createFile();break;case 2:readFile();break;case 3:updateFile();break;case 4:deleteFile();break;case 5:viewFiles();break;case 6:viewVersionHistory();break;case 7:beginTransaction();break;case 8:commitTransaction();break;case 9:abortTransaction();break;case 10:viewTransactions();break;case 11:createSnapshot();break;case 12:restoreSnapshot();break;case 13:viewSnapshots();break;case 14:selectTransaction();break;case 15:viewLocks();break;case 16:viewWaitForGraph();break;case 17: detectDeadlock(); break;case 18: recoverDeadlock();break; case 0:{sqlite3_stmt *s=prep("SELECT COUNT(*) FROM transactions WHERE status='ACTIVE';");bool a=s&&sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int(s,0)>0;if(s)sqlite3_finalize(s);if(a){cout<<"Abort or commit active transactions before exiting.\n";choice=-1;}else cout<<"Exiting.\n";break;}default:cout<<"Invalid choice.\n";}}while(choice!=0);sqlite3_close(DB);return 0;}
