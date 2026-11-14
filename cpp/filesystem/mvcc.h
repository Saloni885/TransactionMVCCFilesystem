#ifndef MVCC_H
#define MVCC_H

#include <vector>
#include "../common/app_state.h"

bool applyChange(int id, const Change &c, int seq);

bool hasWriteConflict(int id, const vector<Change> &changes);

#endif