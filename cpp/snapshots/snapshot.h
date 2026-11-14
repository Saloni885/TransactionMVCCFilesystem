#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <string>
#include <cstddef>

void createSnapshot();
bool createSnapshot(const std::string &name, std::string &message);
void viewSnapshots();

bool readField(const std::string &d, size_t &p, std::string &v);

void restoreSnapshot();

#endif