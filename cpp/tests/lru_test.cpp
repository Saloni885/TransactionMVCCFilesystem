#include "../memory/paging.h"

#include <iostream>
using namespace std;

int main()
{
    PagingManager memory(3, 4);

    cout << "===== LRU PAGING TEST =====\n";

    memory.loadPageLRU("file1.txt", 1, "AAAA");
    memory.loadPageLRU("file1.txt", 2, "BBBB");
    memory.loadPageLRU("file1.txt", 3, "CCCC");

    // Page 1 becomes recently used
    memory.loadPageLRU("file1.txt", 1, "AAAA");

    // LRU should replace page 2
    memory.loadPageLRU("file1.txt", 4, "DDDD");

    memory.displayFrames();
    memory.displayStatistics();

    return 0;
}