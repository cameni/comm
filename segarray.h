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

#ifndef __COID_COMM_SEGARRAY__HEADER_FILE__
#define __COID_COMM_SEGARRAY__HEADER_FILE__

#include "namespace.h"

#include "local.h"
#include "dynarray.h"
#include "radix.h"
#include "assert.h"
#include "alloc/chunkalloc.h"
#include "binstream/stlstream.h"
//#include <malloc.h>
#include <fstream>

COID_NAMESPACE_BEGIN

///Segmented linear container
/*!
  Chain of segments (pages) keeping arrays of specified type.

  The segarray class keeps its data within chained segments. A subclass segarray::ptr defines a 'pointer'
  to items of the container, with operators ++,-- (prefix), T& [], T* (), * and ->, and methods like
  ins(n) and del(n) which can insert resp. delete items from the container.
  The segarray class allows to obtain ptr to specified item via get_ptr, to insert and delete items via the
  ins and del methods; defines operators [] and () to get items from the container, and more
*/

///Empty class
class EmptyTail {
    friend binstream& operator << (binstream& out, const EmptyTail& p) { return out;
    }
    friend binstream& operator >> (binstream& in, EmptyTail& p) { return in;
    }
};


////////////////////////////////////////////////////////////////////////////////
//Fw
#ifndef SYSTYPE_MSVC
template<class T, class TAIL> class segarray;
template<class T, class TAIL> binstream& operator << (binstream&, const segarray<T,TAIL>& );
template<class T, class TAIL> binstream& operator >> (binstream&, segarray<T,TAIL>& );
#endif //SYSTYPE_MSVC

///Segmented array
template <class T, class TAIL = EmptyTail>
class segarray
{
public:
    struct segment;
	struct ptr;

    struct PERSIST
    {
        segarray<T,TAIL>* array;

        void* context;                  ///< context pointer
        void* segdata;                  ///< pointer to raw segment start
        uints segsize;                  ///< raw segment size

        T*   data;                      ///< ptr to useful data in the segment
        uints chunksize;                ///< size of one element in bytes (for dynamic TAILs)
        uints first;                     ///< index of the first element in the segment
        uints count;                     ///< number of useful elements in the segment

        uints segmentno;                 ///< segment ordinal
        uints fileblock;                 ///< file block number the segment should be persisted to

        bool bdestroy;                  ///< set if the segment is going to be destroyed after saving
        bool bprimeload;                ///< set if the segment is to be loaded from persistent storage (1st time load)
    };

    ///stream function prototype
    typedef opcd (*fnc_stream)( const PERSIST& arg );

    COIDNEWDELETE

protected:
    ///_flags enum
    enum {
        fSEQ_INSERT                 = 1,    ///< hint: sequential insert
        fSIZE_ZERO                  = 2,    ///< size of T should be zero
        fTRIVIAL_CONSTRUCTOR        = 4,
        fTRIVIAL_DESTRUCTOR         = 8,

        fSEG_PRIMELOAD              = 0x100,

        DEF_RSEGSIZE                = 12,   ///< default segment size, 4k
    };

public:
    ///Segment structure of segarray container
    /**
        If _pseg is null the segment has been swapped out and must be streamed back
    **/
    struct segment
    {
        friend class segarray;
		friend struct ptr;
    private:
        char*       _pseg;                      ///<segment start
        uints        _usdpos;                   ///<used block position
        uints        _begidx;                   ///<first item global index
        uints        _nitems;                   ///<no.of items present
        uints        _segitems;                 ///<number of items in segment
        segarray*   _array;                     ///<shared segment array
        ushort      _chunksize;                 ///<item byte size
        ushort      _ntail;                     ///<tail element count per record
        uint        _flags;                     ///<flags from segarray::_flags

        mutable uints _lrui;                    ///<last recently used iteration
        uints        _nref;                     ///<no.of references to the segment
        uints        _swapid;                   ///<id of corresponding swap segemnt

        size_t sizeof_T() const                 { return (_flags & fSIZE_ZERO) ? 0 : sizeof(T); }
        
        TAIL* ptr_tail( const void* p ) const   { return (TAIL*)((char*)p + sizeof_T()); }

    public:

        bool is_prime_load() const              { return (_flags & fSEG_PRIMELOAD) != 0; }
        void clear_prime_load()                 { _flags &= ~fSEG_PRIMELOAD; }
        void set_prime_load()                   { _flags |= fSEG_PRIMELOAD; }

        opcd save_descriptor( binstream& bin ) const
        {
            bin << _usdpos << _begidx << _nitems << _swapid;
            return 0;
        }

        opcd load_descriptor( binstream& bin )
        {
            bin >> _usdpos >> _begidx >> _nitems >> _swapid;
            return 0;
        }

        COIDNEWDELETE

        uints   used_size() const                { return _nitems*_chunksize; }
        uints   free_size() const                { return (_segitems - _nitems)*_chunksize; }
        uints   free_size_before() const         { return _usdpos*_chunksize; }
        uints   free_size_after() const          { return (_segitems - _nitems - _usdpos)*_chunksize; }
        uints   used_size_before(uints a) const   { return a*_chunksize; }
        uints   used_size_after(uints a) const    { return (_nitems-a)*_chunksize; }

        uints   used_count() const               { return _nitems; }
        uints   free_count() const               { return _segitems - _nitems; }
        uints   free_count_before() const        { return _usdpos; }
        uints   free_count_after() const         { return _segitems - _nitems - _usdpos; }
        
        uints   used_count_before(uints a) const  { return a - _begidx; }
        uints   used_count_after(uints a) const   { return _nitems - a + _begidx; }

        uints   bgi_index() const                { return _begidx; }

		uints   get_chunksize() const			{ return _chunksize; }
        uints   tail_element_count() const       { return _ntail; }

        void   refcnt_inc()                     { ++_nref; }
        void   refcnt_dec()                     { --_nref; }
        bool   is_locked() const                { return _nref > 0; }

        void   lrui_upd( uints& lrui ) const     { _lrui = ++lrui; }
        uints   get_lrui() const                 { return _lrui; }


        uints  getset_swapid( uints& heap )
        {
            if( _swapid != UMAX )
                return _swapid;
            return _swapid = heap++;
        }

		bool   is_valid_ptr( const void* p ) const	{ return p<end_ptr() && p>=bgi_ptr(); }

        opcd   get_info( uints* first, uints* nitems, uints* offs ) const
        {
            if(first)   *first = _begidx;
            if(nitems)  *nitems = _nitems;
            if(offs)    *offs = _usdpos;
            return 0;
        }

        //dynarray< local<segment> >* get_segments()  { return &_segments; }
        void   set_array( segarray* a )
        {
            _array = a;
            DASSERT( !a || a->_segitems == _segitems );
        }

        //friend binstream& operator << TEMPLFRIEND ( binstream& out, const segment& seg );
        //friend binstream& operator >> TEMPLFRIEND ( binstream& in, segment& seg );
    public:
        binstream& stream_out ( binstream& out ) const
        {
            out << _usdpos << _nitems << _begidx << get_raw_segsize() << tail_element_count();
            //out.write(bgi_ptr(), _nitems*_chunksize);
            T* p = (T*) bgi_ptr();
            for( uints i=0; i<_nitems; ++i, p=(T*)((char*)p+_chunksize) )
            {
                out << *p;
                TAIL* t = ptr_tail(p);
                for( uints j=0; j<tail_element_count(); ++j )
                    out << t[j];
            }
            return out;
        }
        binstream& stream_in ( binstream& in )
        {
            uints segsize, ntail;

            in >> _usdpos >> _nitems >> _begidx >> segsize >> ntail;

            T* p = get_first();
            for( uints i=0; i<_nitems; ++i, p=(T*)((char*)p+_chunksize) )
            {
                new(p) T;
                in >> *p;
                TAIL* t = ptr_tail (p);
                for (uints j=0; j<ntail; ++j)
                {
                    new(t+j) TAIL;
                    in >> t[j];
                }
            }
            return in;
        }

