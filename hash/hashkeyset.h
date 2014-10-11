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
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#ifndef __COID_COMM_HASHKEYSET__HEADER_FILE__
#define __COID_COMM_HASHKEYSET__HEADER_FILE__


#include "../namespace.h"
#include "hashtable.h"
#include <functional>

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
//@{ EXTRACTKEY templates

///Extracts key object from the value by casting operator
template <class VAL, class KEY>
struct _Select_Copy
{
    typedef KEY ret_type;

    ret_type operator()(const VAL& t) const   { return t; }
};

///Extracts key object from the value pointer by dereferencing and casting
template <class VAL, class KEY>
struct _Select_CopyPtr
{
    typedef KEY ret_type;

    ret_type operator()(const VAL* t) const   { return *t; }
};

///Extracts key reference from the value by casting operator
template <class VAL, class KEY>
struct _Select_GetRef
{
    typedef const KEY& ret_type;

    ret_type operator()(const VAL& t) const   { return t; }
};

///Extracts key reference from the value pointer by dereferencing and casting
template <class VAL, class KEY>
struct _Select_GetRefPtr
{
    typedef const KEY& ret_type;

    ret_type operator()(const VAL* t) const   { return *t; }
};

template <class VAL, class KEY>
struct _Select_DeRef
{
	typedef KEY ret_type;

	ret_type operator()(const VAL& t) const { return *t; }
};

//@} EXTRACTKEY templates

////////////////////////////////////////////////////////////////////////////////
/**
@class hash_keyset
@param VAL value type stored in hash table
@param EXTRACTKEY key extractor from value type, EXTRACTKEY::ret_type is the type extracted
@param HASHFUNC hash function, HASHFUNC::key_type should be the type used for lookup
@param EQFUNC equality functor, comparing EXTRACTKEY::ret_type extracted from value with HASHFUNC::key_type lookup key
**/
template <
    class VAL,
    class EXTRACTKEY,
    class HASHFUNC=hash<typename type_base<typename EXTRACTKEY::ret_type>::type>,
    class EQFUNC=equal_to<typename type_base<typename EXTRACTKEY::ret_type>::type, typename HASHFUNC::key_type>,
    class ALLOC=comm_allocator<VAL>
    >
