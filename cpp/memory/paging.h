
#ifndef PAGING_H
#define PAGING_H

#include <string>
#include <vector>

using namespace std;

struct Page
{
    string fileName;
    int pageNumber;
    string content;
    unsigned long long lastUsed;
    unsigned long long loadedAt;
};

struct Frame
{
    bool occupied = false;
    Page page;
};

class PagingManager
{
private:
    vector<Frame> frames;

    int pageSize;
    int pageHits;
    int pageFaults;
    int pageReplacements;

    unsigned long long timer;

    bool useLRU;

public:
    PagingManager(int frameCount = 3, int pageSize = 4096);

    void loadPage(const string &fileName, int pageNumber,
                  const string &content);

    void loadPageLRU(const string &fileName, int pageNumber,
                     const string &content);

    void loadFile(const string &fileName, const string &content);

    void setAlgorithm(bool lru);

    void displayFrames();

    void displayStatistics();

    void reset();
    const vector<Frame>& getFrames() const;

int getPageHits() const;
int getPageFaults() const;
int getPageReplacements() const;
int getPageSize() const;

bool isLRU() const;
};

// Shared memory manager
extern PagingManager pagingManager;

#endif
