#ifndef __ROBIN_HOOD_HASH_TABLE_H__
#define __ROBIN_HOOD_HASH_TABLE_H__

#include <algorithm>
#include <functional>
#include <cstddef>
#include <iostream>
#include <algorithm>
#include <memory>
#include <stack>


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

    uint8_t _dib;   // Distance to Initial Bucket
    T       _value; // the actual value
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

    constexpr static const std::size_t INIT_SIZE   = 4096; // number of buckets to start with
    constexpr static const std::size_t LOAD_FACTOR =    3; // load factor as 1 - 1 / 2^n

    RobinHoodHashtable() : _buckets(),
                           _capacity(INIT_SIZE),
                           _size(0),
                           _hasher(),
                           _equalTo(),
                           _allocator(),
                           _allocs()                       
    {
        _buckets = _allocator.allocate(INIT_SIZE);
        _allocs.push(Alloc(_buckets, INIT_SIZE));

        for (std::size_t i = 0; i < INIT_SIZE; i++)
        {
            _allocator.construct(_buckets + i);
            _buckets[i].markEmpty();
        }
    }

    /**
     * Reserve more memory and rehash the table accordingly
     */
    void rehash()
    {
        
        std::size_t oldCapacity = _capacity;
        Bucket<T>* added = _allocator.allocate(_capacity << 1, _buckets + _capacity);

        std::uninitialized_copy(_buckets, _buckets + _capacity, added);
        _buckets = added;

        _allocs.push(Alloc(added, _capacity << 1));
        _capacity = _capacity << 1;

        //Mark the already contained elements as rehashable
        for (std::size_t i = 0; i < oldCapacity; i++)
        {
            if (_buckets[i].isFilled())
            {
                _buckets[i].markRehash();
            }
        }

        for (std::size_t i = oldCapacity; i < _capacity; i++)
        {
            _allocator.construct(_buckets + i);
            _buckets[i].markEmpty();
        }

        _size = 1;

        //Each non-empty bucket is marked empty before its value is rehashed
        for (std::size_t i = 0; i < oldCapacity; i++)
        {
            if (_buckets[i].isRehash())
            {
                _buckets[i].markEmpty();
                this->insert(_buckets[i]._value);
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

        loop:

            const std::size_t hash = _hasher(t);

            //Skip filled buckets with larger dib
            while (dib < _buckets[(hash + dib) % _capacity]._dib)
            {
                dib++;
            }

            Bucket<T>* head = &_buckets[(hash + dib) % _capacity];

            if (dib == head->_dib && _equalTo(t, head->_value))
            {
                _size--;
            }
            else if (head->isEmpty())
            {
                head->_value = t;
                head->_dib = dib;
            }
            else
            {
                if (head->isRehash()) _size++;
                //copy the value of the found bucket and insert our own
                const T tTmp = head->_value;
                const uint8_t dibTmp = head->_dib + 1;

                head->_value = t;
                head->_dib = dib;

                t = tTmp;
                dib = dibTmp;
                goto loop;
            }
    }

    void erase(const T& t)
    {
        uint8_t dib = Bucket<T>::FILLED;
        const std::size_t hash = _hasher(t);
        Bucket<T>* const base = &_buckets[0];

        Bucket<T>* prec = &_buckets[(hash + dib) % _capacity];

        //Skip buckets with lower dib or different value
        while (dib < prec->_dib || (dib == prec->_dib && !_equalTo(t, prec->_value)))
        {
            dib++;
            prec = &_buckets[(hash + dib) % _capacity];
        }

        //if the element is found
        if (dib == prec->_dib && _equalTo(t, prec->_value))
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
        while (dib < prec->_dib || (dib == prec->_dib && !_equalTo(t, prec->_value)))
        {
            dib++;
            prec = &_buckets[(hash + dib) % _capacity];
        }

        //if the element is found
        if (dib == prec->_dib && _equalTo(t, prec->_value))
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

    struct Alloc
    {
        Alloc(Bucket<T>* ptr, std::size_t size) : _ptr(ptr),
                                                  _size(size) {}

        Bucket<T>*  _ptr;
        std::size_t _size;
    };

    ~RobinHoodHashtable()
    {
        while (!(_allocs.empty()))
        {
            _allocator.deallocate(_allocs.top()._ptr, 
                                  _allocs.top()._size);
            _allocs.pop();
        }
    }

    Bucket<T>*        _buckets;   // buckets...
    std::size_t       _capacity;  // number of buckets
    std::size_t       _size;      // number of elements
    H                 _hasher;    // hasher...
    E                 _equalTo;   // equality...
    A                 _allocator; // allocator...
    std::stack<Alloc> _allocs;    // past allocations 
};


#endif