    private:
        char*  end_ptr() const                  { return _pseg + (_usdpos+_nitems)*_chunksize; }
        char*  bgi_ptr() const                  { return _pseg + _usdpos*_chunksize; }

        char*  end_ptr(ints idx) const          { return _pseg + (_usdpos+_nitems-idx)*_chunksize; }
        char*  bgi_ptr(ints idx) const          { return _pseg + (_usdpos+idx)*_chunksize; }

    public:
        uints   index_behind() const            { return _begidx + _nitems; }
        uints   index_before() const            { return _begidx - 1; }

        void   adjust( const segment& seg )     { _begidx = seg._begidx + seg._nitems; }
        void   set_index( uints i )             { _begidx = i; }

        uints get_index( const T* p ) const     { return _begidx + ((char*)p - bgi_ptr())/_chunksize; }

        bool inc_ptr( T*& p ) const
        { 
            p = (T*) ((char*)p + _chunksize);
            return (char*)p < end_ptr();
        }

        bool dec_ptr( T*& p ) const
        {
            p = (T*) ((char*)p - _chunksize);
            return (char*)p >= bgi_ptr();
        }

        T* get_first() const                    { return (T*) bgi_ptr(); }
        T* get_last() const                     { return (T*) end_ptr(1); }

        T* get_at(uints a) const                { RASSERTX(a-_begidx < _nitems, "segment not containg the item"); return (T*) bgi_ptr(a-_begidx); }
        bool is_at(uints a) const               { return (a-_begidx >= _nitems)  ?  false  :  true; }

        uints get_raw_segsize() const           { return 1<<_array->_rsegsize; }

        T* ins( uints pos, bool forward, uints n=1 )
        {
            uints m = n*_chunksize;
            uints nt = tail_element_count();
            char* p = bgi_ptr(pos);

            if(forward)
            {
                DASSERTX( n <= free_count_after(), "invalid params" );
                memmove( p+m, p, used_size_after(pos) );
                _nitems += n;
                for( uints i=0; i<m; i+=_chunksize )
                {
                    new (p+i) T;
                    TAIL* t = ptr_tail(p+i);
                    for (uints j=0; j<nt; ++j)  new(t+j) TAIL;
                }
            }
            else
            {
                DASSERTX( n <= free_count_before(), "invalid params" );
                uints bf = pos * _chunksize;
				if(bf)
					memmove( bgi_ptr()-m, bgi_ptr(), bf );
                //_pusd -= m;
                _usdpos -= n;
                _nitems += n;
                p -= m;
                for( uints i=0; i<m; i+=_chunksize)
                {
                    new (p+i) T;
                    TAIL* t = ptr_tail(p+i);
                    for (uints j=0; j<nt; ++j)  new(t+j) TAIL;
                }
            }
            return (T*)p;
        }

        void del( uints pos, uints n=1 )
        {
            uints m = n*_chunksize;
            uints nt = tail_element_count();
            char* p = bgi_ptr(pos);

            if( !(_flags & fTRIVIAL_DESTRUCTOR) )
            {
                for( uints i=0; i<m; i+=_chunksize )
                {
                    ((T*)(p+i))->~T();
                    TAIL* t = ptr_tail(p+i);
                    for( uints j=0; j<nt; ++j )  t[j].~TAIL();
                }
            }

            uints af = _nitems - n - pos;
            if( pos >= af )
            {
                if(af)
                    memmove( p, p+m, af*_chunksize );
            }
            else
            {
                _usdpos += n;
                if(pos)
                {
                    uints bf = pos * _chunksize;
                    memmove( p+m-bf, p-bf, bf );
                }
            }

            _nitems -= n;
        }

        T*  insm( uints pos, uints n=1 )
        {
            if( free_count_after() >= n )
            {
                if( free_count_before() >= n )
                {
                    if( pos < _nitems/2 )
                        return ins( pos, false, n );
                    else
                        return ins( pos, true, n );
                }
                else
                    return ins( pos, true, n );
            }
            else if( free_count_before() >= n )
                return ins( pos, false, n );
            else
            {
                uints fwd = n >> 1;
                uints bkw = n - fwd;
                if( fwd > free_count_after() )          { fwd = free_count_after(); bkw = n - fwd; }
                else if( bkw > free_count_before() )    { bkw = free_count_before(); fwd = n - bkw; }
                ins( pos, true, fwd );
                return ins( pos, false, bkw );
            }
        }

        void move( segment &seg, bool forward, uints n )
        {
            uints m = n*_chunksize;
            if(forward)
            {
                xmemcpy( seg.insm( 0, n ), end_ptr()-m, m );
                del( used_count()-n, n );
                seg._begidx = _begidx + used_count();
            }
            else
            {
                xmemcpy( seg.insm( seg.used_count(), n ), bgi_ptr(), m );
                del( _usdpos, n );
                _begidx += n;
            }
        }

        segment( ushort chunksize = 0 )
        {
            _pseg = 0;
            _begidx = UMAX;
            _nitems = 0;
            _usdpos = 0;
            _chunksize = chunksize;
            _ntail = 0;
            _flags = fSEG_PRIMELOAD;
            _segitems = 0;
            _array = 0;
            _nref = 0;
            _swapid = UMAX;
            _lrui = 0;
        }

        ~segment()
        {
            reset();
            if(_pseg)
                free_mem();
        }

        segment( const segment& seg )
        {
            _chunksize = seg._chunksize;
            _ntail = seg._ntail;
            _segitems = seg._segitems;
            _usdpos = seg._usdpos;
            _begidx = seg._begidx;
            _nitems = seg._nitems;
            _nref = 0;
            _swapid = UMAX;
            _flags = seg._flags;

            _pseg = seg._array.seg_alloc();
            xmemcpy( bgi_ptr(), seg.bgi_ptr(), seg.used_size() );

            _array = 0;//seg._array;
            //_segments.share( (dynarray< local<segment> >&)seg._segments );
        }

        segment& operator = (const segment& seg)
        {
            reset();
            _chunksize = seg._chunksize;
            _ntail = seg._ntail;
            _segitems = seg._segitems;
            _usdpos = seg._usdpos;
            _begidx = seg._begidx;
            _nitems = seg._nitems;
            _nref = 0;
            _swapid = UMAX;
            _flags = seg._flags;

            if( !_pseg )
                _pseg = _array.seg_alloc();
            xmemcpy( bgi_ptr(), seg.bgi_ptr(), seg.used_size() );

            //_array = seg._array;
            //_segments.share( (dynarray< local<segment> >&)seg._segments );
            return *this;
        }
/*
        void share( dynarray< local<segment> >& syn )
        {
            _segments.unshare();
            _segments.share( syn );
        }
*/
        segment* alloc( uints idx, segarray<T,TAIL>* a )
        {
            if( idx == UMAX )
                idx = 0;
            _chunksize = (ushort)a->item_size();
            _ntail = (ushort)a->tail_element_count();
            _segitems = a->_segitems;
            _begidx = idx;
            _flags = a->_flags;

            _array = a;
            _lrui = a->_usg_iter;
            //_segments.share( a._segments );

            if(_flags & fSEQ_INSERT)
                _usdpos = 0;
            else
                _usdpos = _segitems>>1;

            if(!_pseg)
                alloc_mem();

            return this;
        }

