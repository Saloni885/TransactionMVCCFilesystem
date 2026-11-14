
#include "../memory/paging.h"

#include <iostream>
using namespace std;

int main()
{
    // 3 frames, page size = 4 bytes
    PagingManager memory(3, 4);

    cout << "===== FIFO PAGING TEST =====\n";

    memory.loadPage("file1.txt", 1, "AAAA");
    memory.loadPage("file1.txt", 2, "BBBB");
    memory.loadPage("file1.txt", 3, "CCCC");

    // Page 1 is already present: Hit
    memory.loadPage("file1.txt", 1, "AAAA");

    // All frames full: FIFO replaces page 1
    memory.loadPage("file1.txt", 4, "DDDD");

    memory.displayFrames();
    memory.displayStatistics();

    return 0;
}
