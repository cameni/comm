#pragma once

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * Outerra.
 * Portions created by the Initial Developer are Copyright (C) 2013
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __COID_COMM_SLOTALLOC__HEADER_FILE__
#define __COID_COMM_SLOTALLOC__HEADER_FILE__

#include <new>
#include "../atomic/atomic.h"
#include "../namespace.h"
#include "../commexception.h"
#include "../dynarray.h"

#include "slotalloc_tracker.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
/**
@brief Allocator for efficient slot allocation/deletion of objects.

Allocates array of slots for given object type, and allows efficient allocation and deallocation of objects without
having to resort to expensive system memory allocators.
Objects within the array have a unique slot id that can be used to retrieve the objects by id or to have an id of the 
object during its lifetime.
The allocator has for_each and find_if methods that can run functors on each object managed by the allocator.

Optionally can be constructed in pool mode, in which case the removed/freed objects aren't destroyed, and subsequent
allocation can return one of these without having to call the constructor. This is good when the objects allocate
their own memory buffers and it's desirable to avoid unnecessary freeing and allocations there as well. Obviously,
care must be taken to initialize the initial state of allocated objects in this mode.
All objects are destroyed with the destruction of the allocator object.

Safe to use from a single producer / single consumer threading mode, as long as the working set is
reserved in advance.

@param POOL if true, do not call destructors on item deletion, only on container deletion
@param ATOMIC if true, ins/del operations and versioning are done as atomic operations
@param TRACKING if true, slotalloc will contain data and methods needed for tracking modifications
**/
template<class T, bool POOL=false, bool ATOMIC=false, bool TRACKING=false, class ...Es>
class slotalloc_base
    : protected slotalloc_tracker_base<TRACKING, Es...>
{
    typedef slotalloc_tracker_base<TRACKING, Es...>
        tracker_t;

    typedef typename tracker_t::extarray_t
        extarray_t;

    typedef typename tracker_t::changeset_t
        changeset_t;

public:

    ///Construct slotalloc container
    //@param pool true for pool mode, in which removed objects do not have destructors invoked
    slotalloc_base() : _count(0), _version(0)
    {}

    explicit slotalloc_base( uints reserve_items ) : _count(0), _version(0) {
        reserve(reserve_items);
    }

    ~slotalloc_base() {
        if(!POOL)
            reset();
    }

    

    ///Reset content. Destructors aren't invoked in the pool mode, as the objects may still be reused.
    void reset()
    {
        if(TRACKING)
            mark_all_modified<void>(false);

        //destroy occupied slots
        if(!POOL) {
            for_each([](T& p) {destroy(p);});

            extarray_reset();
            //_relarrays.for_each([&](relarray& ra) {
            //    ra.reset();
            //});
        }

        if(ATOMIC)
            atomic::exchange(&_count, 0);
        else
            _count = 0;

        extarray_reset_count();
        //_relarrays.for_each([&](relarray& ra) {
        //    ra.set_count(0);
        //});

        _array.set_size(0);
        _allocated.set_size(0);
    }

    ///Discard content. Also destroys pooled objects and frees memory
    void discard()
    {
        //destroy occupied slots
        if(!POOL) {
            for_each([](T& p) {destroy(p);});

            extarray_reset_count();
            //_relarrays.for_each([&](relarray& ra) {
            //    ra.set_count(0);
            //});

            _array.set_size(0);
            _allocated.set_size(0);
        }

        if(ATOMIC)
            atomic::exchange(&_count, 0);
        else
            _count = 0;

        _array.discard();
        _allocated.discard();

        extarray_discard();
        //_relarrays.for_each([&](relarray& ra) {
        //    ra.discard();
        //});
    }

/*
    ///Append related array of type R which will be managed alongside the main array of type T
    //@return array index
    //@note types aren't initialized when allocated
    template<typename R>
    uints append_relarray( dynarray<R>** parray = 0 ) {
        uints idx = _relarrays.size();
        relarray* p = new(_relarrays.add_uninit())
            relarray(&type_creator<R>::constructor, &type_creator<R>::destructor);
        p->elemsize = sizeof(T);

        auto& dyn = dynarray<R>::from_dynarray_conforming_ptr(p->data);
        dyn.reserve(_array.reserved_total() / sizeof(T), true);

        if(parray)
            *parray = &dyn;

        return idx;
    }

    //@return value from related array
    template<typename R>
    R* value( const T* v, uints index ) const {
        return reinterpret_cast<R*>(_relarrays[index].item(get_item_id(v)));
    }*/

    template<int V>
    typename std::tuple_element<V,extarray_t>::type::value_type&
        assoc_value( const T* p ) {
        return std::get<V>(*this)[get_item_id(p)];
    }

    template<int V>
    const typename std::tuple_element<V,extarray_t>::type::value_type&
        assoc_value( const T* p ) const {
        return std::get<V>(*this)[get_item_id(p)];
    }

    template<int V>
    typename std::tuple_element<V,extarray_t>::type::value_type&
        value( uints index ) {
        return std::get<V>(*this)[index];
    }

    template<int V>
    const typename std::tuple_element<V,extarray_t>::type::value_type&
        value( uints index ) const {
        return std::get<V>(*this)[index];
    }

    template<int V>
    typename std::tuple_element<V,extarray_t>::type&
        value_array() {
        return std::get<V>(*this);
    }

    template<int V>
    const typename std::tuple_element<V,extarray_t>::type&
        value_array() const {
        return std::get<V>(*this);
    }

    void swap( slotalloc_base& other ) {
        _array.swap(other._array);
        _allocated.swap(other._allocated);
        std::swap(_count, other._count);
        std::swap(_version, other._version);

        extarray_swap(other);
    }

    //@return byte offset to the newly rebased array
    ints reserve( uints nitems ) {
        T* old = _array.ptr();
        T* p = _array.reserve(nitems, true);

        extarray_reserve(nitems);
        //_relarrays.for_each([&](relarray& ra) {
        //    ra.reserve(nitems);
        //});

        return (uints)p - (uints)old;
    }

    ///Insert object
    //@return pointer to the newly inserted object
    T* push( const T& v ) {
        if(_count < _array.size()) {
            T* p = alloc(0);
            if(POOL) destroy(*p);
            return new(p) T(v);
        }
        return new(append()) T(v);
    }

    ///Insert object
    //@return pointer to the newly inserted object
    T* push( T&& v ) {
        if(_count < _array.size()) {
            T* p = alloc(0);
            if(POOL) destroy(*p);
            return new(p) T(v);
        }
        return new(append()) T(v);
    }

    ///Add new object initialized with default constructor
    T* add() {
        return _count < _array.size()
            ? (POOL ? alloc(0) : new(alloc(0)) T)
            : new(append()) T;
    }

    ///Add new object, uninitialized (no constructor invoked on the object)
    //@param newitem optional variable that receives whether the object slot was newly created (true) or reused from the pool (false)
    //@note if newitem == 0 within the pool mode and thus no way to indicate the item has been reused, the reused objects have destructors called
    T* add_uninit( bool* newitem = 0 ) {
        if(_count < _array.size()) {
            T* p = alloc(0);
            if(POOL) {
                if(!newitem) destroy(*p);
                else *newitem = false;
            }
            return p;
        }
        if(newitem) *newitem = true;
        return append();
    }
/*
    //@return id of the next object that will be allocated with add/push methods
    uints get_next_id() const {
        return _unused != reinterpret_cast<const T*>(this)
            ? _unused - _array.ptr()
            : _array.size();
    }*/

    ///Delete object in the container
    void del( T* p )
    {
        uints id = p - _array.ptr();
        if(id >= _array.size())
            throw exception("object outside of bounds");

        DASSERT( get_bit(id) );

        if(TRACKING)
            tracker_t::set_modified(id);

        if(!POOL)
            p->~T();
        clear_bit(id);
        //--_count;
        if(ATOMIC) {
            atomic::dec(&_count);
            atomic::inc(&_version);
        }
        else {
            --_count;
            ++_version;
        }
    }

    ///Delete object by id
    void del( uints id )
    {
        return del(_array.ptr() + id);
    }

    //@return number of used slots in the container
    uints count() const { return _count; }

    //@return true if next allocation would rebase the array
    bool full() const { return (_count + 1)*sizeof(T) > _array.reserved_total(); }

    ///Return an item given id
    //@param id id of the item
    const T* get_item( uints id ) const
    {
        DASSERT( id < _array.size() && get_bit(id) );
        return id < _array.size() ? _array.ptr() + id : 0;
    }

    T* get_item( uints id )
    {
        DASSERT( id < _array.size() && get_bit(id) );
        if(TRACKING)
            tracker_t::set_modified(id);
        return id < _array.size() ? _array.ptr() + id : 0;
    }

    const T& operator [] (uints idx) const {
        return *get_item(idx);
    }

    T& operator [] (uints idx) {
        return *get_item(idx);
    }

    ///Get a particular free slot
    //@param id item id
    //@param is_new optional if not null, receives true if the item was newly created
    T* get_or_create( uints id, bool* is_new=0 )
    {
        if(id < _array.size()) {
            if(TRACKING)
                tracker_t::set_modified(id);

            if(get_bit(id)) {
                if(is_new) *is_new = false;
                return _array.ptr() + id;
            }

            set_bit(id);
            //++_count;
            if(ATOMIC) {
                atomic::inc(&_count);
                atomic::inc(&_version);
            }
            else {
                ++_count;
                ++_version;
            }

            if(is_new) *is_new = true;
            return _array.ptr() + id;
        }

        uints n = id+1 - _array.size();

        extarray_expand();
        //_relarrays.for_each([&](relarray& ra) {
        //    DASSERT( ra.count() == _count );
        //    ra.add(n);
        //});

        _array.add(n);

        if(TRACKING)
            tracker_t::set_modified(id);

        set_bit(id);
        //++_count;
        if(ATOMIC) {
            atomic::inc(&_count);
            atomic::inc(&_version);
        }
        else {
            ++_count;
            --_version;
        }

        if(is_new) *is_new = true;
        return _array.ptr() + id;
    }

    //@return id of given item, or UMAXS if the item is not managed here
    uints get_item_id( const T* p ) const
    {
        uints id = p - _array.ptr();
        return id < _array.size()
            ? id
            : UMAXS;
    }

    //@return true if item is valid
    bool is_valid_item( uints id ) const
    {
        return get_bit(id);
    }

protected:
    ///Helper functions for for_each to allow calling with optional index argument
    template<typename T, typename Callable>
    static auto funccall(Callable c, const T& v, size_t index)
        -> decltype(c(v, index))
    {
        return c(v, index);
    }

    template<typename T, typename Callable>
    static auto funccall(Callable c, const T& v, size_t index)
        -> decltype(c(v))
    {
        return c(v);
    }

    template<typename T, typename Callable>
    static auto funccall(Callable c, T& v, size_t index)
        -> decltype(c(v, index))
    {
        return c(v, index);
    }

    template<typename T, typename Callable>
    static auto funccall(Callable c, T& v, size_t index)
        -> decltype(c(v))
    {
        return c(v);
    }

public:
    ///Invoke a functor on each used item.
    //@note const version doesn't handle array insertions/deletions during iteration
    //@param f functor with ([const] T&) or ([const] T&, size_t index) arguments
    template<typename Func>
    void for_each( Func f ) const
    {
        T const* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1)
                    //f(d[s+i]);
                    funccall(f, d[s+i], s+i);
            }
        }
    }

    ///Invoke a functor on each used item.
    //@note non-const version handles array insertions/deletions during iteration
    //@param f functor with ([const] T&) or ([const] T&, size_t index) arguments
    template<typename Func>
    void for_each( Func f )
    {
        T* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();
        uints version = _version;

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1) {
                    //f(d[s+i]);
                    funccall(f, d[s+i], s+i);

                    if(version != _version) {
                        //handle alterations and rebase
                        d = _array.ptr();
                        ints x = p - b;
                        b = _allocated.ptr();
                        e = _allocated.ptre();
                        p = b + x;
                        m = *p >> i;
                        version = _version;
                    }
                }
            }
        }
    }

    ///Invoke a functor on each used item in given ext array
    //@note const version doesn't handle array insertions/deletions during iteration
    //@param K ext array id
    //@param f functor with ([const] T&) or ([const] T&, size_t index) arguments, with T being the type of given value array
    template<int K, typename Func>
    void for_each_in_array( Func f ) const
    {
        auto d = value_array<K>().ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1)
                    //f(d[s+i]);
                    funccall(f, d[s+i], s+i);
            }
        }
    }

    ///Invoke a functor on each item that was modified between two frames
    //@note const version doesn't handle array insertions/deletions during iteration
    //@param rel_frame relative frame from which to count the modifications, should be <= 0
    //@param f functor with ([const] T* ptr) or ([const] T* ptr, size_t index) arguments; ptr can be null if item was deleted
    template<typename Func, typename = std::enable_if_t<TRACKING>>
    void for_each_modified( int rel_frame, Func f ) const
    {
        bool all_modified = rel_frame < -5*8;

        uint mask = all_modified ? UMAX32 : modified_mask(rel_frame);
        if(!mask)
            return;

        T const* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        auto chs = tracker_t::get_changeset();
        DASSERT( chs->size() >= uints(e - b) );

        const changeset_t* ch = chs->ptr();

        for(uints const* p=b; p!=e; ++p, ++ch) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(all_modified || (ch->mask & mask) != 0) {
                    T const* p = (m&1) != 0 ? d+s+i : 0;
                    funccall(f, p, s+i);
                }
            }
        }
    }


    ///Find first element for which the predicate returns true
    //@return pointer to the element or null
    //@param f functor with ([const] T&) or ([const] T&, size_t index) arguments
    template<typename Func>
    const T* find_if(Func f) const
    {
        T const* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1) {
                    const T& v = d[s+i];
                    if(funccall(f, v, s+i))
                        return &v;
                }
            }
        }

        return 0;
    }

    ///Find first element for which the predicate returns true
    //@return pointer to the element or null
    //@param f functor with ([const] T&) or ([const] T&, size_t index) arguments
    template<typename Func>
    T* find_if(Func f)
    {
        T* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1) {
                    T& v = d[s+i];
                    if(funccall(f, v, s+i))
                        return &v;
                }
            }
        }

        return 0;
    }

    //@{ Get internal array directly
    //@note altering the array directly may invalidate the internal structure
    dynarray<T>& get_array() { return _array; }
    const dynarray<T>& get_array() const { return _array; }
    //@}

    ///Advance to the next tracking frame, updating changeset
    //@return new frame number
    template<typename = std::enable_if_t<TRACKING>>
    uint advance_frame()
    {
        if(!TRACKING)
            return 0;

        auto changeset = tracker_t::get_changeset();
        uint* frame = tracker_t::get_frame();

        update_changeset(*frame, *changeset);

        return ++*frame;
    }

    ///Mark all objects that have the corresponding bit set as modified in current frame
    //@param clear_old if true, old change bits are cleared
    template<typename = std::enable_if_t<TRACKING>>
    void mark_all_modified( bool clear_old )
    {
        dynarray<changeset_t>& changeset = *tracker_t::get_changeset();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        DASSERT( changeset.size() == e - b );

        uint16 preserve = clear_old ? 0xfffeU : 0U;

        changeset_t* d = changeset.ptr();
        static const int NBITS = 8*sizeof(uints);

        for(uints const* p=b; p!=e; ++p, d+=NBITS) {
            uints m = *p;
            if(!m) continue;

            for(int i=0; m && i<NBITS; ++i, m>>=1) {
                if(m&1)
                    d[i].mask = (d[i].mask & preserve) | 1;
            }
        }
    }