        segment* init( segarray<T,TAIL>* a )
        {
            _chunksize = a->item_size();
            _ntail = a->tail_element_count();
            _segitems = a->_segitems;
            _flags = a->_flags | fSEG_PRIMELOAD;

            _array = a;
            _lrui = a->_usg_iter;
            return this;
        }


        void* ptr_mem()
        {
            return _pseg;
        }

        void* alloc_mem()
        {
            if(_pseg)  return _pseg;

            _pseg = (char*) _array->seg_alloc();
            return _pseg;
        }

        void free_mem()
        {
            DASSERT( _pseg );
            _array->seg_free(_pseg);
            _pseg = 0;
        }

        bool is_allocated() const
        {
            return _pseg != 0;
        }

        void reset()
        {
            //_segments.unshare();

            uints i;
            T* p;
            if( _pseg  &&  !(_flags & fTRIVIAL_DESTRUCTOR) )
            {
                for( p=(T*)bgi_ptr(), i=0; i<_nitems; p=(T*)((char*)p+_chunksize), ++i )
                    p->~T();
            }

            if( !(_flags & segarray::fSEQ_INSERT) )
                _usdpos = _segitems>>1;
            else
                _usdpos = 0;
            _nitems = 0;
            
            set_prime_load();
        }


        segment* next_segment()
        {
            uints sid = _array->get_segment_id( index_behind() );
            if( sid == UMAX )
                return 0;

            return _array->get_segment(sid);
        }

        segment* prev_segment()
        {
            if( _begidx == 0 )  return 0;
            uints sid = _array->get_segment_id( _begidx-1 );

            return _array->get_segment(sid);
        }
    };

    friend struct segment;

private:
    dynarray< local<segment> >  _segments;  ///<segment array
    uints               _segitems;      ///<number of items per page
    //uint                _segsize;       ///<segment size in bytes
    uint                _rsegsize;      ///<segment size in bytes, two's exponent
    uint                _flags;
    uints               _lastidx;       ///<number of items total
    uints               _ntail;         ///<number of tail TAIL-type elements per record


    //swapping

    struct GET_INT_FROM_SEG
    {
        uints operator() (const void* p) const
        {
            segment* ps = *(local<segment>*)p;
            if( !ps->is_allocated() )
                return 0;
            if( ps->is_locked() )
                return 0;
            uints k = _ref - ps->get_lrui();
            return ++k;
        }

        uints _ref;

        GET_INT_FROM_SEG() : _ref(0)            { }
        GET_INT_FROM_SEG( uints k ) : _ref(k)    { }
    };

    typedef radixi< local<segment>, uints, uints, GET_INT_FROM_SEG >  t_radix;

    mutable uints       _usg_iter;      ///<usage iteration number
    t_radix             _radix;
    uints               _nsegmapped;    ///<number of segments mapped
    uints               _nsegmapmax;    ///<max.number of segments to map
    uints               _swapsegcount;  ///<number of swapped segments

    chunkalloc          _segmem;        ///< allocator used to allocate segment structures
    
    void*               _stream_context;///<context value to pass to the streaming functions
    fnc_stream          _fnc_stream_out;///<function for streaming out a segment
    fnc_stream          _fnc_stream_in; ///<function for streaming in a segment

    uints               _usg_iter_last;
    dynarray<uints>     _pages_to_swap;


    void* seg_alloc()
    {
        ++_nsegmapped;

        if( _nsegmapmax == UMAX )
            return ::malloc( 1<<_rsegsize );

        return _segmem.alloc();
    }

    void seg_free( void* seg )
    {
        if( _nsegmapmax == UMAX )
            ::free(seg);
        else
            _segmem.free(seg);
        --_nsegmapped;
    }

    ///Unmap old segments to make space
    uints make_unmap_list()
    {
        // 1/8th to free, but at least one
        uints n = _nsegmapmax >> 5;
        if(!n) n=1;

        DASSERT( _nsegmapped + n >= _nsegmapmax );

        GET_INT_FROM_SEG gi(_usg_iter);
        _radix.set_getint(gi);
        _radix.sortdsc( _segments.ptr(), _segments.size() );
        const uints* idx = _radix.ret_index();

        for( ; n>0; )
        {
            --n;
            segment* sg = _segments[ idx[n] ];
            if( sg->is_allocated()  &&  !sg->is_locked() )
            { ++n; break; }
        }

        if( n == 0 )
            throw ersUNAVAILABLE "cannot free enough pages due to locked segments";

        _pages_to_swap.need_new(n);
        for( uints i=n; i>0; )
        {
            --i;
            _pages_to_swap[i] = idx[i];
        }

        _usg_iter_last = _segments[ idx[n-1] ]->get_lrui();
        return n;
    }

    void unmap()
    {
        uints n = _pages_to_swap.size();
        if( n == 0 )
            n = make_unmap_list();

        for( uints i=n; i>0; )
        {
            --i;
            segment* sg =  _segments[ _pages_to_swap[i] ];
            if( sg->is_allocated()  &&  !sg->is_locked()  &&  ints( sg->get_lrui() - _usg_iter_last ) <= 0 )
            {
                unmap_segment( sg, _pages_to_swap[i] );
                _pages_to_swap.need(i);
                return;
            }
        }

        _pages_to_swap.reset();
        
        //try again
        unmap();
    }

    void unmap_segment( segment* sg, uints id )
    {
        opcd e = save_segment( sg, id, true );
        if(e)
            throw e;

        sg->free_mem();
    }

    ///Map segment
    segment* map( uints sid )
    {
        if( _nsegmapped >= _nsegmapmax )
            unmap();

        segment* sg = _segments[sid];

        opcd e = load_segment(sg,sid);
        if(e)
            throw e;

        sg->clear_prime_load();
        sg->lrui_upd(_usg_iter);

        return sg;
    }

    segment* create_segment( uints sid, uints n=1 )
    {
        if( sid == UMAX )
            sid = _segments.size();

        uints off = sid>0 ? _segments[sid-1]->index_behind() : 0;

        for( uints i=0; i<n; ++i )
        {
            if( _nsegmapped >= _nsegmapmax )
                unmap();

            local<segment>* bs = _segments.ins(sid);
            segment* sg = *bs = new segment;
            sg->alloc( off, this );

            if(_fnc_stream_in)
            {
                PERSIST p;
                p.context = _stream_context;
                p.array = this;
                p.segdata = sg->ptr_mem();
                p.segsize = sg->get_raw_segsize();
                p.data = sg->get_first();
                p.first = sg->bgi_index();
                p.count = sg->used_count();
                p.chunksize = sg->get_chunksize();
                p.segmentno = sid;
                p.fileblock = UMAX;
                p.bprimeload = 0;

                opcd e = _fnc_stream_in(p);
                if(e)
                    throw e;
            }
        }

        return get_segment(sid);
    }

    opcd save_segment( segment* sg, uints id, bool todestroy )
    {
        PERSIST p;
        p.context = _stream_context;
        p.array = this;
        p.segdata = sg->ptr_mem();
        p.segsize = sg->get_raw_segsize();
        p.data = sg->get_first();
        p.first = sg->bgi_index();
        p.count = sg->used_count();
        p.chunksize = sg->get_chunksize();
        p.segmentno = id;
        p.fileblock = sg->getset_swapid(_swapsegcount);
        p.bdestroy = todestroy;
        p.bprimeload = 0;

        return _fnc_stream_out(p);
    }