class hash_keyset
    : public hashtable<VAL,HASHFUNC,EQFUNC,EXTRACTKEY,ALLOC>
{
    typedef hashtable<VAL,HASHFUNC,EQFUNC,EXTRACTKEY,ALLOC> _HT;

public:

    typedef typename _HT::LOOKUP                    key_type;
    typedef VAL                                     value_type;
    typedef EXTRACTKEY                              extractor;
    typedef HASHFUNC                                hasher;
    typedef EQFUNC                                  key_equal;

    typedef size_t                                  size_type;
    typedef ptrdiff_t                               difference_type;
    typedef value_type*                             pointer;
    typedef const value_type*                       const_pointer;
    typedef value_type&                             reference;
    typedef const value_type&                       const_reference;

    typedef typename _HT::iterator                  iterator;
    typedef typename _HT::const_iterator            const_iterator;

    std::pair<iterator, bool> insert( const value_type& val )
    {   return insert_unique(val);    }

    void insert( const value_type* f, const value_type* l )
    {   insert_unique( f, l );   }

    void insert( const_iterator f, const_iterator l ) 
    {   insert_unique( f, l );   }

    ///Insert value if it's got an unique key
    ///@note the key needed for the insertion is extracted from the value using the extractor object provided in the constructor
    ///@return NULL if the value could not be inserted, or a constant pointer to the value
    const VAL* insert_value( const value_type& val )
    {
        typename _HT::Node** v = this->_copy_insert_unique(val);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Insert value if it's got an unique key, swapping the provided value
    ///@note the key needed for the insertion is extracted from the value using the extractor object provided in the constructor
    ///@return NULL if the value could not be inserted, or a constant pointer to the value
    const VAL* swap_insert_value( value_type& val )
    {
        typename _HT::Node** v = this->_swap_insert_unique(val);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Insert new value or override the existing one under the same key.
    ///@note the key needed for the insertion is extracted from the value using the extractor object provided in the constructor
    ///@return constant pointer to the value
    const VAL* insert_or_replace_value( const value_type& val )
    {
        typename _HT::Node** v = this->_copy_insert_unique__replace(val);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Insert new value or override the existing one under the same key, swapping the content.
    ///@note the key needed for the insertion is extracted from the value using the extractor object provided in the constructor
    ///@return constant pointer to the value
    const VAL* swap_insert_or_replace_value( value_type& val )
    {
        typename _HT::Node** v = this->_swap_insert_unique__replace(val);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Create an empty entry for value object that will be initialized by the caller afterwards
    ///@note the value object should be initialized so that it would return the same key as the one passed in here
    ///@param key the key under which the value object should be created
    VAL* insert_value_slot( const key_type& key )
    {
        typename _HT::Node** v = _HT::_insert_unique_slot(key);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Find or create an empty entry for value object that will be initialized by the caller afterwards
    ///@note the value object should be initialized so that it would return the same key as the one passed in here
    ///@param key the key under which the value object should be created
    VAL* find_or_insert_value_slot( const key_type& key, bool* isnew=0 )
    {
        typename _HT::Node** v = _HT::_find_or_insert_slot(key, isnew);
        return &(*v)->_val;
    }


    ///Find value object corresponding to given key
    const VAL* find_value( const key_type& k ) const
    {
        const typename _HT::Node* v = _HT::find_node(k);
        return v ? &v->_val : 0;
    }

    ///Find value object corresponding to given key
    const VAL* find_value( uint hash, const key_type& k ) const
    {
        const typename _HT::Node* v = _HT::find_node(hash,k);
        return v ? &v->_val : 0;
    }


    hash_keyset()
        : _HT( 128, hasher(), key_equal(), extractor() ) {}

    explicit hash_keyset( const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex ) {}
    hash_keyset( const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex ) {}
    hash_keyset( const extractor& ex, const hasher& hf, const key_equal& eql, size_type n=128 )
        : _HT( n, hf, eql, ex ) {}



    hash_keyset( const value_type* f, const value_type* l, size_type n=128 )
        : _HT( n, hasher(), key_equal(), extractor() )
    {
        insert_unique( f, l );
    }
    hash_keyset( const value_type* f, const value_type* l,
        const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_keyset( const value_type* f, const value_type* l,
        const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_keyset( const value_type* f, const value_type* l,
        const extractor& ex, const hasher& hf, const key_equal& eqf, size_type n=128 )
        : _HT( n, hf, eqf, ex )
    {
        insert_unique( f, l );
    }


    hash_keyset( const_iterator* f, const_iterator* l, size_type n=128 )
        : _HT( n, hasher(), key_equal(), extractor() )
    {
        insert_unique( f, l );
    }
    hash_keyset( const_iterator* f, const_iterator* l,
        const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_keyset( const_iterator* f, const_iterator* l,
        const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_keyset( const_iterator* f, const_iterator* l,
        const extractor& ex, const hasher& hf, const key_equal& eqf, size_type n=128 )
        : _HT( n, hf, eqf, ex )
    {
        insert_unique( f, l );
    }
};


////////////////////////////////////////////////////////////////////////////////
/**
@class hash_keyset
@param VAL value type stored in hash table
@param EXTRACTKEY key extractor from value type, EXTRACTKEY::ret_type is the type extracted
@param HASHFUNC hash function, HASHFUNC::key_type should be the type used for lookup
@param EQFUNC equality functor, comparing EXTRACTKEY::ret_type extracted from value with HASHFUNC::key_type lookup key
**/
template <
    class VAL,
    class EXTRACTKEY,
    class HASHFUNC=hash<typename type_base<typename EXTRACTKEY::ret_type>::type>,
    class EQFUNC=equal_to<typename type_base<typename EXTRACTKEY::ret_type>::type, typename HASHFUNC::key_type>,
    class ALLOC=comm_allocator<VAL>
    >
class hash_multikeyset
    : public hashtable<VAL,HASHFUNC,EQFUNC,EXTRACTKEY,ALLOC>
{
    typedef hashtable<VAL,HASHFUNC,EQFUNC,EXTRACTKEY,ALLOC> _HT;

public:

    typedef typename _HT::LOOKUP                    key_type;
    typedef VAL                                     value_type;
    typedef EXTRACTKEY                              extractor;
    typedef HASHFUNC                                hasher;
    typedef EQFUNC                                  key_equal;

    typedef size_t                                  size_type;
    typedef ptrdiff_t                               difference_type;
    typedef value_type*                             pointer;
    typedef const value_type*                       const_pointer;
    typedef value_type&                             reference;
    typedef const value_type&                       const_reference;

    typedef typename _HT::iterator                  iterator;
    typedef typename _HT::const_iterator            const_iterator;
    
    iterator insert( const value_type& val )
    {   return insert_equal(val);    }

    void insert( const value_type* f, const value_type* l )
    {   insert_equal( f, l );   }

    void insert( const_iterator f, const_iterator l ) 
    {   insert_equal( f, l );   }

    const VAL* insert_value( const value_type& val )
    {
        typename _HT::Node** v = this->_copy_insert_equal(val);
        return v  ?  &(*v)->_val  :  0;
    }

    const VAL* swap_insert_value( value_type& val )
    {
        typename _HT::Node** v = this->_swap_insert_equal(val);
        return v  ?  &(*v)->_val  :  0;
    }

    VAL* insert_value_slot( const key_type& key )
    {
        typename _HT::Node** v = _insert_equal_slot(key);
        return v  ?  &(*v)->_val  :  0;
    }

    VAL* find_or_insert_value_slot( const key_type& key, bool* isnew=0 )
    {
        typename _HT::Node** v = _find_or_insert_slot(key, isnew);
        return &(*v)->_val;
    }


    const VAL* find_value( const key_type& k ) const
    {
        const typename _HT::Node* v = this->find_node(k);
        return v ? &v->_val : 0;
    }


    hash_multikeyset()
        : _HT( 128, hasher(), key_equal(), extractor() ) {}

    explicit hash_multikeyset( const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex ) {}
    hash_multikeyset( const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex ) {}
    hash_multikeyset( const extractor& ex, const hasher& hf, const key_equal& eql, size_type n=128 )
        : _HT( n, hf, eql, ex ) {}



    hash_multikeyset( const value_type* f, const value_type* l, size_type n=128 )
        : _HT( n, hasher(), key_equal(), extractor() )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const value_type* f, const value_type* l,
        const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const value_type* f, const value_type* l,
        const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const value_type* f, const value_type* l,
        const extractor& ex, const hasher& hf, const key_equal& eqf, size_type n=128 )
        : _HT( n, hf, eqf, ex )
    {
        insert_unique( f, l );
    }


    hash_multikeyset( const_iterator* f, const_iterator* l, size_type n=128 )
        : _HT( n, hasher(), key_equal(), extractor() )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const_iterator* f, const_iterator* l,
        const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const_iterator* f, const_iterator* l,
        const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const_iterator* f, const_iterator* l,
        const extractor& ex, const hasher& hf, const key_equal& eqf, size_type n=128 )
        : _HT( n, hf, eqf, ex )
    {
        insert_unique( f, l );
    }
};


////////////////////////////////////////////////////////////////////////////////
template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline binstream& operator << ( binstream& bin, const hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{
    typedef hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC> HT;
    typename HT::hashtable_binstream_container bc(a);
    opcd e = bin.write_array(bc);

    if(e) throw exception(e);
    return bin;
}

////////////////////////////////////////////////////////////////////////////////
template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline binstream& operator >> ( binstream& bin, hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{
    typedef hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC> HT;
    typename HT::hashtable_binstream_container bc(a, UMAXS);
    opcd e = bin.read_array(bc);

    if(e) throw exception(e);
    return bin;
}

////////////////////////////////////////////////////////////////////////////////
template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline binstream& operator << ( binstream& bin, const hash_multikeyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{
    typedef hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC> HT;
    typename HT::hashtable_binstream_container bc(a);
    opcd e = bin.write_array(bc);

    if(e) throw exception(e);
    return bin;
}

////////////////////////////////////////////////////////////////////////////////
template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline binstream& operator >> ( binstream& bin, hash_multikeyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{
    typedef hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC> HT;
    typename HT::hashtable_binstream_container bc(a, UMAXS);
    opcd e = bin.read_array(bc);

    if(e) throw exception(e);
    return bin;
}


////////////////////////////////////////////////////////////////////////////////
template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline metastream& operator << ( metastream& m, const hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{
    m.meta_decl_array();
    m << *(const VAL*)0;
    return m;
}

////////////////////////////////////////////////////////////////////////////////
template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline metastream& operator || ( metastream& m, hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{
    typedef hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC> _HT;

    if(m.stream_reading()) {
        a.clear();
        typename _HT::hashtable_binstream_container bc(a,0,0);
        return m.read_container<VAL>(bc);
    }
    else if(m.stream_writing()) {
        typename _HT::hashtable_binstream_container bc(a,0,0);
        return m.write_container<VAL>(bc);
    }
    else {
        m.meta_decl_array();
        m || *(VAL*)0;
    }

    return m;
}

template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline metastream& operator << ( metastream& m, const hash_multikeyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{
    m.meta_decl_array();
    m << *(const VAL*)0;
    return m;
}

template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline metastream& operator || ( metastream& m, hash_multikeyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{
    typedef typename hash_keyset<VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC> _HT;

    if(m._binr) {
        a.clear();
        typename _HT::hashtable_binstream_container bc(a,0,0);
        return m.xread_container<VAL>(bc);
    }
    else if(m._binw) {
        typename _HT::hashtable_binstream_container bc(a,0,0);
        return m.xwrite_container<VAL>(bc);
    }
    else {
        m.meta_decl_array();
        m || *(VAL*)0;
    }

    return m;
}


COID_NAMESPACE_END

#endif //__COID_COMM_HASHKEYSET__HEADER_FILE__
