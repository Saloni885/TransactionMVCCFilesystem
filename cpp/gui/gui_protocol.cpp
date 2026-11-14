
#include "gui_protocol.h"
#include "../database/database.h"

#include "../common/app_state.h"
#include "../os/deadlock.h"
#include "../transactions/transaction.h"
#include "../snapshots/snapshot.h"
#include "../memory/paging.h"
#include "../filesystem/file_operations.h"

#include <iostream>
#include <string>
#include <queue>
#include <sstream>

using namespace std;

string jsonEscape(const string &value) {
    string out;

    for (unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c >= 32) out += (char)c;
    }

    return out;
}

void guiState() {
    cout << "{\"ok\":true,\"selected_transaction\":"
         << currentTransactionID << ",\"locks\":[";

    for (size_t i = 0; i < lockTable.size(); ++i) {
        if (i) cout << ",";

        cout << "{\"file\":\"" << jsonEscape(lockTable[i].fileName)
             << "\",\"transaction_id\":" << lockTable[i].transactionID << "}";
    }

    cout << "],\"waiting\":[";

    queue<LockRequest> q = waitingQueue;
    bool first = true;

    while (!q.empty()) {
        auto r = q.front();
        q.pop();

        if (!first) cout << ",";
        first = false;

        cout << "{\"file\":\"" << jsonEscape(r.fileName)
             << "\",\"transaction_id\":" << r.transactionID << "}";
    }

    cout << "],\"edges\":[";

    bool edgeFirst = true;
    q = waitingQueue;

    while (!q.empty()) {
        auto r = q.front();
        q.pop();

        for (const auto &l : lockTable) {
            if (l.fileName == r.fileName &&
                l.transactionID != r.transactionID) {

                if (!edgeFirst) cout << ",";
                edgeFirst = false;

                cout << "{\"from\":" << r.transactionID
                     << ",\"to\":" << l.transactionID
                     << ",\"file\":\"" << jsonEscape(r.fileName) << "\"}";
            }
        }
    }

    cout << "]}" << endl;
}
string decodeHex(const string &hex)
{
    string result;

    if (hex.size() % 2 != 0)
        return "";

    for (size_t i = 0; i < hex.size(); i += 2)
    {
        try
        {
            int value = stoi(hex.substr(i, 2), nullptr, 16);
            result += static_cast<char>(value);
        }
        catch (...)
        {
            return "";
        }
    }

    return result;
}