    opcd load_segment( segment* sg, uints id )
    {
        PERSIST p;
        p.context = _stream_context;
        p.array = this;
        p.segdata = sg->alloc_mem();
        p.segsize = sg->get_raw_segsize();
        p.data = sg->get_first();
        p.first = sg->bgi_index();
        p.count = sg->used_count();
        p.chunksize = sg->get_chunksize();
        p.segmentno = id;
        p.fileblock = sg->getset_swapid(_swapsegcount);
        p.bprimeload = sg->is_prime_load();

        return _fnc_stream_in(p);
    }




    uints get_segment_id( uints a ) const
    {
        if( _segments.size() == 0  ||  a >= _lastidx )
            return UMAX;

        uints i=0, j=_segments.size();
        for( ;j-i>1; )
        {
            uints m = (j+i)/2;
            if( _segments[m]->bgi_index() > a )
                j=m;
            else
                i=m;
        }

        return i;
    }
    
    segment* get_segment( uints sid )
    {
        DASSERT( sid < _segments.size() );

        segment* sg = _segments[sid];
        if( !sg->is_allocated() )
        {
            //mapped out
            map( sid );
        }
        else
            sg->lrui_upd(_usg_iter);
        return sg;
    }

    segment* get_segment_const( uints sid ) const
    {
        DASSERT( sid < _segments.size() );

        segment* sg = _segments[sid];
        if( !sg->is_allocated() )
            throw ersIMPROPER_STATE;

        sg->lrui_upd(_usg_iter);
        return sg;
    }

    segment* get_last_segment()
    {
        return _segments.size() ? get_segment(_segments.size()-1) : 0;
    }

    segment* get_last_segment_const() const
    {
        return _segments.size() ? get_segment_const(_segments.size()-1) : 0;
    }

    //look for the segment above the one given
    uints get_segment_behind( uints a, uints sid ) const
    {
        uints offseg = _segments.size();
        for(; sid < offseg; ++sid) {
            if( _segments[sid]->is_at(a) )  return sid;
        }
        return 0;
    }

    //look for the segment below the one given
    uints get_segment_before( uints a, uints sid ) const
    {
        for(; (ints)sid >= 0; --sid) {
            if( _segments[sid]->is_at(a) )  return sid;
        }
        return 0;
    }

public:

    static uints get_sizeof_segment_desc()                { return sizeof(segment); }

    uints get_last_segment_id() const
    {
        if( 0 == _segments.size() )  return UMAX;

        return _segments.size()-1;
    }
    


    ////////////////////////////////////////////////////////////////////////////////
    ///Segarray iterator
    struct ptr : std::iterator<std::random_access_iterator_tag, T>
    {
    private:
        T*  _p;                     ///<ptr to the managed item
        union {
            segment*    _seg;       ///<IF(_p!=0) ptr to the segment where the item occurs
            segarray*   _array;     ///<IF(_p==0) ptr to the segarray
        };

    public:

        COIDNEWDELETE

        typedef T           value_type;
        typedef ptrdiff_t   difference_type;

        operator value_type () const            { return *_p; }
        //operator const value_type () const      { return *_p; }

        difference_type operator - (const ptr& p) const     { return index() - p.index(); }

        bool operator < (const ptr& p) const    { return index() < p.index(); }
        bool operator <= (const ptr& p) const   { return index() <= p.index(); }

        int operator == (const T* ptr) const    { return ptr == _p; }

        int operator == (const ptr& p) const    { return p._p == _p; }
        int operator != (const ptr& p) const    { return p._p != _p; }

        const T& operator *(void) const         { return *_p; }
        T& operator *(void)                     { return *_p; }

#ifdef SYSTYPE_MSVC
#pragma warning( disable : 4284 )
#endif
        T* operator ->(void) const              { return _p; }

        ptr& operator ++()
        {
            if (!_p)
                throw ersUNAVAILABLE "invalid reference";
            if( !_seg->inc_ptr(_p) )  return next_segment();
            return *this;
        }
        ptr& operator --()
        {
            if(!_p)
            {
                //special, if ptr invalid, -- will move to the last item
                //this is to be able to --iterate from end-iterator
                
                _array->last(*this);
                DASSERT(_p);
            }
            else if( !_seg->dec_ptr(_p) )  return prev_segment();
            return *this;
        }

        ptr operator ++(int)
        {
            ptr x = *this;
            ++(*this);
            return x;
        }
        ptr operator --(int)
        {
            ptr x = *this;
            --(*this);
            return x;
        }

        ptr& operator += (ints i)
        {
            if( i == 0 )
                return *this;

            if(!_p)
            {
                return _array->_get_at_safe( _array->_lastidx+i, *this );
            }

            ints oi = i * _seg->_chunksize;
            char* p = (char*)_p + oi;

            if( _seg->is_valid_ptr(p) )
                _p = (T*)p;
            else
            {
                _seg->refcnt_dec();
                _seg->_array->_get_at_safe( _seg->get_index(_p)+i, *this );
            }
            return *this;
        }

        ptr& operator -= (ints i)        { return operator += (-i); }

        const T& operator [] (ints i) const
        {
            if( i == 0 )
                return **this;

            if(!_p)
            {
                return *_array->_get_at( _array->_lastidx+i );
            }

            ints oi = i * _seg->_chunksize;
            char* p = (char*)_p + oi;

            if( _seg->is_valid_ptr(p) )
                return *(T*)p;
            else
                return *_seg->_array->_get_at( _seg->get_index(_p)+i );
        }

        T& operator [] (ints i)
        {
            if( i == 0 )
                return **this;

            if(!_p)
            {
                return *_array->_get_at( _array->_lastidx+i );
            }

            ints oi = i * _seg->get_chunksize();
            char* p = (char*)_p + oi;


            if( _seg->is_valid_ptr(p) )
                return *(T*)p;
            else
                return *_seg->_array->_get_at( _seg->get_index(_p)+i );
        }

        const T* operator () () const           { return _p; }
        T* operator () ()                       { return _p; }

        const T* operator () (ints i) const      { return & (operator[] (i)); }
        T* operator () (ints i)                  { return & (operator[] (i)); }

        ptr operator + (ints i) const
        {
            ptr t = *this;
            t += i;
            return t;
        }

        ptr operator - (ints i) const
        {
            ptr t = *this;
            t -= i;
            return t;
        }

        T* ptr_data() const                     { return _p; }
        TAIL* ptr_tail() const                  { return (TAIL*)((char*)_p + _seg->sizeof_T()); }

        T* copy_to( T* dest ) const
        {
            *dest = *_p;
            uints len = _seg->tail_element_count();
            const TAIL* t = ptr_tail ();
            TAIL* d = _seg->ptr_tail(dest);
            for (uints i=0; i<len; ++i)
                d[i] = t[i];
            return dest;
        }

        T* copy_from( const T* src )
        {
            *_p = *src;
            uints len = _seg->tail_element_count();
            TAIL* t = ptr_tail ();
            const TAIL* s = _seg->ptr_tail(src);
            for (uints i=0; i<len; ++i)
                t[i] = s[i];
            return _p;
        }

