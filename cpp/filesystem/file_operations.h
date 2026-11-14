#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H
#include <string>
using namespace std;
void createFile();
void readFile();
void updateFile();
bool stageUpdateFile(
    const string &name,
    const string &content,
    string &message
);
void deleteFile();

void viewFiles();
void viewVersionHistory();
void viewTransactions();

#endif