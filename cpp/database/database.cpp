
#include "database.h"
#include "../common/app_state.h"
#include <iostream>

bool execSQL(const string &sql) {
    char *err = nullptr;

    int rc = sqlite3_exec(DB, sql.c_str(), nullptr, nullptr, &err);

    if (rc != SQLITE_OK) {
        cout << "SQL Error: "
             << (err ? err : sqlite3_errmsg(DB)) << endl;

        sqlite3_free(err);
        return false;
    }

    return true;
}

sqlite3_stmt *prep(const string &sql) {
    sqlite3_stmt *s = nullptr;

    if (sqlite3_prepare_v2(DB, sql.c_str(), -1, &s, nullptr) != SQLITE_OK) {
        cout << "SQL Error: " << sqlite3_errmsg(DB) << endl;
        return nullptr;
    }

    return s;
}

bool done(sqlite3_stmt *s) {
    bool ok = sqlite3_step(s) == SQLITE_DONE;

    if (!ok)
        cout << "SQL Error: " << sqlite3_errmsg(DB) << endl;

    sqlite3_finalize(s);
    return ok;
}

void bindText(sqlite3_stmt *s, int i, const string &v) {
    sqlite3_bind_text(s, i, v.c_str(), -1, SQLITE_TRANSIENT);
}
