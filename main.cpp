#include "RobinHoodHashTable.hpp"

int main()
{
    RobinHoodHashtable<int> r;

    for (int i = 0; i < 1000; i++)
        r.insert(i);

    return 0;
}
