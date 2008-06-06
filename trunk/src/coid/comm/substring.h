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
 * Brano Kemen.
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#ifndef __COID_COMM_SUBSTRING__HEADER_FILE__
#define __COID_COMM_SUBSTRING__HEADER_FILE__

#include "namespace.h"

#include "commassert.h"

COID_NAMESPACE_BEGIN


struct token;

////////////////////////////////////////////////////////////////////////////////
///Preprocessed substring for fast substring searches
class substring
{
    //mutable uchar _shf[256];
    uchar* _shf;
    uchar _from, _to;
    uchar _onechar, _onebuf;

    const uchar* _subs;
    uints _len;
public:

    substring()                                 { _shf=0; set(0); }
    substring( const char* subs, uints len )    { _shf=0; set(subs,len); }
    substring( const token& tok )               { _shf=0; set(tok); }
    explicit substring( char k )                { _shf=0; set(k); }

    ~substring()
    {
        if( _shf && _shf!=&_onebuf )
            delete[] _shf;
    }

    substring& operator = ( const token& tok )  { set(tok);  return *this; }

    //@{ Predefined substrings
    ///CRLF sequence
    static substring& crlf()                    { static substring _S("\r\n",2);  return _S; }

    ///Character \n
    static substring& newline()                 { static substring _S("\n",1);  return _S; }

    ///Character 0 (end of string)
    static substring& zero()                    { static substring _S("",1);  return _S; }
    //@}


    ///Initialize with string
    void set( const char* subs, uints len )
    {
        DASSERT( len < 255 );
        if( len >= 255 )
            len = 254;

        _subs = (const uchar*)subs;
        _len = len;

        _from = 255;
        _to = 0;
        for( uints nc=len; nc>0; --nc,++subs ) {
            uchar c = *subs;
            if( c < _from )  _from = c;
            if( c > _to )    _to = c;
        }

        if( _shf && _shf!=&_onebuf )
            delete[] _shf;

        uints n = (uint)_to+1 - _from;
        _shf = new uchar[n];

        //store how many characters remain until the end of the substring
        // from the last occurence of each of the characters contained
        // in the substring

        ::memset( _shf, (uchar)_len+1, n );
        for( uints i=0; i<_len; ++i )
            _shf[ _subs[i] - _from ] = (uchar)(_len-i);
    }

    void set( char k )
    {
        _onechar = k;
        _onebuf = 1;

        _from = _to = k;

        _shf = &_onebuf;
        _subs = &_onechar;
        _len = 1;
    }

    void set( const token& tok );

    ///Length of the contained substring
    uints len() const               { return _len; }

    ///Pointer to string this was initialized with
    const char* ptr() const         { return (const char*)_subs; }

    ///Get substring as token
    token get() const;


    ///Return how many characters can be skipped upon encountering particular character

    ///Find the substring within the provided data
    ///@return position of the substring or \a len if not found
    uints find( const char* ptr, uints len ) const
    {
        if( _len == 1 )
            return find_onechar(ptr,len);

        uints off = 0;
        uints rlen = len;

        while(1)
        {
            if( rlen >= _len )
            {
                if( 0 == ::memcmp( ptr+off, _subs, _len ) )
                    return off;
/*                uints n = memcmplen(ptr+off);
                if( n == _len )
                    //substring found
                    return off;*/
            }
            
            if( _len >= rlen )
                return len;       //end of input, substring cannot be there

            //so the substring wasn't there
            //we can look on the character just behind (the substring length) in the input
            // data, and skip the amount computed during the substring initialization,
            // as the substring cannot lie in that part
            //specifically, if the character we found behind isn't contained within the
            // substring at all, we can skip substring length + 1

            uchar o = ptr[off+_len];
            uints sk = (o<_from || o>_to)  ?  _len+1  :  _shf[o-_from];

            off += sk;
            rlen -= sk;
        }
    }
/*
    ///Compare with string and return length of matching byte sequence
    uints memcmplen( const char* a ) const
    {
        uints off=0;
        for( ; off<_len && a[off] == _subs[off]; ++off );
        return off;
    }
*/
    uints get_skip( uchar k ) const
    {
        return (k<_from || k>_to)  ?  _len+1  :  _shf[k-_from];
    }

protected:
    uints find_onechar( const char* ptr, uints len ) const
    {
        char c = _subs[0];
        uints i;
        for( i=0; i<len; ++i)
            if( ptr[i] == c )
                break;

        return i;
    }
};


COID_NAMESPACE_END

#endif //__COID_COMM_SUBSTRING__HEADER_FILE__