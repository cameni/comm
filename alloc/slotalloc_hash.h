#pragma once

#include "slotalloc.h"
#include <type_traits>

COID_NAMESPACE_BEGIN

///Helper class for key extracting from values, performing a cast
template<class T, class KEY>
struct extractor
{
    KEY operator()( const T& value ) const { return value; }
};

template<class T, class KEY>
struct extractor<T*, KEY*>
{
    KEY* operator()(T* value) const { return value; }
};

template<class T, class KEY>
struct extractor<T*, KEY>
{
    KEY operator()(T* value) const { return *value; }
};


////////////////////////////////////////////////////////////////////////////////
///Hash map (keyset) based on the slotalloc container, providing items that can be accessed
/// both with an id or a key.
///Items in the container are assumed to store the key within themselves, and able to extract
/// it via an extractor
template<
    class T,
    class KEY,
    class EXTRACTOR = extractor<T, KEY>,
    class HASHFUNC = hash<KEY>,
    bool POOL=false
>
class slotalloc_hash : public slotalloc<T, POOL>
{
    typedef slotalloc<T, POOL> base;

public:

    T* push( const T& v ) = delete;
    T* add() = delete;
    T* add_uninit( bool* newitem = 0 ) = delete;

    slotalloc_hash( uint reserve_items = 64 )
        : base(reserve_items)
    {
        //append related array for hash table sequences
        base::append_relarray(&_seqtable);

        _buckets.calloc(reserve_items, true);
    }

    //@return object with given key or null if no matching object was found
    const T* find_value( const KEY& key ) const
    {
        uint b = bucket(key);
        uint id = find_object(b, key);

        return id != UMAX32 ? get_item(id) : 0;
    }

    ///Find item by key or insert a new slot for item with such key
    //@param key lookup key
    //@param isnew [out] set to true if the item was newly created
    //@return found object or pointer to a newly created one
    //@note if isnew is true, caller has to set the object pointed to by the return value
    T* find_or_insert_value_slot( const KEY& key, bool* isnew=0 )
    {
        uint id;
        if(find_or_insert_value_slot_uninit_(key, &id)) {
            if(isnew) *isnew = true;
            return new(base::get_item(id)) T;
        }
        
        return base::get_item(id);
    }

    ///Find item by key or insert a new uninitialized slot for item with such key
    //@param key lookup key
    //@param isnew [out] set to true if the item was newly created
    //@return found object or pointer to a newly created one
    //@note if isnew is true, caller has to in-place construct the object pointed to by the return value
    T* find_or_insert_value_slot_uninit( const KEY& key, bool* isnew=0 )
    {
        uint id;
        bool isnew_ = find_or_insert_value_slot_uninit_(key, &id);
        if(isnew)
            *isnew = isnew_;

        return get_item(id);
    }

    T* insert_value_slot( const KEY& key ) { return insert_value_(add(), key); }

    T* insert_value_slot_uninit( const KEY& key ) { return insert_value_(add_uninit(), key); }

    ///Delete item from hash map
    void del( T* p )
    {
        uint id = (uint)get_item_id(p);

        const KEY& key = _EXTRACTOR(*p);
        uint b = bucket(key);

        //remove id from the bucket list
        uint* n = find_object_entry(b, key);
        DASSERT( *n == id );

        *n = (*_seqtable)[id];

        base::del(p);
    }


    ///Delete all items that match the key
    uints erase( const KEY& key )
    {
        uint b = bucket(key);
        uint c = 0;

        uint* n = find_object_entry(b, key);
        while(*n != UMAX32) {
            if(!(_EXTRACTOR(*get_item(*n)) == key))
                break;

            uint id = *n;
            *n = (*_seqtable)[id];

            base::del(get_item(id));
            ++c;
        }

        return c;
    }

    ///Reset content. Destructors aren't invoked in the pool mode, as the objects may still be reused.
    void reset()
    {
        slotalloc::reset();
        memset(_buckets.begin().ptr(), 0xff, _buckets.byte_size());
    }

protected:

    uint bucket( const KEY& k ) const       { return uint(_HASHFUNC(k) % _buckets.size()); }

    ///Find first node that matches the key
    uint find_object( uint bucket, const KEY& k ) const
    {
        uint n = _buckets[bucket];
        while(n != UMAX32)
        {
            if(_EXTRACTOR(*base::get_item(n)) == k)
                return n;
            n = (*_seqtable)[n];
        }

        return n;
    }

    uint* find_object_entry( uint bucket, const KEY& k )
    {
        uint* n = &_buckets[bucket];
        while(*n != UMAX32)
        {
            if(_EXTRACTOR(*base::get_item(*n)) == k)
                return n;
            n = &(*_seqtable)[*n];
        }

        return n;
    }

    bool find_or_insert_value_slot_uninit_( const KEY& key, uint* pid )
    {
        uint b = bucket(key);
        uint id = find_object(b, key);

        bool isnew = id == UMAX32;
        if(isnew) {
            T* p = base::add_uninit();
            id = (uint)base::get_item_id(p);

            (*_seqtable)[id] = _buckets[b];
            _buckets[b] = id;
        }

        *pid = id;
        
        return isnew;
    }

    T* insert_value_( T* p )
    {
        uint id = (uint)base::get_item_id(p);

        uint b = bucket(key);
        uint fid = find_object(b, key);

        _seqtable[id] = fid != UMAX32 ? fid : _buckets[b];

        if(_buckets[b] == _seqtable[id])
            _buckets[b] = id;

        return p;
    }

private:

    EXTRACTOR _EXTRACTOR;
    HASHFUNC _HASHFUNC;

    coid::dynarray<uint> _buckets;      //< table with ids of first objects belonging to the given hash socket
    coid::dynarray<uint>* _seqtable;    //< table with ids pointing to the next object in hash socket chain
};

COID_NAMESPACE_END
