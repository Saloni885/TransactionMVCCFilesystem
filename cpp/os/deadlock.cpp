
#include "deadlock.h"
#include "../common/app_state.h"

#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
#include <functional>

using namespace std;

// Function is defined in the transaction module.
void abortTransaction();

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
