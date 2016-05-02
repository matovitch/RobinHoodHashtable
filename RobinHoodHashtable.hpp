#ifndef __ROBIN_HOOD_HASH_TABLE_H__
#define __ROBIN_HOOD_HASH_TABLE_H__

#include <functional>
#include <algorithm>
#include <cstddef>
#include <memory>

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
        FILLED = 1
    };

    Bucket() : _dib(EMPTY) {}

    void markEmpty () { _dib = EMPTY ; }
    void markFilled() { _dib = FILLED; }

    bool isEmpty () const { return _dib == EMPTY ; }
    bool isFilled() const { return _dib != EMPTY; }

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
class RobinHoodHashtable
{

private:

    void init(std::size_t capacity)
    {
        _capacity = capacity;

        _buckets = _allocator.allocate(_capacity + 1);

        for (std::size_t i = 0; i <= _capacity; i++)
        {
            _allocator.construct(_buckets + i);
        }

        _buckets[_capacity].markFilled();
    }

    void free()
    {
        for (std::size_t i = 0; i <= _capacity; i++)
        {
            _allocator.destroy(_buckets + i);
        }

        _allocator.deallocate(_buckets, _capacity + 1);
    }

    /**
     * Reserve more memory and rehash the table accordingly
     */
    void rehash(std::size_t oldCap,
                std::size_t newCap)
    {
        _capacity = newCap;

        Bucket<T>* added = _allocator.allocate(newCap + 1);

        for (std::size_t i = 0; i <= newCap; i++)
        {
            _allocator.construct(added + i);
        }

        added[newCap].markFilled();

        std::swap(added, _buckets);

        _size = 0;

        for (std::size_t i = 0; i < oldCap; i++)
        {
            if (added[i].isFilled())
            {
                this->insert(std::move(added[i]._value));
            }
            _allocator.destroy(added + i);
        }

        _allocator.deallocate(added, oldCap);
    }

    /**
     * Template function to avoid find/cfind redundancy
     */
    template <typename I>
    I ufind(const T& t) const
    {
        uint8_t dib =  Bucket<T>::FILLED;

        Bucket<T>* prec = &_buckets[(_hasher(t) + dib) % _capacity];

        //Skip buckets with lower dib or different value
        while (dib < prec->_dib || (dib == prec->_dib && !_equalTo(t, prec->_value)))
        {
            dib++;
            prec = (++prec == _buckets + _capacity) ? _buckets : prec;
        }

        //if the element is found
        if (dib == prec->_dib && _equalTo(t, prec->_value))
        {
            return I(prec);
        }

        //no luck :(
        return uend<I>();
    }

    /**
     * Same thing, utility for begin/cbegin
     */
    template <typename I>
    I ubegin() const
    {
        return (_buckets->isFilled()) ?    I(_buckets)
                                       : ++I(_buckets);
    }

    /**
     * Same thing, utility for end/cend
     */
    template <typename I>
    I uend() const
    {
        return I(_buckets + _capacity);
    }

    template <typename U, // U buckets pointer type const/non-const
              typename V> // V value type const/non-const
    struct Iterator
    {
        Iterator(const U bucketPtr) : _bucketPtr(bucketPtr) {}

        Iterator(const Iterator<U, V>& it) : _bucketPtr(it._bucketPtr) {}

        V& operator* () const { return _bucketPtr->_value; }

        V* operator-> () const { return &(_bucketPtr->_value); }

        Iterator<U, V>& operator++()
        {
            do
            {
                _bucketPtr++;
            } while (!_bucketPtr->isFilled());
            //we skipped the empty buckets ! Hoora !
            return *this;
        }

        Iterator<U, V> operator++(int)
        {
            iterator tmp(*this);
            operator++();
            return tmp;
        }

        bool operator==(const Iterator<U, V>& rhs) const
        {
            return _bucketPtr == rhs->_bucketPtr;
        }

        bool operator!=(const Iterator<U, V>& rhs) const
        {
            return _bucketPtr != rhs._bucketPtr;
        }

        U _bucketPtr;           //Bucket pointer by the iterator
    };

public:

    typedef T value_type;

    //The classic iterator typedefs
    typedef Iterator<      Bucket<T>*,       T>       iterator;
    typedef Iterator<const Bucket<T>*, const T> const_iterator;

    constexpr static const std::size_t INIT_SIZE   = 16; // number of buckets to start with
    constexpr static const std::size_t LOAD_FACTOR =  2; // load factor as 1 - 1 / 2^n

    RobinHoodHashtable() :
        _size(0)
    {
        init(INIT_SIZE);
    }

    RobinHoodHashtable(const RobinHoodHashtable& source) :
        _size(source._size)
    {
        init(source._capacity);
        std::copy(source._buckets, source._buckets + _capacity, _buckets);
    }

