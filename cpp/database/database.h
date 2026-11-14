
#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <sqlite3.h>

using namespace std;

bool execSQL(const string &sql);
sqlite3_stmt *prep(const string &sql);
bool done(sqlite3_stmt *s);
void bindText(sqlite3_stmt *s, int i, const string &v);

#endif
