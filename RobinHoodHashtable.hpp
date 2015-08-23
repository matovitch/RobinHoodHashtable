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

    // Note : One use a dib of 0 and 1 for empty and to-rehash buckets.
    // Inserting t at (hash(t) + 2) % capacity : this is a small optim.
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


/**
 * RobinHoodHashTable: hashtable using the Robin Hood hashing
 */
template <typename T,                               // type of the contained values
          typename H = std::hash<T>,                // hasher
          typename E = std::equal_to<T>,            // equality comparator
          typename A = std::allocator<Bucket<T>>>   // allocator
struct RobinHoodHashtable
{
    typedef T value_type;

    constexpr static const std::size_t INIT_SIZE   = 8; // number of buckets to start with
    constexpr static const std::size_t EXP_GROWTH  = 1; // exponential growth factor
    constexpr static const std::size_t LOAD_FACTOR = 2000000; // load factor as 1 - 1 / 2^n

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

        //Reserve the new capacity
        const std::size_t oldCapacity = _capacity;

        _buckets.resize(_capacity <<= EXP_GROWTH);
        _size = 1;

        //Each non-empty bucket is marked empty before its value is rehashed
        for (auto& b : _buckets)
        {
            if (b.isRehash())
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

    void insert(T t)
    {
        //Check if one need a rehash
        if (((++_size) << LOAD_FACTOR) >= (_capacity << LOAD_FACTOR) - _capacity)
        {
            rehash();
        }

        uint8_t dib = Bucket<T>::FILLED;

        do
        {
            const std::size_t hash = _hasher(t);

            //Skip filled buckets with larger dib
            while (dib < _buckets[(hash + dib) % _capacity]._dib)
            {
                dib++;
            }

            Bucket<T>* head = &_buckets[(hash + dib) % _capacity];

            if (dib == head->_dib && t == head->_value)
            {
                //the element is present, nothing to do
                break;
            }
            else
            {
                //copy the value of the found bucket and insert our own
                const T tTmp = head->_value;
                const uint8_t dibTmp = head->_dib + 1;

                head->_value = t;
                head->_dib = dib;

                t = tTmp;
                dib = dibTmp;
            }
            //go on with the displaced bucket if non empty
        } while (dib >= Bucket<T>::FILLED);

    }

    void erase(const T& t)
    {
        uint8_t dib = Bucket<T>::FILLED;
        const std::size_t hash = _hasher(t);
        Bucket<T>* const base = &_buckets[0];

        Bucket<T>* prec = &_buckets[(hash + dib) % _capacity];

        //Skip buckets with lower dib or different value
        while (dib < prec->_dib || (dib == prec->_dib && t != prec->_value))
        {
            dib++;
            prec = &_buckets[(hash + dib) % _capacity];
        }

        //if the element is found
        if (dib == prec->_dib && t == prec->_value)
        {
            Bucket<T>* succ = base + ((prec - base + 1) % _capacity);

            //Shift the right-adjacent buckets to the left
            while (succ->_dib > Bucket<T>::FILLED)
            {
                prec->_dib = succ->_dib - 1;
                prec->_value = succ->_value;
                prec = succ;
                succ = &_buckets[0] + ((prec - base + 1) % _capacity);
            }

            //Empty the bucket and decrement the size
            prec->markEmpty();
            _size--;
        }
    }

    /**
     * Template function to avoid find/cfind redundancy
     */
    template <typename I>
    I ufind(const T& t) const
    {
        uint8_t dib =  Bucket<T>::FILLED;
        const std::size_t hash = _hasher(t);
        const Bucket<T>* base = &_buckets[0];

        const Bucket<T>* prec = &_buckets[(hash + dib) % _capacity];

        //Skip buckets with lower dib or different value
        while (dib < prec->_dib || (dib == prec->_dib && t != prec->_value))
        {
            dib++;
            prec = &_buckets[(hash + dib) % _capacity];
        }

        //if the element is found
        if (dib == prec->_dib && t == prec->_value)
        {
            return I(prec, base + _capacity);
        }
        else
        {
            return uend<I>();
        }
    }

    /**
     * Same thing, utility for begin/cbegin
     */
    template <typename I>
    I ubegin() const
    {
        const Bucket<T>* base = &_buckets[0];

        return ((base->isFilled()) ?    I(base, base + _capacity)
                                   : ++(I(base, base + _capacity)));
    }

    /**
     * Same thing, utility for end/cend
     */
    template <typename I>
    I uend() const
    {
        const Bucket<T>* end = &_buckets[0] + _capacity;
        return I(end, end);
    }

    template <typename U, // U buckets pointer type const/non-const
              typename V> // V value type const/non-const
    struct Iterator
    {
        Iterator(const U bucketPtr, const Bucket<T>* end) : _bucketPtr(bucketPtr),
                                                            _end(end) {}

        Iterator(const Iterator<U, V>& it) : _bucketPtr(it._bucketPtr),
                                             _end(it._end) {}

        V& operator* () const { return _bucketPtr->_value; }

        V* operator-> () const { return &(_bucketPtr->_value); }

        Iterator<U, V>& operator++()
        {
            do
            {
                _bucketPtr++;
            } while (_bucketPtr != _end && !_bucketPtr->isFilled());
            //we skipped the empty buckets ! Hoora !
            return *this;
        }

        Iterator<U, V>& operator++(int)
        {
            iterator tmp(*this);
            operator++();
            return tmp;
        }

        bool operator==(const Iterator<U, V>& rhs) const
        {
            return this->_bucketPtr == rhs->_bucketPtr;
        }

        bool operator!=(const Iterator<U, V>& rhs) const
        {
            return this->_bucketPtr != rhs._bucketPtr;
        }

        U _bucketPtr;           //Bucket pointer by the iterator
        const Bucket<T>* _end;  //End iterator pointer
    };

    //The classic iterator typedefs
    typedef Iterator<      Bucket<T>*,       T>       iterator;
    typedef Iterator<const Bucket<T>*, const T> const_iterator;

          iterator  find(const T& t) const { return ufind<      iterator>(t); }
    const_iterator cfind(const T& t) const { return ufind<const_iterator>(t); }

          iterator  begin() const { return ubegin<      iterator>(); }
    const_iterator cbegin() const { return ubegin<const_iterator>(); }

          iterator  end() const { return uend<      iterator>(); }
    const_iterator cend() const { return uend<const_iterator>(); }

    std::size_t size() { return _size; }

    std::vector<Bucket<T>>  _buckets;   // vector of buckets
    std::size_t             _capacity;  // reserved bucket/memory
    std::size_t             _size;      // number of inserted elements
    const H                 _hasher;    // for hash computations
};


#endif
