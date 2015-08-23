#include "RobinHoodHashtable.hpp"
#include <iostream>

int main()
{
    typedef RobinHoodHashtable<int> Table;

    Table r;

    for (int i = 99; i >= 0; i--)
        r.insert(i);

    for (Table::const_iterator it = r.cbegin(); it != r.cend(); ++it)
        std::cout << "plop : " << *it << std::endl;

    std::cout << "size : " << r.size() << std::endl;

    std::cout << (r.cfind(35) != r.cend()) << std::endl;

    for (int i = 0; i < 50; i++)
        r.erase(i);

    for (Table::const_iterator it = r.cbegin(); it != r.cend(); ++it)
        std::cout << "plip : " << *it << std::endl;

    std::cout << "size : " << r.size() << std::endl;

    std::cout << (r.cfind(35) != r.cend()) << std::endl;

    return 0;
}
