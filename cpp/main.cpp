
#include "common/app_state.h"
#include "database/database.h"
#include "os/lock_manager.h"
#include "os/deadlock.h"
#include "transactions/transaction.h"
#include "filesystem/mvcc.h"
#include "gui/gui_protocol.h"
#include "filesystem/file_operations.h"
#include "snapshots/snapshot.h"
#include "memory/paging.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sqlite3.h>
#include <queue>
#include <set>
#include <functional>
#include <sstream>

using namespace std;

// PROGRAM ENTRY POINT: CONSOLE MODE OR GUI BACKEND MODE

int main(int argc, char *argv[])
{
    if(sqlite3_open("database/project.db", &DB) != SQLITE_OK)
    {
        cout << "Database connection failed: "
             << (DB ? sqlite3_errmsg(DB) : "Unknown error") << "\n";

        if(DB)
            sqlite3_close(DB);

        return 1;
    }

    sqlite3_busy_timeout(DB, 5000);

    execSQL("PRAGMA foreign_keys=ON;");

    if(!execSQL("CREATE TABLE IF NOT EXISTS transaction_changes (transaction_id INTEGER NOT NULL, sequence_no INTEGER NOT NULL, file_name TEXT NOT NULL, operation TEXT NOT NULL, content TEXT NOT NULL DEFAULT '', PRIMARY KEY(transaction_id, sequence_no));"))
    {
        sqlite3_close(DB);
        return 1;
    }

    // GUI BACKEND MODE
    if(argc > 1 && string(argv[1]) == "--gui")
    {
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

    do
    {
        cout << "\n===== Transactional MVCC Filesystem =====\n";

        cout << "1. Create File\n";
        cout << "2. Read File\n";
        cout << "3. Update File\n";
        cout << "4. Delete File\n";
        cout << "5. View Files\n";
        cout << "6. View Version History\n";
        cout << "7. Begin Transaction\n";
        cout << "8. Commit Selected Transaction\n";
        cout << "9. Abort Selected Transaction\n";
        cout << "10. View Transaction History\n";
        cout << "11. Create Snapshot\n";
        cout << "12. Restore Snapshot\n";
        cout << "13. View Snapshots\n";
        cout << "14. Select Transaction\n";
        cout << "15. View Locks and Waiting Queue\n";
        cout << "16. View Wait-For Graph\n";
        cout << "17. Detect Deadlock\n";
        cout << "18. Recover Deadlock\n";
        cout << "19. Select Paging Algorithm (FIFO / LRU)\n";
        cout << "20. View Memory Frames and Statistics\n";
        cout << "0. Exit\n";

        cout << "Enter choice: ";

        if(!(cin >> choice))
        {
            cin.clear();
            cin.ignore(10000, '\n');
            cout << "Please enter a valid number.\n";
            continue;
        }

        switch(choice)
        {
            case 1:
                createFile();
                break;

            case 2:
                readFile();
                break;

            case 3:
                updateFile();
                break;

            case 4:
                deleteFile();
                break;

            case 5:
                viewFiles();
                break;

            case 6:
                viewVersionHistory();
                break;

            case 7:
                beginTransaction();
                break;

            case 8:
                commitTransaction();
                break;

            case 9:
                abortTransaction();
                break;

            case 10:
                viewTransactions();
                break;

            case 11:
                createSnapshot();
                break;

            case 12:
                restoreSnapshot();
                break;

            case 13:
                viewSnapshots();
                break;

            case 14:
                selectTransaction();
                break;

            case 15:
                viewLocks();
                break;

            case 16:
                viewWaitForGraph();
                break;

            case 17:
                detectDeadlock();
                break;

            case 18:
                recoverDeadlock();
                break;

            // SELECT FIFO OR LRU PAGING
            case 19:
            {
                int algorithmChoice;

                cout << "\n===== Select Paging Algorithm =====\n";
                cout << "1. FIFO Paging\n";
                cout << "2. LRU Paging\n";
                cout << "Enter choice: ";

                if(!(cin >> algorithmChoice))
                {
                    cin.clear();
                    cin.ignore(10000, '\n');
                    cout << "Please enter a valid number.\n";
                    break;
                }

                if(algorithmChoice == 1)
                {
                    pagingManager.setAlgorithm(false);
                }
                else if(algorithmChoice == 2)
                {
                    pagingManager.setAlgorithm(true);
                }
                else
                {
                    cout << "Invalid algorithm choice.\n";
                }

                break;
            }

            // DISPLAY MEMORY FRAMES AND STATISTICS
            case 20:
            {
                cout << "\n===== Memory Management =====\n";

                pagingManager.displayFrames();
                pagingManager.displayStatistics();

                break;
            }

            case 0:
            {
                sqlite3_stmt *s =
                    prep("SELECT COUNT(*) FROM transactions WHERE status='ACTIVE';");

                bool a = s &&
                         sqlite3_step(s) == SQLITE_ROW &&
                         sqlite3_column_int(s, 0) > 0;

                if(s)
                    sqlite3_finalize(s);

                if(a)
                {
                    cout << "Abort or commit active transactions before exiting.\n";
                    choice = -1;
                }
                else
                {
                    cout << "Exiting.\n";
                }

                break;
            }

            default:
                cout << "Invalid choice.\n";
        }

    } while(choice != 0);

    sqlite3_close(DB);

    return 0;
}