    ~RobinHoodHashtable()
    {
        free();
    }

    RobinHoodHashtable& operator=(const RobinHoodHashtable& source)
    {
        free();

        _size = source._size;

        init(source._capacity);
        std::copy(source._buckets, source._buckets + _capacity, _buckets);

        return *this;
    }

    void clear()
    {
        free();
        init(INIT_SIZE);
    }

    void reserve(std::size_t size)
    {
        rehash(_capacity, (size << LOAD_FACTOR) / ((1 << LOAD_FACTOR) - 1));
    }

    void insert(const T& t)
    {
        uint8_t dib = Bucket<T>::FILLED;

        T tCopy(t);

        loop:
        {
            Bucket<T>* head = &_buckets[(_hasher(tCopy) + dib) % _capacity];

            //Skip filled buckets with larger dib
            while (dib < head->_dib)
            {
                dib++;
                head = (++head == _buckets + _capacity) ? _buckets : head;
            }

            if (dib != head->_dib || !_equalTo(tCopy, head->_value))
            {
                if (head->isEmpty())
                {
                    head->_value = tCopy;
                    head->_dib = dib;

                    //Check if one need a rehash
                    if (((++_size) << LOAD_FACTOR) >= (_capacity << LOAD_FACTOR) - _capacity)
                    {
                        rehash(_capacity, _capacity << 1);
                    }
                }
                else
                {
                    //copy the value of the found bucket and insert our own
                    const T tTmp = head->_value;
                    const uint8_t dibTmp = head->_dib + 1;

                    head->_value = tCopy;
                    head->_dib = dib;

                    tCopy = tTmp;
                    dib = dibTmp;
                    goto loop;
                }
            }
        }
    }

    void insert(T&& t)
    {
        uint8_t dib = Bucket<T>::FILLED;

        T tCopy = std::move(t);

        loop:
        {
            Bucket<T>* head = &_buckets[(_hasher(tCopy) + dib) % _capacity];

            //Skip filled buckets with larger dib
            while (dib < head->_dib)
            {
                dib++;
                head = (++head == _buckets + _capacity) ? _buckets : head;
            }

            if (dib != head->_dib || !_equalTo(tCopy, head->_value))
            {
                if (head->isEmpty())
                {
                    head->_value = std::move(tCopy);
                    head->_dib = dib;

                    //Check if one need a rehash
                    if (((++_size) << LOAD_FACTOR) >= (_capacity << LOAD_FACTOR) - _capacity)
                    {
                        rehash(_capacity, _capacity << 1);
                    }
                }
                else
                {
                    //copy the value of the found bucket and insert our own
                    T tTmp = std::move(head->_value);
                    const uint8_t dibTmp = head->_dib + 1;

                    head->_value = std::move(tCopy);
                    head->_dib = dib;

                    tCopy = std::move(tTmp);
                    dib = dibTmp;
                    goto loop;
                }
            }
        }
    }

    void erase(const T& t)
    {
        uint8_t dib = Bucket<T>::FILLED;

        Bucket<T>* prec = &_buckets[(_hasher(t) + dib) % _capacity];

        //Skip buckets with lower dib or different value
        while (dib < prec->_dib || (dib == prec->_dib && !_equalTo(t, prec->_value)))
        {
            dib++;
            prec = (++prec == _buckets + _capacity) ? _buckets : prec;
        }

        //if the element is found
        if (dib == prec->_dib)
        {
            Bucket<T>* succ = (prec + 1 == _buckets + _capacity) ? _buckets : prec + 1;

            //Shift the right-adjacent buckets to the left
            while (succ->_dib > Bucket<T>::FILLED)
            {
                prec->_dib = succ->_dib - 1;
                prec->_value = std::move(succ->_value);
                prec = succ;
                succ = (++succ == _buckets + _capacity) ? _buckets : succ;
            }

            //Empty the bucket and decrement the size
            prec->markEmpty();
            _size--;
        }
    }

          iterator  find(const T& t)       { return ufind<      iterator>(t); }
    const_iterator cfind(const T& t) const { return ufind<const_iterator>(t); }

          iterator  begin()       { return ubegin<      iterator>(); }
    const_iterator cbegin() const { return ubegin<const_iterator>(); }

          iterator  end()       { return uend<      iterator>(); }
    const_iterator cend() const { return uend<const_iterator>(); }

    std::size_t size() const { return _size; }

    bool empty() const { return _size == 0; }

private:

    Bucket<T>*        _buckets;   // buckets...
    std::size_t       _capacity;  // number of buckets
    std::size_t       _size;      // number of elements
    H                 _hasher;    // hasher...
    E                 _equalTo;   // equality...
    A                 _allocator; // allocator...
};


#endif