        uints copy_raw_to( T* dest, uints n )
        {
            uints initn = n;
            uints chs = _seg->get_chunksize();
            uints usc = _seg->used_count_after( index() );
            ptr x = *this;
            while( n > 0 )
            {
                if( usc == 0 )  break;
                if( n < usc )
                {
                    xmemcpy( dest, x._p, n*chs );
                    return initn;
                }
                xmemcpy( dest, x._p, usc*chs );
                dest += usc;
                n -= usc;
                x += usc;
                usc = x._seg->used_count_after( x.index() );
            }
            return initn - n;
        }

        uints copy_raw_from( const T* src, uints n )
        {
            uints initn = n;
            uints chs = _seg->get_chunksize();
            uints usc = _seg->used_count_after( index() );
            ptr x = *this;
            while( n > 0 )
            {
                if( usc == 0 )  break;
                if( n < usc )
                {
                    xmemcpy( x._p, src, n*chs );
                    return initn;
                }
                xmemcpy( x._p, src, usc*chs );
                src += usc;
                n -= usc;
                x += usc;
                usc = x._seg->used_count_after( x.index() );
            }
            return initn - n;
        }


        ///Insert n items behind the ptr
        ptr& ins( uints n=1 ) {
            if(!_p)
                return _array->push( *this, n );
            return _seg->_array->ins( index(), *this, n );
        }

        ///Delete n items following the ptr
        ptr& del( uints n=1 ) {
            if(!_p)
                throw ersUNAVAILABLE "the iterator points past the last element";
            return _seg->_array->del( index(), *this, n );
        }

        ptr& del_before_after( uints bef, uints aft )
        {
            return _seg->_array->del( index()-bef, *this, bef+aft );
        }
/*
        friend binstream& operator << TEMPLFRIEND (binstream& out, const ptr& p)
        {
            out << *p;
        //    uint len = _seg->_ntail;
        //    for (uint i=0; i<len; ++i)
        //        out << ((TAIL*)(_p+1))[i];
            return out;
        }

        friend binstream& operator >> TEMPLFRIEND (binstream& in, ptr& p)
        {
            in >> *p;
        //    uint len = _seg->_ntail;
        //    for (uint i=0; i<len; ++i)
        //        in >> ((TAIL*)(_p+1))[i];
            return in;
        }
*/
        ///Return index of the item this ptr points to
        uints index() const
        {
            return  _p  ?  _seg->get_index(_p)  :  _array->_lastidx;
        }

        ///Check if the ptr is valid
        bool is_valid() const           { return _p != 0; }

        ptr( const segment* seg, T* p )
        {
            DASSERT( p && seg );
            _seg = (segment*)seg;
            _p = p;
            _seg->refcnt_inc();
        }

        ptr( const segarray* array )
        {
            _array = (segarray*)array;
            _p = 0;
        }

        void set( const segment* seg, T* p )
        {
            if(_p)
                _seg->refcnt_dec();

            DASSERT( p && seg );
            _seg = (segment*)seg;
            _p = p;
            _seg->refcnt_inc();
        }

        void set( const segarray* array )
        {
            if(_p)
                _seg->refcnt_dec();

            _array = (segarray*)array;
            _p = 0;
        }


        ptr() : _p(0), _seg(0) {}

        ptr( const ptr& p )
        {
            _p = p._p;
            _seg = p._seg;
            if(_p)
                _seg->refcnt_inc();
        }

        ptr& operator = (const ptr& p)
        {
            if(_p)
                _seg->refcnt_dec();

            _p = p._p;
            _seg = p._seg;
            if(_p)
                _seg->refcnt_inc();
            return *this;
        }

        ~ptr()
        {
            if(_p)
                _seg->refcnt_dec();
        }

        void unset()
        {
            if(_p)
            {
                _seg->refcnt_dec();
                _array = _seg->_array;
                _p = 0;
            }
        }

    protected:

        ptr& next_segment()
        {
            DASSERT(_p);

            segment* sg = _seg->next_segment();
            _seg->refcnt_dec();
            if(!sg)
            {
                _p = 0;
                _array = _seg->_array;
            }
            else
            {
                _seg = sg;
                _p = sg->get_first();
                _seg->refcnt_inc();
            }
            return *this;
        }

        ptr& prev_segment()
        {
            if( _p == 0 )
            {
                _array->last(*this);
                DASSERT(_p);
                return *this;
            }

            segment* sg = _seg->prev_segment();
            DASSERT(sg);

            _seg->refcnt_dec();
            _seg = sg;
            _p = sg->get_last();
            _seg->refcnt_inc();

            return *this;
        }

    };


    binstream& stream_out( binstream& bin ) const
    {
        bin << _ntail << _flags << _lastidx << _rsegsize << _segitems;

        bin << _segments.size();
        for( uints i=0; i<_segments.size(); ++i )
            _segments[i]->stream_out( bin );

        return bin;
    }

    binstream& stream_in( binstream& bin )
    {
        bin >> _ntail >> _flags >> _lastidx >> _rsegsize >> _segitems;

        uints n;
        bin >> n;

        _segments.reserve(n,true);
        for( uints k=0; k<n; ++k )
        {
            typename segarray<T, TAIL>::segment* sg = create_segment(k);//new typename segarray<T, TAIL>::segment(sega.item_size());
            sg->stream_in( bin );
        }
        return bin;
    }



    typedef ptr                     iterator;
    typedef const ptr               const_iterator;

    ptr begin()                     { ptr x;  get_ptr( 0, x );  return x; }
    const ptr begin() const         { ptr x;  get_ptr( 0, x );  return x; }
    ptr end()                       { ptr x;  get_ptr( _lastidx, x );  return x; }
    const ptr end() const           { ptr x;  get_ptr( _lastidx, x );  return x; }

    ///Get pointer to specified item
    ptr& get_ptr_const( uints a, ptr& p ) const
    {
        if(a >= _lastidx)
        {
            p.set(this);
            return p;
        }

        _get_at_safe_const( a, p );
        return p;
    }

    ptr& get_ptr( uints a, ptr& p )
    {
        if(a >= _lastidx)
        {
            p.set(this);
            return p;
        }

        _get_at_safe( a, p );
        return p;
    }

    ptr& get_ptr_create( uints a, ptr& p )
    {
        if( a >= _lastidx )
        {
            push_set_to_last( p, 1 + a - _lastidx );
            return p;
        }
        segment* segid = get_segment( get_segment_id(a) );

        p.set( segid, segid->get_at(a) );
        return p;
    }

    ///Insert n items on a-th item
    void ins( uints a, uints n=1 )
    {
        if( n==0 )  return;
        _ins(a, n);
    }

    ///Delete n items starting at the a-th item
    void del( uints a, uints n=1 )
    {
        _del(a, n);
    }

    ///Insert n items on a-th item
    void ins( ptr& a, uints n=1 )
    {
        if( n==0 )  return;
        _ins( a.index(), n );
    }

    ///Delete n items starting at the a-th item
    void del( ptr& a, uints n=1 )
    {
        _del( a.index(), n );
    }


    ptr& ins( uints a, ptr& p, uints n=1 )
    {
        segment* segid = _ins(a,n);
        if( segid->is_at(a) )
        {
            segid->lrui_upd(_usg_iter);
            p.set( segid, segid->get_at(a) );
        }
        else
            get_ptr(a, p);
        return p;
    }

    T* ins_item( uints a )
    {
        segment* segid = _ins(a, 1);
        return segid->get_at(a);
    }

    ptr& del( uints a, ptr &p, uints n=1 )
    {
        _del(a, n);
        return get_ptr(a, p);
    }

