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

    return 0u;
}
/*
#include <cstddef>
#include <ctime>
#include <unordered_set>
#include <boost/unordered/unordered_set.hpp>
#include <random>

struct A
{
    A() {}

    A(uint64_t value) : _value(value) {}

    uint64_t _value;

   bool operator==(const A& rhs) const { return rhs._value == _value; }

   bool operator!=(const A& rhs) const { return rhs._value != _value; }
};

struct H
{

    std::size_t operator() (const A& a) const
    {
        std::hash<uint64_t> h;
        
        return h(a._value);
    }
};

int main()
{

    typedef uint64_t Value;
    //typedef std::unordered_set<Value, H> Table;
    typedef RobinHoodHashtable<Value, H> Table;

    std::vector<Value> filler;

    std::random_device rd;

    std::default_random_engine urng(rd());

    std::uniform_int_distribution<uint64_t> distrib;

    constexpr std::size_t size = 10000000;

    for (std::size_t i = 0; i < size; ++i)
    {
        filler.push_back(distrib(urng));
    }

    Table myTable;


    //myTable.reserve(10000000);

    for (const auto& e: filler)
    {
        myTable.insert(e);
    }
    
    std::clock_t begin = std::clock();

    for (const auto& e: filler)
    {
        myTable.erase(e);
        //myTable.insert(e);
    }

    std::clock_t end = std::clock();
    double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;

    std::cout << elapsed_secs << std::endl;

    return 0;
}*/