private:

    typedef typename std::conditional<ATOMIC, volatile uints, uints>::type uint_type;

    dynarray<T> _array;
    dynarray<uints> _allocated;     //< bit mask for allocated/free items
    uint_type _count;
    uint_type _version;
/*
    ///Related data array that's maintained together with the main one
    struct relarray
    {
        void* data;                 //< dynarray-conformant pointer
        uint elemsize;              //< element size
        void (*constructor)(void*);
        void (*destructor)(void*);

        relarray(
            void (*constructor)(void*),
            void (*destructor)(void*))
            : data(0), elemsize(0), constructor(constructor), destructor(destructor)
        {}

        ~relarray() {
            discard();
        }

        uints count() const { return comm_array_allocator::count(data); }

        void reset() {
            uints n = comm_array_allocator::count(data);
            if(destructor) {
                uint8* p = (uint8*)data;
                for(uints i=0; i<n; ++i, p+=elemsize)
                    destructor(p);
            }
            comm_array_allocator::set_count(data, 0);
        }

        void discard() {
            reset();
            comm_array_allocator::free(data);
            data = 0;
        }

        void set_count( uints n ) {
            comm_array_allocator::set_count(data, n);
        }
        
        void add( uints addn ) {
            uints curn = comm_array_allocator::count(data);
            data = comm_array_allocator::add(data, addn, elemsize);

            if(constructor) {
                uint8* p = (uint8*)data + curn * elemsize;
                for(uints i=0; i<addn; ++i, p+=elemsize)
                    constructor(p);
            }
        }

        void reserve( uints n ) {
            uints curn = comm_array_allocator::count(data);
            data = comm_array_allocator::realloc(data, n, elemsize);
            comm_array_allocator::set_count(data, curn);
        }

        void* item( uints index ) const {
            return (uint8*)data + index * elemsize;
        }
    };

    dynarray<relarray> _relarrays;  //< related data arrays
*/

    ///Helper to expand all ext arrays
    template<size_t... Index>
    void extarray_expand_( coid::index_sequence<Index...> ) {
        extarray_t& ext = *this;
        int dummy[] = { 0, ((void)std::get<Index>(ext).add(1), 0)... };
    }

    void extarray_expand() {
        extarray_expand_(coid::make_index_sequence<tracker_t::extarray_size>());
    }

    ///Helper to reset all ext arrays
    template<size_t... Index>
    void extarray_reset_( coid::index_sequence<Index...> ) {
        extarray_t& ext = *this;
        int dummy[] = { 0, ((void)std::get<Index>(ext).reset(), 0)... };
    }

    void extarray_reset() {
        extarray_reset_(coid::make_index_sequence<tracker_t::extarray_size>());
    }

    ///Helper to set_count(0) all ext arrays
    template<size_t... Index>
    void extarray_reset_count_( coid::index_sequence<Index...> ) {
        extarray_t& ext = *this;
        int dummy[] = { 0, ((void)std::get<Index>(ext).set_size(0), 0)... };
    }

    void extarray_reset_count() {
        extarray_reset_count_(coid::make_index_sequence<tracker_t::extarray_size>());
    }

    ///Helper to discard all ext arrays
    template<size_t... Index>
    void extarray_discard_( coid::index_sequence<Index...> ) {
        extarray_t& ext = *this;
        int dummy[] = { 0, ((void)std::get<Index>(ext).discard(), 0)... };
    }

    void extarray_discard() {
        extarray_discard_(coid::make_index_sequence<tracker_t::extarray_size>());
    }

    ///Helper to reserve all ext arrays
    template<size_t... Index>
    void extarray_reserve_( coid::index_sequence<Index...>, uints size ) {
        extarray_t& ext = *this;
        int dummy[] = { 0, ((void)std::get<Index>(ext).reserve(size, true), 0)... };
    }

    void extarray_reserve( uints size ) {
        extarray_reserve_(coid::make_index_sequence<tracker_t::extarray_size>(), size);
    }

    ///Helper to swap all ext arrays
    template<size_t... Index>
    void extarray_swap_( coid::index_sequence<Index...>, slotalloc_base& other ) {
        extarray_t& ext = *this;
        extarray_t& exto = other;
        int dummy[] = { 0, ((void)std::get<Index>(ext).swap(std::get<Index>(exto)), 0)... };
    }

    void extarray_swap( slotalloc_base& other ) {
        extarray_swap_(coid::make_index_sequence<tracker_t::extarray_size>(), other);
    }


    ///Helper to iterate over all ext arrays
    template<typename F, size_t... Index>
    void extarray_iterate_( coid::index_sequence<Index...>, F fn ) {
        extarray_t& ext = *this;
        int dummy[] = { 0, ((void)fn(std::get<Index>(ext)), 0)... };
    }

    template<typename F>
    void extarray_iterate( F fn ) {
        extarray_iterate_(coid::make_index_sequence<tracker_t::extarray_size>(), fn);
    }


    ///Return allocated slot
    T* alloc( uints* pid )
    {
        DASSERT( _count < _array.size() );

        uint_type* p = _allocated.ptr();
        uint_type* e = _allocated.ptre();
        for(; p!=e && *p==UMAXS; ++p);

        if(p == e)
            *(p = _allocated.add()) = 0;

        uint8 bit = lsb_bit_set((uints)~*p);
        uints slot = ((uints)p - (uints)_allocated.ptr()) * 8;

        uint id = slot + bit;
        if(TRACKING)
            tracker_t::set_modified(id);

        DASSERT( !get_bit(id) );

        if(pid)
            *pid = id;
        //*p |= uints(1) << bit;
        if(ATOMIC) {
            atomic::aor(p, uints(1) << bit);

            atomic::inc(&_count);
            atomic::inc(&_version);
        }
        else {
            *p |= uints(1) << bit;
            ++_count;
            ++_version;
        }
        return _array.ptr() + id;
    }

    T* append()
    {
        uints count = _count;

        DASSERT( count == _array.size() );
        set_bit(count);

        extarray_expand();
/*
        _relarrays.for_each([&](relarray& ra) {
            DASSERT( ra.count() == count );
            ra.add(1);
        });*/

        if(ATOMIC) {
            atomic::inc(&_count);
            atomic::inc(&_version);
        }
        else {
            ++_count;
            ++_version;
        }

        if(TRACKING)
            tracker_t::set_modified(count);

        return _array.add_uninit(1);
    }

    void set_bit( uints k )
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));

        if(ATOMIC)
            atomic::aor(const_cast<uint_type*>(&_allocated.get_or_addc(s)), uints(1) << b);
        else
            _allocated.get_or_addc(s) |= uints(1) << b;
    }

    void clear_bit( uints k )
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));
        
        if(ATOMIC)
            atomic::aand(const_cast<uint_type*>(&_allocated.get_or_addc(s)), ~(uints(1) << b));
        else
            _allocated.get_or_addc(s) &= ~(uints(1) << b);
    }

    bool get_bit( uints k ) const
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));

        if(ATOMIC)
            return s < _allocated.size()
                && (*const_cast<uint_type*>(_allocated.ptr()+s) & (uints(1) << b)) != 0;
        else
            return s < _allocated.size() && (_allocated[s] & (uints(1) << b)) != 0;
    }

    //WA for lambda template error
    void static destroy(T& p) {p.~T();}


    static void update_changeset( uint frame, dynarray<changeset_t>& changeset )
    {
        //the changeset keeps n bits per each element, marking if there was a change in data
        // half of the bits correspond to the given number of most recent frames
        // older frames will be coalesced, containing flags that tell if there was a change in any of the
        // coalesced frames
        //frame aggregation:
        //      8844222211111111 (MSb to LSb)

        changeset_t* chb = changeset.ptr();
        changeset_t* che = changeset.ptre();

        //make space for a new frame

        bool b8 = (frame & 7) == 0;
        bool b4 = (frame & 3) == 0;
        bool b2 = (frame & 1) == 0;

        for(changeset_t* ch = chb; ch < che; ++ch) {
            uint16 v = ch->mask;
            uint16 vs = (v << 1) & 0xaeff;                 //shifted bits
            uint16 va = ((v << 1) | (v << 2)) & 0x5100;    //aggregated bits
            uint16 vx = vs | va;

            uint16 xc000 = (b8 ? vx : v) & (3<<14);
            uint16 x3000 = (b4 ? vx : v) & (3<<12);
            uint16 x0f00 = (b2 ? vx : v) & (15<<8);
            uint16 x00ff = vs & 0xff;

            ch->mask = xc000 | x3000 | x0f00 | x00ff;
        }
    }

    static uint modified_mask( int rel_frame )
    {
        if(rel_frame > 0)
            return 0;

        if(rel_frame < -5*8)
            return UMAX32;     //too old, everything needs to be considered as modified

        //frame changes aggregated like this: 8844222211111111 (MSb to LSb)
        // compute the bit plane of the relative frame
        int r = -rel_frame;
        int bitplane = 0;

        for(int g=0; r>0 && g<4; ++g) {
            int b = r >= 8 ? 8 : r;
            r -= 8;

            bitplane += b >> g;
        }

        if(r > 0)
            ++bitplane;

        return uint(1 << bitplane) - 1;
    }
};

//variants of slotalloc

template<class T, class ...Es>
using slotalloc = slotalloc_base<T,false,false,false,Es...>;

template<class T, class ...Es>
using slotalloc_pool = slotalloc_base<T,true,false,false,Es...>;

template<class T, class ...Es>
using slotalloc_atomic_pool = slotalloc_base<T,true,true,false,Es...>;

template<class T, class ...Es>
using slotalloc_tracking_pool = slotalloc_base<T,true,false,true,Es...>;

template<class T, class ...Es>
using slotalloc_tracking_atomic_pool = slotalloc_base<T,true,true,true,Es...>;

template<class T, class ...Es>
using slotalloc_atomic = slotalloc_base<T,false,true,false,Es...>;

template<class T, class ...Es>
using slotalloc_tracking_atomic = slotalloc_base<T,false,true,true,Es...>;

template<class T, class ...Es>
using slotalloc_tracking = slotalloc_base<T,false,false,true,Es...>;


COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_SLOTALLOC__HEADER_FILE__