    ptr& push( ptr &p, uints n=1 )
    {
        if(!n)
            return p;
        segment* segid = _ins( _lastidx, n );
        p.set( segid, segid->get_at(_lastidx-n) );
        return p;
    }

    ptr& push_set_to_last( ptr &p, uints n=1 )
    {
        if(!n)
            return p;
        segment* segid = _ins(_lastidx, n);
        p.set( segid, segid->get_at(_lastidx-1) );
        return p;
    }

    ptr& push( ptr &p, binstream& in )
    {
        segment* segid = _ins(_lastidx, 1);
        p.set( segid, segid->get_at(_lastidx-1) );
        in >> p;
        return p;
    }

    T* push_item()
    {
        segment* segid = _ins(_lastidx, 1);
        return segid->get_last();
    }

    bool pop( uints n=1 )
    {
        if(_lastidx == 0)  return false;
        _del( _lastidx, n );
        return true;
    }

    ptr& first( ptr &p )
    {
        return get_ptr( 0, p );
    }

    void last( ptr &p )
    {
        if(_lastidx)
        {
            segment* seg = get_last_segment();
            p.set( seg, seg->get_last() );
        }
        else
        {
            p.set(this);
        }
    }

    void last_const( ptr &p ) const
    {
        if(_lastidx)
        {
            segment* seg = get_last_segment_const();
            p.set( seg, seg->get_last() );
        }
        else
        {
            p.set(this);
        }
    }

    T* last()
    {
        return _lastidx ? get_last_segment()->get_last() : 0;
    }

    T* last_const() const
    {
        return _lastidx ? get_last_segment_const()->get_last() : 0;
    }

    bool exists( uints idx ) const            { return idx < size(); }


    T* _get_at_const( uints i ) const
    {
        uints segn = get_segment_id(i);
        DASSERT( segn < _segments.size() );
        return get_segment_const(segn)->get_at(i);
    }

    T* _get_at( uints i )
    {
        uints segn = get_segment_id(i);
        DASSERT( segn < _segments.size() );
        return get_segment(segn)->get_at(i);
    }

    ///Get item ptr, do not throw when index equals array size
    void _get_at_safe_const( uints i, ptr& p ) const
    {
        if( i == _lastidx  ||  i == UMAX )
        {
            p.set(this);
            return;
        }
        
        uints segn = get_segment_id(i);
        DASSERT( segn < _segments.size() );
        segment* sg = get_segment_const(segn);
        p.set( sg, sg->get_at(i) );
    }

    ///Get item ptr, do not throw when index equals array size
    ptr& _get_at_safe( uints i, ptr& p )
    {
        if( i == _lastidx  ||  i == UMAX )
        {
            p.set(this);
            return p;
        }
        
        uints segn = get_segment_id(i);
        DASSERT( segn < _segments.size() );
        segment* sg = get_segment(segn);
        p.set( sg, sg->get_at(i) );

        return p;
    }

    const T* get_at_const( uints i ) const      { return _get_at_const(i); }
    T* get_at_const( uints i)                   { return _get_at_const(i); }

    T* get_at(uints i)                          { return _get_at(i); }

    T* get_at_create( uints a )
    {
        if(a >= _lastidx)
        {
            segment* segid = _ins(a, 1);
            if( segid->is_at(a) )
            {
                segid->lrui_upd(_usg_iter);
                return segid->get_at(a);
            }
            else
                return get_at(a);
        }
        return get_at(a);
    }


    opcd get_segment_info( uints sg, uints* first, uints* nitems, uints* offs ) const
    {
        if( sg >= _segments.size() )
            return ersOUT_OF_RANGE;

        const segment& csg = *_segments[sg];
        return csg.get_info( first, nitems, offs );
    }

    uints get_segment_size() const
    {
        return 1<<_rsegsize;
    }


    bool is_segment_mapped( uints sg ) const
    {
        DASSERT( sg < _segments.size() );
        return _segments[sg]->is_allocated();
    }


    const T& operator [] (ints i) const      { return *_get_at_const(i); }
    T& operator [] (ints i)                  { return *_get_at_const(i); }

    T* operator () (ints i)                  { return _get_at(i); }

    ///set up ptr object to point at specified item index
    ptr& operator () (ints i, ptr& x)        { _get_at_safe( i, x );  return x; }

    segarray& operator = (const segarray& in)
    {
        _ntail = in._ntail;
        _flags = in._flags;
        _lastidx = in._lastidx;
        _rsegsize = in._rsegsize;
        _segitems = in._segitems;
        _segments.need_newc( in._segments.size() );
        for( uints i=0; i<in._segments.size(); ++i )
            create_segment(i);

        _swapsegcount = 0;
        _stream_context = 0;

        //_segsize = in._segsize;
        return *this;
    }

    segarray& takeover( segarray& in )
    {
        _ntail = in._ntail;
        _flags = in._flags;
        _lastidx = in._lastidx;
        _rsegsize = in._rsegsize;
        _segitems = in._segitems;
        
        _segments.takeover( in._segments );
        for( uints i=0; i<_segments.size(); ++i )
            _segments[i]->set_array(this);

        //_segsize = in._segsize;

        _stream_context = in._stream_context;
        in._stream_context = 0;
        _fnc_stream_in = in._fnc_stream_in;
        _fnc_stream_out = in._fnc_stream_out;

        in._lastidx = 0;
        return *this;
    }

    size_t sizeof_T () const            { return (_flags & fSIZE_ZERO) ? 0 : sizeof(T); }


    uints item_size() const             { return sizeof_T() + _ntail*sizeof(TAIL); }
    uints size() const                  { return _lastidx; }        ///<size of whole array in items
    uints items_in_seg() const          { return _segitems; }       ///<size of segment in items
    uints tail_element_count() const    { return _ntail; }

    bool is_trivial_destructor() const  { return (_flags & fTRIVIAL_DESTRUCTOR) != 0; }
    bool is_trivial_constructor() const { return (_flags & fTRIVIAL_CONSTRUCTOR) != 0; }

    ///Reset content
    void reset()
    {
        _segments.reset();
        _lastidx = 0;
        _nsegmapped = 0;
        _swapsegcount = 0;
        _segmem.reset();
        _pages_to_swap.reset();
        _usg_iter_last = 0;
        _usg_iter = 0;
    }

    ///Discard content
    void discard()
    {
        _segments.discard();
        reset();
    }


    uints get_mapped_segment_id( uints sid ) const
    {
        if( _nsegmapmax == UMAX )  return UMAX;

        segment* sg = get_segment_const(sid);
        if(!sg) return UMAX;

        return _segmem.get_item_id( sg->ptr_mem() );
    }

    void set_fnc_stream( void* context, fnc_stream streamout, fnc_stream streamin )
    {
        _stream_context = context;
        _fnc_stream_out = streamout;
        _fnc_stream_in = streamin;
    }

    ///Write all mapped segments to disk
    opcd flush_swap()
    {
        opcd e;
        for( uints i=0; i<_segments.size(); ++i )
        {
            segment* sg = _segments[i];
            if( sg->is_allocated() )
            {
                e = save_segment( sg, i, false );
                if(e)  return e;
            }
        }
        return 0;
    }


    opcd save_descriptor( binstream& bin ) const
    {
        try {
            bin << _rsegsize << _segitems << _flags << _lastidx << _ntail << _nsegmapmax;
            uints n = _segments.size();
            bin << n;

            opcd e;
            for( uints i=0; i<n; ++i )
            {
                e = _segments[i]->save_descriptor(bin);
                if(e)  return e;
            }   
        }
        catch(opcd e)
        {
            return e;
        }

        return 0;
    }

