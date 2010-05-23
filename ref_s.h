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
* Ladislav Hrabcak
* Portions created by the Initial Developer are Copyright (C) 2007
* the Initial Developer. All Rights Reserved.
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

#ifndef __COMM_REF_S_H__
#define __COMM_REF_S_H__

#include "commassert.h"
#include "ref_base.h"
#include "binstream/binstream.h"
#include "metastream/metastream.h"

struct create_me { };

struct create_pooled { };

static create_me CREATE_ME;

static create_pooled CREATE_POOLED;

template<class T> class pool;
template<class T> class policy_queue_pooled;

namespace atomic {
	template<class T> class queue_ng;
}

template<class T>
class ref 
{
public:
	friend atomic::queue_ng<T>;

public:
	typedef ref<T> ref_t;
	typedef pool<policy_base> pool_t;
	typedef policy_shared_base policy; 
    typedef policy_queue_pooled<T> policy_pooled;

	ref() : _p(0), _o(0){}

	// from T* constructors
	explicit ref( T* o )
		: _p( policy_trait<T>::policy::create(o) )
		, _o(o) {}

	explicit ref( policy_shared<T>* p )
		: _p( p )
		, _o(p->get()) {}

	void create(T* const p) { 
		release();
		_o=p;
		_p=policy_trait<T>::policy::create(p);
	}

	// special constructor from default policy
	explicit ref( const create_me& ) {
		policy_trait<T>::policy *p=policy_trait<T>::policy::create();
		_p=p;
		_o=p->get();
	}

	// special constructor from default policy
	explicit ref( const create_pooled&) {
		policy_queue_pooled<T> *p=policy_queue_pooled<T>::create();
		_p=p;
		_o=p->get();
	}

	// special constructor from default policy
	explicit ref( const create_pooled&, pool<policy_pooled> *po) {
		policy_queue_pooled<T> *p=policy_queue_pooled<T>::create(po);
		_p=p;
		_o=p->get();
	}

	// copy constructors
	ref( const ref<T>& p )
		: _p( p.add_ref_copy() )
		, _o( p.get() ) {}

	// constructor from inherited object
	template< class T2 >
	explicit ref( const ref<T2>& p )
		: _p( p.add_ref_copy() )
		, _o( p.get() ) {}

	void create(policy_shared<T> * const p) {
		release();
		_p=p;
		_o=p->get();
	}

	void create() {
		release();
		policy_trait<T>::policy *p = policy_trait<T>::policy::create();
		_p=p;
		_o=p->get();
	}

	void create_pooled() {
		release();
		policy_queue_pooled<T> *p = policy_queue_pooled<T>::create();
		_p=p;
		_o=p->get();
	}

	void create_pooled(pool<policy_pooled> *po) {
		release();
		policy_queue_pooled<T> *p=policy_queue_pooled<T>::create(po);
		_p=p;
		_o=p->get();
	}

	// standard destructor
	~ref() { if( _p ) { _p->release(); _p=0; _o=0; } }

	const ref_t& operator=(const ref_t& r) {
        release();
		_p=r.add_ref_copy();
		_o=r._o;
		return *this;
	}

	template< class T2 >
	const ref_t& operator=(const ref<T2>& r) {
        release();
		_p=r.add_ref_copy();
		_o=r.get();
		return *this;
	}

	/// DO NOT USE !!!
	policy* add_ref_copy() const { if( _p ) _p->add_ref_copy(); return _p; }

	T * operator->() const { DASSERT( _p!=0 && "You are trying to use not initialized REF!" ); return _o; }

	T * operator->() { DASSERT( _p!=0 && "You are trying to use not initialized REF!" ); return _o; }

	T & operator*() const { return *_o; }

	void swap(ref_t& rhs) {
		policy* tmp_p = _p;
		T* tmp_o = _o;
		_p = rhs._p;
		_o = rhs._o;
		rhs._p = tmp_p;
		rhs._o = tmp_o;
	}

	void release() {
		if( _p!=0 ) { 
			_p->release(); _p=0; _o=0; 
		} 
	}

	void create(pool_t* p) {
		release();
		_p = policy_trait<T>::policy::create(p); 
		_o = _p->object_ptr(); 
	}

	T* get() const { return _o; }

	bool is_empty() const { return (_p==0); }

	void forget() { _p=0; _o=0; }

	template<class T2>
	void takeover(ref<T2>& p) {
		release();
		_o = p.get();
		_p = p.give_me();
		p.forget();
	}

	policy* give_me() { policy* tmp=_p; _p=0;_o=0; return tmp; }

	coid::int32 refcount() const { return _p?_p->refcount():0; }

	friend coid::binstream& operator<<( coid::binstream& bin,const ref<T>& s ) {
		return bin<<(*s._o); 
	}

	friend coid::binstream& operator>>( coid::binstream& bin,ref<T>& s ) { 
		s.create(); return bin>>(*s._o); 
	}

	friend coid::metastream& operator<<( coid::metastream& m,const ref<T>& s ) {
		MSTRUCT_OPEN(m,"ref")
			MMAT(m, "ptr", object)
		MSTRUCT_CLOSE(m)
	}

private:
	policy *_p;
	T *_o;
};

template<class T> 
inline bool operator==( const ref<T>& a,const ref<T>& b ) {
	return a.get()==b.get();
}

template<class T> 
inline bool operator!=( const ref<T>& a,const ref<T>& b ){
	return !operator==(a,b);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#endif // __COMM_REF_S_H__
