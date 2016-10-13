
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

#ifndef __COMM_LOGGER_H__
#define __COMM_LOGGER_H__

#include "../binstream/filestream.h"
#include "../binstream/txtstream.h"
#include "../str.h"
#include "../ref.h"
#include "../singleton.h"
#include "policy_log.h"
#include "../atomic/queue_base.h"
#include "../atomic/pool_base.h"

COID_NAMESPACE_BEGIN

enum class ELogType {
    None = -1,
    Exception=0,
    Error,
    Warning,
    Info,
    Highlight,
    Debug,
    Perf,
    Last,
};

class logger_file;
class logger;
class logmsg;
class policy_msg;

/* 
 * Log message object, returned by the logger.
 * Message is written in policy_log::release() upon calling destructor of ref
 * with policy policy_log.
 */
class logmsg 
    //: public policy_pooled<logmsg>
{
protected:

    friend class policy_msg;

    logger* _logger;
    ref<logger_file> _logger_file;

    ELogType _type;
    charstr _str;

public:

    logmsg() : _logger(0), _type(ELogType::None)
    {}

    logmsg( logmsg&& other )
        : _type(other._type)
    {
        _logger = other._logger;
        other._logger = 0;

        _logger_file.takeover(other._logger_file);
        _str.takeover(other._str);
    }

    void set_logger( logger* l ) { _logger = l; }

	void reset() {
        _str.reset();
        _logger = 0;
        _logger_file.release();
    }

    void write();

    ///Consume type prefix from the message
    static ELogType consume_type( token& msg )
    {
        static token ERR = "error:";
        static token WARN = "warning:";
        static token INFO = "info:";
        static token MSG = "msg:";
        static token DBG1 = "dbg:";
        static token DBG2 = "debug:";
        static token PERF = "perf:";

        ELogType t = ELogType::Info;
        if(msg.consume_icase("error:"))
            t = ELogType::Error;
        else if(msg.consume_icase("warn:") || msg.consume_icase("warning:"))
            t = ELogType::Warning;
        else if(msg.consume_icase("info:"))
            t = ELogType::Info;
        else if(msg.consume_icase("msg:"))
            t = ELogType::Highlight;
        else if(msg.consume_icase("dbg:") || msg.consume_icase("debug:"))
            t = ELogType::Debug;
        else if(msg.consume_icase("perf:"))
            t = ELogType::Perf;

        msg.skip_whitespace();
        return t;
    }

    static const token& type2tok( ELogType t )
    {
        static token st[1 + int(ELogType::Last)]={
            "",
            "FATAL: ",
            "ERROR: ",
            "WARNING: ",
            "INFO: ",
            "INFO: ",
            "DEBUG: ",
            "PERF: ",
        };
        static token empty;

        return t<ELogType::Last ? st[1 + int(t)] : empty;
    }

    ELogType deduce_type() {
        token tok = _str;
        ELogType t = consume_type(tok);
        //if(tok.ptr() > _str.ptr())
        //    _str.del(0, tok.ptr() - _str.ptr());
        return t;
    }

    ELogType get_type() const { return _type; }

	void set_type(ELogType t) { _type = t; }

    charstr& str() { return _str; }
    const charstr& str() const { return _str; }

protected:

    void finalize( policy_msg* p );
};

typedef ref<logmsg> logmsg_ptr;

/* 
 * USAGE :
 *
 * class fwmlogger : public logger
 * {
 * public:
 *     fwmlogger() : logger("fwm.log") {}
 * };
 * #define fwmlog(msg) (*SINGLETON(fwmlogger)()<<msg)
 * 
 * USAGE:
 * 
 * fwmlog("ugh" << x << "asd")
 */
class logger
{
protected:

	ref<logger_file> _logfile;

    bool _stdout;

public:

	logger( bool std_out=false );

    void open(const token& filename);

    void post( token msg );

    template<class ...Vs>
    void print( token msg, Vs&&... vs ) {
        ref<logmsg> msgr = create_msg(ELogType::None);
        if(!msgr)
            return;

        charstr& str = msgr->str();
        str.print(msg, std::forward<Vs>(vs)...);
    }

    template<class ...Vs>
    void print( ELogType type, const tokenhash& hash, const void* inst, token msg, Vs&&... vs )
    {
        ref<logmsg> msgr = create_msg(type, hash, inst);
        if(!msgr)
            return;

        charstr& str = msgr->str();
        str.print(msg, std::forward<Vs>(vs)...);
    }

    //@return logmsg, filling the prefix by the log type (e.g. ERROR: )
    ref<logmsg> operator()( ELogType type = ELogType::Info );

    //@return an empty logmsg object
    ref<logmsg> create_msg( ELogType type );

    ///Creates logmsg object if given log message type is enabled
    //@param type log level
    //@param hash tokenhash identifying the client (interface) name
    //@param inst optional instance id
    //@return logmsg reference or null if not enabled
    ref<logmsg> create_msg( ELogType type, const tokenhash& hash, const void* inst );

    const ref<logger_file>& file() const { return _logfile; }

    virtual void enqueue( ref<logmsg>& msg );

    virtual void write( const logmsg& msg );

	void flush();
};

COID_NAMESPACE_END

#endif // __COMM_LOGGER_H__