    opcd load_descriptor( binstream& bin )
    {
        discard();

        try {
            bin >> _rsegsize >> _segitems >> _flags >> _lastidx >> _ntail >> _nsegmapmax;
            uints n;
            bin >> n;
            _segments.need_new(n);

            opcd e;
            for( uints i=0; i<n; ++i )
            {
                _segments[i] = new segment;
                _segments[i]->init(this);

                e = _segments[i]->load_descriptor(bin);
                if(e)  return e;
            }
        }
        catch(opcd e)
        {
            return e;
        }

        _swapsegcount = _segments.size();

        return 0;
    }



    opcd set_max_memory( uints size )
    {
        if( size == 0 )
        {
            _nsegmapmax = UMAX;
            return 0;
        }

        if( size < (2UL<<_rsegsize) )
            size = 2;
        else
            size = align_to_chunks_pow2( size, _rsegsize );

        _nsegmapmax = size;

        if( _nsegmapped >= _nsegmapmax )
            unmap();

        return _segmem.init( 1<<_rsegsize, size<<_rsegsize );
    }

    uints get_max_memory() const
    {
        return _nsegmapmax == UMAX  ?  0 : (_nsegmapmax<<_rsegsize);
    }

    ///Set actual item size
    uints set_tail_element_count( uints n, bool sizeofTzero = false )
    {
        if(_lastidx)  return _ntail;        //if anything already pushed in, return the old size
        if(sizeofTzero)
            _flags |= fSIZE_ZERO;
        RASSERTX( sizeof_T()+n*sizeof(TAIL) < (1UL<<_rsegsize), "required size must be lower than size of segment" );
        _segments.reset();
        _ntail = n;
        _segitems = (1<<_rsegsize)/item_size();
        //_segsize = _segitems*item_size();
        return true;
    }

    uints set_segsize( uints segsize )
    {
        if(_lastidx)
            return 1<<_rsegsize;        //if anything already pushed in, return the old size

        uints rs = segsize;
        uchar i;
        for( i=0; rs; ++i, rs>>=1 );
        if( i-- == 0 )
            _rsegsize = DEF_RSEGSIZE;
        else
        {
            if( segsize & ((1<<i)-1) )
                ++i;
            _rsegsize = i;
        }
        _segitems = (uints) ((1<<_rsegsize)/item_size());
        //_segsize = _segitems*item_size();
        return 1<<_rsegsize;
    }

    uints get_raw_segsize() const                { return 1<<_rsegsize; }

    void set_trivial_destructor( bool trivial=true )
    {
        if (trivial)
            _flags |= fTRIVIAL_DESTRUCTOR;
        else
            _flags &= ~fTRIVIAL_DESTRUCTOR;
    }

    void set_trivial_constructor( bool trivial=true )
    {
        if (trivial)
            _flags |= fTRIVIAL_CONSTRUCTOR;
        else
            _flags &= ~fTRIVIAL_CONSTRUCTOR;
    }

    TAIL* ptr_tail( const T* p ) const      { return (TAIL*)((char*)p + sizeof_T()); }

    segarray( uint flags = fSEQ_INSERT, uints segsize = 0, uints ntail = 0 )
    {
        uchar i;
        uints rs = segsize;
        for(i=0; rs; ++i, rs>>=1);
        if(i--==0)  _rsegsize = DEF_RSEGSIZE;
        else
        {
            if(segsize & ((1<<i)-1))  ++i;
            _rsegsize = i;
        }
        _flags = flags;
        _lastidx = 0;
        _ntail = ntail;
        _segitems = uints( (1<<_rsegsize)/item_size() );
        //_segsize = _segitems*item_size();

        _fnc_stream_in = _fnc_stream_out = 0;
        _stream_context = 0;
        _nsegmapmax = UMAX;
        _nsegmapped = 0;
        _usg_iter = 0;
        _swapsegcount = 0;
        _usg_iter_last = 0;
    }

    friend struct ptr;

protected:
    segment* _ins( uints a, uints n )
    {
        uints sid = get_segment_id(a);
        if( sid >= _segments.size() )
        {
            //required item is behind currently allocated array
            uints nc = a - _lastidx;    //to construct
            uints nt = n + nc;          //to tal    :)
            sid = get_last_segment_id();
            if( sid != UMAX )
            {
                segment* segid = get_segment(sid);
                //if (segid->_segarray != this)
                //    throw ersFAILED "object damaged";
                uints tin = nt>segid->free_count_after() ? segid->free_count_after() : nt;
                uints tco = nc>segid->free_count_after() ? segid->free_count_after() : nc;
                construct(
                    segid->ins( segid->used_count(), true, tin ),
                    tco
                    );
                nc -= tco;
                nt -= tin;
                _lastidx += tin;
            }
            else
                sid = 0;
            while(nt)
            {
                segment* segid = create_segment(UMAX);
                //segment* segid = *_segments.add(1) = new segment;
                //segid->alloc( _lastidx, *this, _usg_iter );

                uints tin = nt>_segitems ? _segitems : nt;
                uints tco = nc>_segitems ? _segitems : nc;
                //construct items created by the way but not required by the arguments
                construct(
                    segid->ins( 0, true, tin ),
                    tco
                    );
                nc -= tco;
                nt -= tin;
                _lastidx += tin;
            }
            return get_segment( get_segment_behind(a,sid) );
        }
        else if(_segments[sid]->free_count() < n)
        {
            segment* segid = get_segment(sid);
            //if (segid->_segarray != this)
            //    throw ersFAILED "object damaged";
            //current page does not have enough of free space to hold new data, check next and previous page
            if(sid+1 < _segments.size()  &&  (segid->free_count() + _segments[sid+1]->free_count() >= n))
            {
                //distribute to the two segments
                uints n1 = segid->free_count();
                uints n2 = _segments[sid+1]->free_count();
                uints nm = segid->used_count_after(a);
                distrib(n1, n2, n);
                segment& s1 = *get_segment(sid+1);

                if (nm <= n2)
                {
                    segid->move( s1, true, nm );
                    s1.insm( 0, n2-nm );
                    n1 += nm;
                    nm = 0;
                }
                else
                {
                    segid->move( s1, true, n2 );
                    n1 += n2;
                    nm -= n2;
                }

                segid->insm( segid->used_count()-nm, n1 );
                return get_segment( adjust(sid,a) );
            }
            else if( segid->bgi_index() > 0  &&  (segid->free_count() + _segments[sid-1]->free_count() >= n) )
            {
                //distribute to the two segments
                uints n1 = segid->free_count();
                uints n2 = _segments[sid-1]->free_count();
                uints nm = a - segid->bgi_index();
                distrib(n1, n2, n);
                segment& s1 = *get_segment(sid-1);

                if (nm <= n2)
                {
                    segid->move( s1, false, nm );
                    s1.insm( s1.used_count(), n2-nm );
                    n1 += nm;
                    nm = 0;
                }
                else
                {
                    segid->move( s1, false, n2 );
                    n1 += n2;
                    nm -= n2;
                }

                segid->insm( nm, n1);
                return get_segment( adjust(sid-1,a) );
            }
            else
            {
                //create new segments
                uints nseg = (n+_segitems-1)/_segitems;

                if (a - segid->bgi_index() < (_segitems>>1))
                {
                    // insert to the front
                    segment* segidnew = create_segment( sid, nseg );
                    //segidnew->alloc( 0, *this, _usg_iter );

                    uints nm = a - segid->bgi_index();
                    segid->move( *segidnew, false, nm );

                    uints fc = segidnew->free_count();
                    if (n > fc)
                    {
                        segidnew->insm( segidnew->used_count(), fc );
                        n -= fc;

                        for (uints i=1; i<nseg; ++i)
                        {
                            //_segments[sid+i] = new segment;
                            //_segments[sid+i]->alloc( 0, *this, _usg_iter );

                            segment* s = get_segment(sid+i);
                            s->insm( 0, n>_segitems ? _segitems : n);
                            if(n>_segitems)  n -= _segitems;  else  n = 0;
                        }
                    }
                    else
                    {
                        segidnew->insm( segidnew->used_count(), n );
                    }
                }
                else
                {
                    // insert to the back
                    create_segment( sid+1, nseg );
                    segment* segidnew = get_segment(sid+nseg);
                    //_segments.ins(sid+1, nseg);
                    //segment* segidnew = (_segments[sid+nseg] = new segment);
                    //segidnew->alloc( 0, *this, _usg_iter );

                    uints nm = segid->index_behind() - a;
                    segid->move( *segidnew, true, nm );

                    uints fc = segid->free_count();
                    if (n > fc)
                    {
                        segid->insm( segid->used_count(), fc );
                        n -= fc;

                        for( uints i=1; i<=nseg; ++i )
                        {
                            //if( i != nseg )
                            //{
                            //    _segments[sid+i] = new segment;
                            //    _segments[sid+i]->alloc( 0, *this, _usg_iter );
                            //}

                            if(n)
                            {
                                segment* si = get_segment(sid+i);
                                si->insm( 0, n>_segitems ? _segitems : n);
                                if( n>_segitems )  n -= _segitems;  else  n = 0;
                            }
                        }
                    }
                    else
                    {
                        segid->insm( segid->used_count(), n );
                    }
                }

                return get_segment( adjust(sid,a) );
            }
        }
        else
        {
            segment* segid = get_segment(sid);
            //if (segid->_segarray != this)
            //    throw ersFAILED "object damaged";
            segid->insm( a - segid->bgi_index(), n );
            adjust(sid);
            return segid;
        }
    }

