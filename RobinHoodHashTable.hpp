#ifndef __ROBIN_HOOD_HASH_TABLE_H__
#define __ROBIN_HOOD_HASH_TABLE_H__

#include <functional>
#include <cstddef>
#include <vector>

/**
 * This is an implementation of a Robin-Hood hashing based hashtable, you can find more info there :
 * http://codecapsule.com/2013/11/11/robin-hood-hashing/
 **/


/**
 *Bucket: one entry of the hashtable
 */
template <typename T>
struct Bucket
{
    typedef T value_type;

    constexpr static std::size_t EMPTY  = static_cast<uint8_t>(-1);

    void markEmpty() { _dib = EMPTY; }
    bool isEmpty() const { return _dib == EMPTY; }

    uint8_t _dib = EMPTY;   // Distance to Initial Bucket
    T       _value;         // the actual value
};


//typedef int T; //TODO comment


/**
 * RobinHoodHashTable: hashtable using the Robin Hood hashing
 */
template <typename T,                               // type of the contained values
          typename H = std::hash<T>,                // hasher
          typename E = std::equal_to<T>,            // equality comparator
          typename A = std::allocator<Bucket<T>>    // allocator   
         > struct RobinHoodHashTable
{
    typedef T value_type;

    constexpr static float LOAD_FACTOR           = 0.7; // load factor
    constexpr static const float EXP_GROWTH      = 2.0; // exponential growth factor
    constexpr static const std::size_t INIT_SIZE =  64; // number of buckets to start with

    RobinHoodHashTable() : _size(0),
                           _hasher(),
                           _buckets(INIT_SIZE),
                           _capacity(INIT_SIZE) {}

    /**
     * Reserve more memory and rehash the table accordingly
     */
    void rehash()
    {
        //reserve the new capacity
        _buckets.reserve(_capacity *= EXP_GROWTH);
 
        //Each non-empty bucket is marked empty before its value is rehashed
        for (auto& b : _buckets)
        {
            if (!b.isEmpty())
            {
                b.markEmpty();
                this->insert(b._value); 
            }
            //The filled buckets cannot be futher than the old capacity
            if ((&b - &_buckets[0]) * EXP_GROWTH > _capacity )
            {
                break;
            }
        }
    }

    /**
     * Insert an element in the hashtable (!by copy!)
     */
    void insert(value_type t)
    {
        //Check if one need a rehash
        if ((++_size) > _capacity * LOAD_FACTOR)
        {
            rehash();
        }

        //Robin Hood hashing technique
        uint8_t dib = 0;
        std::size_t hash = _hasher(t);       
        Bucket<T>* head = &_buckets[hash % _buckets.size()];

        do
        {
            while (!(head->isEmpty()) &&
                   dib < head->_dib)
            {
                head++;
                dib++;
                if (head - &_buckets[0] == _buckets.size())
                {
                    head = &_buckets[0]; 
                }
            }

            const T tTmp = _hasher(head->_value);
            const uint8_t dibTmp = head->_dib;
       
            head->_value = t;
            head->_dib = dib;

            t = tTmp;
            dib = dibTmp;
            
        } while (dib != Bucket<T>::EMPTY);

    }

    std::size_t size() { return _size; }

    std::vector<Bucket<T>>  _buckets;
    std::size_t             _capacity;
    std::size_t             _size;
    const H                 _hasher;
};


#endif