void guiProtocol() {
    string command;

    while (getline(cin, command)) {
        istringstream input(command);
        string action;
        input >> action;

        if (action == "STATE") {
            guiState();
        }

        else if (action == "BEGIN") {
            ostringstream captured;
            streambuf *old = cout.rdbuf(captured.rdbuf());

            beginTransaction();

            cout.rdbuf(old);

            cout << "{\"ok\":true,\"message\":\""
                 << jsonEscape(captured.str())
                 << "\",\"selected_transaction\":"
                 << currentTransactionID << "}" << endl;
        }

        else if (action == "SELECT") {
            int id;

            if (!(input >> id)) {
                cout << "{\"ok\":false,\"message\":\"Invalid transaction ID.\"}"
                     << endl;
                continue;
            }

            ostringstream captured;
            streambuf *old = cout.rdbuf(captured.rdbuf());

            sqlite3_stmt *s = prep(
                "SELECT 1 FROM transactions "
                "WHERE transaction_id=? AND status='ACTIVE';"
            );

            bool valid = false;

            if (s) {
                sqlite3_bind_int(s, 1, id);
                valid = sqlite3_step(s) == SQLITE_ROW;
                sqlite3_finalize(s);
            }

            if (valid) {
                currentTransactionID = id;
                cout << "Selected transaction " << id << ".\n";
            }
            else {
                cout << "Transaction is not active.\n";
            }

            cout.rdbuf(old);

            cout << "{\"ok\":" << (valid ? "true" : "false")
                 << ",\"message\":\"" << jsonEscape(captured.str())
                 << "\",\"selected_transaction\":"
                 << currentTransactionID << "}" << endl;
        }
                else if (action == "UPDATE") {

            string encodedName, encodedContent;
            input >> encodedName >> encodedContent;

            if (encodedName.empty() || encodedContent.empty()) {
                cout << "{\"ok\":false,\"message\":\"Invalid update data.\"}"
                     << endl;
                continue;
            }

            string name = decodeHex(encodedName);
            string content = decodeHex(encodedContent);

            string message;

            bool success = stageUpdateFile(name, content, message);

            cout << "{\"ok\":" << (success ? "true" : "false")
                 << ",\"message\":\"" << jsonEscape(message)
                 << "\"}" << endl;
        }

        else if (action == "COMMIT" || action == "ABORT") {
            ostringstream captured;
            streambuf *old = cout.rdbuf(captured.rdbuf());

            if (action == "COMMIT")
                commitTransaction();
            else
                abortTransaction();

            cout.rdbuf(old);

            cout << "{\"ok\":true,\"message\":\""
                 << jsonEscape(captured.str())
                 << "\",\"selected_transaction\":"
                 << currentTransactionID << "}" << endl;
        }

else if (action == "SNAPSHOT") {

    string name;
    getline(input >> ws, name);

    string message;

    bool success = createSnapshot(name, message);

    cout << "{\"ok\":" << (success ? "true" : "false")
         << ",\"message\":\"" << jsonEscape(message)
         << "\"}" << endl;
}

else if (action == "PAGING") {

    string algorithm;
    input >> algorithm;

    if (algorithm != "FIFO" && algorithm != "LRU") {
        cout << "{\"ok\":false,\"message\":\"Invalid paging algorithm.\"}"
             << endl;
        continue;
    }

    ostringstream captured;
    streambuf *old = cout.rdbuf(captured.rdbuf());

    pagingManager.setAlgorithm(algorithm == "LRU");

    cout.rdbuf(old);

    cout << "{\"ok\":true,\"message\":\""
         << jsonEscape(captured.str())
         << "\"}" << endl;
}

else if (action == "PAGINGSTATE") {

    cout << "{\"ok\":true,\"algorithm\":\""
         << (pagingManager.isLRU() ? "LRU" : "FIFO")
         << "\",\"page_size\":" << pagingManager.getPageSize()
         << ",\"hits\":" << pagingManager.getPageHits()
         << ",\"faults\":" << pagingManager.getPageFaults()
         << ",\"replacements\":" << pagingManager.getPageReplacements()
         << ",\"frames\":[";

    const vector<Frame>& frames = pagingManager.getFrames();

    for (size_t i = 0; i < frames.size(); i++) {

        if (i > 0)
            cout << ",";

        cout << "{\"frame\":" << i + 1
             << ",\"occupied\":"
             << (frames[i].occupied ? "true" : "false");

        if (frames[i].occupied) {
            cout << ",\"file\":\""
                 << jsonEscape(frames[i].page.fileName)
                 << "\",\"page_number\":"
                 << frames[i].page.pageNumber;
        }

        cout << "}";
    }

    cout << "]}" << endl;
}


        else if (action == "LOADFILE") {

            int fileId;
            string fileName;

            if (!(input >> fileId)) {
                cout << "{\"ok\":false,\"message\":\"Invalid file ID.\"}"
                     << endl;
                continue;
            }

            getline(input >> ws, fileName);

            sqlite3_stmt *s = prep(
                "SELECT content FROM file_versions "
                "WHERE file_id=? "
                "AND COALESCE(is_committed, 1)=1 "
                "ORDER BY version_number DESC LIMIT 1;"
            );

            if (!s) {
                cout << "{\"ok\":false,\"message\":\"Database query failed.\"}"
                     << endl;
                continue;
            }

            sqlite3_bind_int(s, 1, fileId);

            if (sqlite3_step(s) != SQLITE_ROW) {
                sqlite3_finalize(s);

                cout << "{\"ok\":false,\"message\":\"No committed file content found.\"}"
                     << endl;
                continue;
            }

            const unsigned char *data = sqlite3_column_text(s, 0);
            int bytes = sqlite3_column_bytes(s, 0);

            string content;

            if (data && bytes > 0) {
                content.assign(
                    reinterpret_cast<const char *>(data),
                    bytes
                );
            }

            sqlite3_finalize(s);

            ostringstream captured;
            streambuf *old = cout.rdbuf(captured.rdbuf());

            pagingManager.loadFile(fileName, content);

            cout.rdbuf(old);

            cout << "{\"ok\":true,\"message\":\""
                 << jsonEscape(captured.str())
                 << "\"}" << endl;
        }


else if (action == "RESTORE") {

    string name;
    getline(input >> ws, name);

    if (name.empty()) {
        cout << "{\"ok\":false,\"message\":\"Snapshot name cannot be empty.\"}"
             << endl;
        continue;
    }

    istringstream snapshotInput(name + "\n");
    ostringstream captured;

    streambuf *oldIn = cin.rdbuf();
    streambuf *oldOut = cout.rdbuf();

    cin.rdbuf(snapshotInput.rdbuf());
    cout.rdbuf(captured.rdbuf());

    restoreSnapshot();

    cin.rdbuf(oldIn);
    cout.rdbuf(oldOut);

    string message = captured.str();

    bool success =
        message.find("Snapshot restored.") != string::npos;

    cout << "{\"ok\":" << (success ? "true" : "false")
         << ",\"message\":\"" << jsonEscape(message)
         << "\"}" << endl;
}



       
else if (action == "DETECT" || action == "RECOVER")
{
    ostringstream captured;
    streambuf *old = cout.rdbuf(captured.rdbuf());

    if (action == "DETECT")
        detectDeadlock();
    else
        recoverDeadlock();

    cout.rdbuf(old);

    string output = captured.str();

    if (output.empty())
        output = "No response received from deadlock operation.";

    // Remove leading and trailing whitespace
    size_t first = output.find_first_not_of(" \n\r\t");
    size_t last = output.find_last_not_of(" \n\r\t");

    if (first != string::npos)
        output = output.substr(first, last - first + 1);

    cout << "{\"ok\":true,\"message\":\""
         << jsonEscape(output) << "\"}" << endl;
}
    }
}