    segment* _del(uints a, uints n)
    {
        uints sid = get_segment_id(a);
        RASSERTX( sid < _segments.size(), "out of range" );

        segment* segid;
        segid = get_segment(sid);

        //uints m = n*_chunksize;
        //T* p = segid->get_at(a);
		uints rp = a - segid->bgi_index();

        if(n > segid->used_count_after(a))
        {
            n -= segid->used_count_after(a);
            segid->del( rp, segid->used_count_after(a) );
            uints i;

            for (i=1;;++i)
            {
                segment& si = *get_segment(sid+i);
                if(n < si.used_count())
                {
                    si.del( 0, n );
                    break;
                }
                else
                {
                    n -= si.used_count();
                    si.reset();
                    if(!n) { ++i; break; }
                }
            }
            --i;
            _segments.del( sid+1, i );
            adjust(sid);
            return get_segment(sid);
        }
        else
        {
            segid->del( rp, n );
            if( segid->used_count() == 0 )
                _segments.del(sid, 1);
            if(sid == 0)
            {
                if(_segments.size() > 0)
                    _segments[0]->set_index(0);
                else
                {
                    _lastidx = 0;
                    return 0;
                }
            }
            else  --sid;
        }
        adjust(sid);
        return get_segment(sid);
    }

    static void distrib( uints &n1, uints &n2, uints n )
    {
        // n1+n2 >= n  &&  n\chunk  &&  n1\chunk  &&  n2\chunk
        // n2-n + n1-n
        if(n2>n)    { n2 = n; n1 = 0; }
        else        { n1 = n - n2; }        //this keeps as much on the n1 as possible
    }

    void adjust( uints sid )
    {
        for( ++sid; sid<_segments.size(); ++sid )
            _segments[sid]->adjust( *_segments[sid-1] );

        _lastidx = _segments[sid-1]->index_behind();
    }

    uints adjust( uints sid, uints id )
    {
        uints ssi = UMAX;
        for( ++sid; sid<_segments.size(); ++sid )
        {
            _segments[sid]->adjust( *_segments[sid-1] );
            if( id < _segments[sid]->bgi_index() )
            {
                ssi = sid-1;
                id = UMAX;
            }
        }

        _lastidx = _segments[sid-1]->index_behind();
        if( id < _lastidx )
            ssi = sid-1;
        return ssi;
    }

    void construct( T* p, uints n ) const {
        //for(; n>0; --n, p=(T*)((char*)p+_chunksize))
        //  new(p) T;
        memset (p, 0, n*item_size());
    }


public:
	friend binstream& operator << TEMPLFRIEND(binstream &out, const segarray<T,TAIL> &sega);
	friend binstream& operator >> TEMPLFRIEND(binstream &in, segarray<T,TAIL> &sega);
};


////////////////////////////////////////////////////////////////////////////////
template <class T, class TAIL>
inline typename segarray<T,TAIL>::ptr::difference_type operator - (const typename segarray<T,TAIL>::ptr& a, const typename segarray<T,TAIL>::ptr& b)
{
    return a.index() - b.index();
}


template <class T, class TAIL>
inline binstream& operator << (binstream &out, const segarray<T,TAIL> &sega)
{
    return sega.stream_out(out);
}

template <class T, class TAIL>
inline binstream& operator >> (binstream &in, segarray<T, TAIL> &sega)
{
    return sega.stream_in(in);
}

/*
///These streamizing operators need from the managed class the methods out and in, which take a stream to persist
/// to or from, and an actual byte size of the structure as it is managed within the segarray
template <class T, class TAIL>
inline binstream& operator << (binstream &out, const typename segarray<T,TAIL>::segment &sega)
{
    out << sega._usdpos << sega._nitems << sega._begidx << sega.get_raw_segsize() << sega.tail_element_count();
    //out.write(sega.bgi_ptr(), sega._nitems*sega._chunksize);
    T* p = (T*) sega.bgi_ptr();
    for(uint i=0; i<sega._nitems; ++i, p=(T*)((char*)p+sega._chunksize))
    {
        out << *p;
        TAIL* t = sega.ptr_tail(p);
        for( uint j=0; j<sega.tail_element_count(); ++j )
            out << t[j];
    }
    return out;
}

template <class T, class TAIL>
inline binstream& operator >> (binstream &in, typename segarray<T,TAIL>::segment &sega)
{
    uint segsize, ntail;

    in >> sega._usdpos >> sega._nitems >> sega._begidx >> segsize >> ntail;

    T* p = sega.get_first();
    for( uint i=0; i<sega._nitems; ++i, p=(T*)((char*)p+sega._chunksize) )
    {
        new(p) T;
        in >> *p;
        TAIL* t = sega.ptr_tail (p);
        for (uint j=0; j<ntail; ++j)
        {
            new(t+j) TAIL;
            in >> t[j];
        }
    }
    return in;
}
*/





COID_NAMESPACE_END

#endif

/**
    - overloadable method for persisting segment through binstream
    - limit on amount of memory mapped
    - LRU cache

  After hitting the roof, when some segments need to be swapped out, we need to
  determine which segments were not been used recently.
  Each segment keeps the most recent iteration number when it was touched. We need
  to sort the segments by the numbers and select the oldest segments.

  - radix sort
  - minimum number of segments to free, a percentage of total count

  - streaming function can return an error, if it doesn't want to persist the content
    out or in (a log for example)
**/
