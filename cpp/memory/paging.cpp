
#include "paging.h"

#include <iostream>

using namespace std;
PagingManager pagingManager(3, 4096);

PagingManager::PagingManager(int frameCount, int pageSize)
{
    if (frameCount <= 0)
        frameCount = 3;

    if (pageSize <= 0)
        pageSize = 4096;

    frames.resize(frameCount);

    this->pageSize = pageSize;

    pageHits = 0;
    pageFaults = 0;
    pageReplacements = 0;

    timer = 0;
    useLRU = false;
}
void PagingManager::setAlgorithm(bool lru)
{
    useLRU = lru;

    reset();

    if(useLRU)
        cout << "LRU algorithm selected.\n";
    else
        cout << "FIFO algorithm selected.\n";
}

void PagingManager::loadPage(const string &fileName, int pageNumber,
                             const string &content)
{
    timer++;

    // Check whether the page is already present in memory
    for (int i = 0; i < frames.size(); i++)
    {
        if (frames[i].occupied &&
            frames[i].page.fileName == fileName &&
            frames[i].page.pageNumber == pageNumber)
        {
            pageHits++;
            frames[i].page.lastUsed = timer;

            cout << "Page Hit: " << fileName
                 << " - Page " << pageNumber << endl;

            return;
        }
    }

    // Page is not present in memory
    pageFaults++;

    // Find an empty frame
    for (int i = 0; i < frames.size(); i++)
    {
        if (!frames[i].occupied)
        {
            frames[i].occupied = true;

            frames[i].page.fileName = fileName;
            frames[i].page.pageNumber = pageNumber;
            frames[i].page.content = content;

            frames[i].page.lastUsed = timer;
            frames[i].page.loadedAt = timer;

            cout << "Page Fault: Loaded Page "
                 << pageNumber << " into Frame " << i << endl;

            return;
        }
    }

    // All frames are full: FIFO replacement
    int oldestFrame = 0;

    for (int i = 1; i < frames.size(); i++)
    {
        if (frames[i].page.loadedAt <
            frames[oldestFrame].page.loadedAt)
        {
            oldestFrame = i;
        }
    }

    cout << "Page Replacement: Evicting "
         << frames[oldestFrame].page.fileName
         << " Page " << frames[oldestFrame].page.pageNumber
         << " from Frame " << oldestFrame << endl;

    pageReplacements++;

    frames[oldestFrame].page.fileName = fileName;
    frames[oldestFrame].page.pageNumber = pageNumber;
    frames[oldestFrame].page.content = content;

    frames[oldestFrame].page.lastUsed = timer;
    frames[oldestFrame].page.loadedAt = timer;
}

void PagingManager::loadPageLRU(const string &fileName, int pageNumber,
                                const string &content)
{
    timer++;

    // Check whether page already exists
    for (int i = 0; i < frames.size(); i++)
    {
        if (frames[i].occupied &&
            frames[i].page.fileName == fileName &&
            frames[i].page.pageNumber == pageNumber)
        {
            pageHits++;

            // Update last used time
            frames[i].page.lastUsed = timer;

            cout << "Page Hit: " << fileName
                 << " - Page " << pageNumber << endl;

            return;
        }
    }

    // Page not found
    pageFaults++;

    // Find an empty frame
    for (int i = 0; i < frames.size(); i++)
    {
        if (!frames[i].occupied)
        {
            frames[i].occupied = true;

            frames[i].page.fileName = fileName;
            frames[i].page.pageNumber = pageNumber;
            frames[i].page.content = content;

            frames[i].page.lastUsed = timer;
            frames[i].page.loadedAt = timer;

            cout << "Page Fault: Loaded Page "
                 << pageNumber << " into Frame " << i << endl;

            return;
        }
    }

    // All frames full: LRU replacement
    int leastUsedFrame = 0;

    for (int i = 1; i < frames.size(); i++)
    {
        if (frames[i].page.lastUsed <
            frames[leastUsedFrame].page.lastUsed)
        {
            leastUsedFrame = i;
        }
    }

    cout << "Page Replacement: Evicting "
         << frames[leastUsedFrame].page.fileName
         << " Page " << frames[leastUsedFrame].page.pageNumber
         << " from Frame " << leastUsedFrame << endl;

    pageReplacements++;

    frames[leastUsedFrame].page.fileName = fileName;
    frames[leastUsedFrame].page.pageNumber = pageNumber;
    frames[leastUsedFrame].page.content = content;

    frames[leastUsedFrame].page.lastUsed = timer;
    frames[leastUsedFrame].page.loadedAt = timer;
}


void PagingManager::loadFile(const string &fileName,
                             const string &content)
{
    int totalPages = (content.size() + pageSize - 1) / pageSize;

    cout << "\nFile Size: " << content.size() << " bytes\n";
    cout << "Total Pages Required: " << totalPages << "\n";

    bool contentChanged = false;

    // Check whether any cached page has outdated content
    for(int i = 0; i < frames.size(); i++)
    {
        if(frames[i].occupied &&
           frames[i].page.fileName == fileName)
        {
            int pageNumber = frames[i].page.pageNumber;

            if(pageNumber >= totalPages)
            {
                contentChanged = true;
                break;
            }

            string newPageContent =
                content.substr(pageNumber * pageSize, pageSize);

            if(frames[i].page.content != newPageContent)
            {
                contentChanged = true;
                break;
            }
        }
    }

    // Invalidate old pages if file content has changed
    if(contentChanged)
    {
        cout << "File content changed. Invalidating old cached pages.\n";

        for(int i = 0; i < frames.size(); i++)
        {
            if(frames[i].occupied &&
               frames[i].page.fileName == fileName)
            {
                frames[i].occupied = false;
            }
        }
    }

    // Divide file into pages and load them
    int pageNumber = 0;

    for(int i = 0; i < content.size(); i += pageSize)
    {
        string pageContent = content.substr(i, pageSize);

        if(useLRU)
        {
            loadPageLRU(fileName, pageNumber, pageContent);
        }
        else
        {
            loadPage(fileName, pageNumber, pageContent);
        }

        pageNumber++;
    }
}

void PagingManager::displayFrames()
{
    cout << "\n===== MEMORY FRAMES =====\n";

    for (int i = 0; i < frames.size(); i++)
    {
        cout << "Frame " << i << ": ";

        if (frames[i].occupied)
        {
            cout << frames[i].page.fileName
                 << " - Page " << frames[i].page.pageNumber;
        }
        else
        {
            cout << "Empty";
        }

        cout << endl;
    }
}

void PagingManager::displayStatistics()
{
    cout << "\n===== PAGING STATISTICS =====\n";

    cout << "Page Hits: " << pageHits << endl;
    cout << "Page Faults: " << pageFaults << endl;
    cout << "Page Replacements: " << pageReplacements << endl;
}

void PagingManager::reset()
{
    for (int i = 0; i < frames.size(); i++)
    {
        frames[i].occupied = false;
    }

    pageHits = 0;
    pageFaults = 0;
    pageReplacements = 0;

    timer = 0;
}
const vector<Frame>& PagingManager::getFrames() const
{
    return frames;
}

int PagingManager::getPageHits() const
{
    return pageHits;
}

int PagingManager::getPageFaults() const
{
    return pageFaults;
}

int PagingManager::getPageReplacements() const
{
    return pageReplacements;
}

int PagingManager::getPageSize() const
{
    return pageSize;
}

bool PagingManager::isLRU() const
{
    return useLRU;
}
