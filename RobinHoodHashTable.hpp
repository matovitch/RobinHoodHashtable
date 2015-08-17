#ifndef __ROBIN_HOOD_HASH_TABLE_H__
#define __ROBIN_HOOD_HASH_TABLE_H__

#include <algorithm>
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

    enum
    {
        EMPTY  = 0,
        REHASH = 1,
        FILLED = 2
    };

    void markEmpty () { _dib = EMPTY ; }
    void markRehash() { _dib = REHASH; }

    bool isEmpty () const { return _dib == EMPTY ; }
    bool isRehash() const { return _dib == REHASH; }
    bool isFilled() const { return _dib >= FILLED; }

    uint8_t _dib = 0;   // Distance to Initial Bucket
    T       _value;     // the actual value
};

//typedef int T; //TODO comment

/**
 * RobinHoodHashTable: hashtable using the Robin Hood hashing
 */
template <typename T,                               // type of the contained values
          typename H = std::hash<T>,                // hasher
          typename E = std::equal_to<T>,            // equality comparator
          typename A = std::allocator<Bucket<T>>    // allocator   
         > struct RobinHoodHashtable
{
    typedef T value_type;

    constexpr static float LOAD_FACTOR           = 0.7; // load factor
    constexpr static const float EXP_GROWTH      = 2.0; // exponential growth factor
    constexpr static const std::size_t INIT_SIZE =   8; // number of buckets to start with

    RobinHoodHashtable() : _size(0),
                           _hasher(),
                           _buckets(INIT_SIZE),
                           _capacity(INIT_SIZE) {}

    /**
     * Reserve more memory and rehash the table accordingly
     */
    void rehash()
    {
        //Mark the already contained elements as rehashable
        for (auto& b : _buckets)
        {
            if (b.isFilled())
            {
                b.markRehash();
            }
        }

        //reserve the new capacity
        const std::size_t oldCapacity = _capacity;

        _buckets.resize(_capacity *= EXP_GROWTH);
        _size = 0;

        //Each non-empty bucket is marked empty before its value is rehashed
        for (auto& b : _buckets)
        {
            if (b.isFilled())
            {
                this->insert(b._value); 
            }
            //The filled buckets cannot be futher than the old capacity
            if ((&b - &_buckets[0]) > oldCapacity)
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
        if ((++_size) >= _capacity * LOAD_FACTOR)
        {
            rehash();
        }

        //Robin Hood hashing technique
        uint8_t dib = Bucket<T>::FILLED;

        do
        {
            std::size_t offset = _hasher(t) % _capacity;
    
            while (dib < _buckets[(offset + dib) % _capacity]._dib)
            {
                dib++;
            }

            Bucket<T>* other = &_buckets[0] + ((offset + dib) % _capacity);

            if (dib == other->_dib && t == other->_value)
            {
                break;
            }
            else
            {
                const T tTmp = other->_value;
                const uint8_t dibTmp = other->_dib + 1;
       
                other->_value = t;
                other->_dib = dib;
          
                t = tTmp;
                dib = dibTmp;
            }
            
        } while (dib >= Bucket<T>::FILLED);

    }

    std::size_t size() { return _size; }

    std::vector<Bucket<T>>  _buckets;
    std::size_t             _capacity;
    std::size_t             _size;
    const H                 _hasher;
};


#endif