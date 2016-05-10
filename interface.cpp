
#include "interface.h"
#include "commexception.h"
#include "hash/hashkeyset.h"
#include "sync/mutex.h"
#include "dir.h"

#include <cstdio>

COID_NAMESPACE_BEGIN

struct entry
{
    token ifcname;
    token classname;
    token creatorname;
    token ns;

    void* creator_ptr;
    bool script_creator;

    operator const token&() const { return ifcname; }
};

static void _default_log( const token& msg )
{
    fwrite(msg.ptr(), 1, msg.len(), stdout);
    fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
class interface_register_impl : public interface_register
{
public:

    hash_keyset<entry, _Select_GetRef<entry,token> > _hash;
    comm_mutex _mx;

    charstr _root_path;
    interface_register::fn_log_t _fn_log;
    interface_register::fn_acc_t _fn_acc;

    static interface_register_impl& get();

    interface_register_impl() : _mx(500, false)
        , _fn_log(0)
        , _fn_acc(0)
    {}

    bool current_dir( token curpath, charstr& dst )
    {
        if(directory::is_absolute_path(curpath))
            dst = curpath;
        else {
            curpath.consume_icase("file:///");
            dst = _root_path;
            
            return directory::append_path(dst, curpath, true);
        }

        return true;
    }

    bool include_path( const token& curpath, const token& incpath, charstr& dst )
    {
        bool slash = incpath.first_char() == '/' || incpath.first_char() == '\\';
        DASSERT( !_root_path.is_empty() );

        if(!slash && !directory::is_absolute_path(incpath))
        {
            //relative
            if(!current_dir(curpath, dst) || !directory::append_path(dst, incpath, true))
                return false;
        }
        else {
            //absolute
            dst = _root_path;

            token append = incpath;
            if(slash)
                ++append;

            if(!directory::append_path(dst, append, true))
                return false;
        }

        if(!directory::compact_path(dst, '/'))
            return false;

        token relpath = dst;
        if(!directory::subpath(_root_path, relpath))
            return false;

        return _fn_acc ? _fn_acc(relpath) : true;
    }
};

////////////////////////////////////////////////////////////////////////////////
void* interface_register::get_interface_creator( const token& ifcname )
{
    interface_register_impl& reg = interface_register_impl::get();
    GUARDTHIS(reg._mx);

    const entry* en = reg._hash.find_value(ifcname);

    return en ? en->creator_ptr : 0;
}

////////////////////////////////////////////////////////////////////////////////
bool interface_register::include_path( const token& curpath, const token& incpath, charstr& dst )
{
    interface_register_impl& reg = interface_register_impl::get();
    return reg.include_path(curpath, incpath, dst);
}

////////////////////////////////////////////////////////////////////////////////
const charstr& interface_register::root_path()
{
    return interface_register_impl::get()._root_path;
}

////////////////////////////////////////////////////////////////////////////////
void interface_register::log( const token& msg )
{
    fn_log_t logfn = interface_register_impl::get()._fn_log;

    if(logfn)
        logfn(msg);
    else
        _default_log(msg);
}

////////////////////////////////////////////////////////////////////////////////
void interface_register::setup( const token& path, fn_log_t log, fn_acc_t access )
{
    interface_register_impl& reg = interface_register_impl::get();
    if(!reg._root_path) {
        reg._root_path = path;
        directory::treat_trailing_separator(reg._root_path, true);
    }

    if(log)
        reg._fn_log = log;

    if(access)
        reg._fn_acc = access;
}

////////////////////////////////////////////////////////////////////////////////
dynarray<interface_register::creator>& interface_register::get_interface_creators( const token& name, dynarray<interface_register::creator>& dst )
{
    //interface creator names:
    // [ns1::[ns2:: ...]]::class.creator
    static token SEP = "::";
    token ns = name;
    token classname = ns.cut_right_group_back(SEP);
    token creatorname = classname.cut_right('.', token::cut_trait_remove_sep_default_empty());

    interface_register_impl& reg = interface_register_impl::get();
    GUARDTHIS(reg._mx);

    auto i = reg._hash.begin();
    auto ie = reg._hash.end();
    for(; i!=ie; ++i) {
        if(i->script_creator)
            continue;

        if(i->classname != classname)
            continue;

        if(creatorname && i->creatorname != creatorname)
            continue;

        if(ns && i->ns != ns)
            continue;

        creator* p = dst.add();
        p->creator_ptr = i->creator_ptr;
        p->name = token(i->ifcname);
    }

    return dst;
}

////////////////////////////////////////////////////////////////////////////////
dynarray<interface_register::creator>& interface_register::find_interface_creators( const regex& name, dynarray<interface_register::creator>& dst )
{
    //interface creator names:
    // [ns1::[ns2:: ...]]::class.creator

    interface_register_impl& reg = interface_register_impl::get();
    GUARDTHIS(reg._mx);

    auto i = reg._hash.begin();
    auto ie = reg._hash.end();
    for(; i!=ie; ++i) {
        if(i->script_creator)
            continue;

        if(name.match(i->ifcname)) {
            creator* p = dst.add();
            p->creator_ptr = i->creator_ptr;
            p->name = token(i->ifcname);
        }
    }

    return dst;
}

////////////////////////////////////////////////////////////////////////////////
void interface_register::register_interface_creator( const token& ifcname, void* creator_ptr )
{
    static token SEP = "::";

    token ns = ifcname;
    token classname = ns.cut_right_group_back(SEP);
    token creatorname = classname.cut_right('.');
    token script = creatorname.cut_right('@', token::cut_trait_remove_sep_default_empty());

    interface_register_impl& reg = interface_register_impl::get();
    GUARDTHIS(reg._mx);

    entry* en = reg._hash.insert_value_slot(ifcname);
    if(en) {
        en->creator_ptr = creator_ptr;
        en->ifcname = ifcname;
        en->ns = ns;
        en->classname = classname;
        en->creatorname = creatorname;
        en->script_creator = !script.is_empty();
    }
}

////////////////////////////////////////////////////////////////////////////////

//#ifdef _DEBUG
//#define INTERGEN_GLOBAL_REGISTRAR iglo_registrard
//#else
#define INTERGEN_GLOBAL_REGISTRAR iglo_registrar
//#endif



#ifdef SYSTYPE_WIN

typedef int (__stdcall *proc_t)();

extern "C"
__declspec(dllimport) proc_t __stdcall GetProcAddress (
    void* hmodule,
    const char* procname
    );

typedef interface_register_impl* (*ireg_t)();

#define MAKESTR(x) STR(x)
#define STR(x) #x

////////////////////////////////////////////////////////////////////////////////
interface_register_impl& interface_register_impl::get()
{
    static interface_register_impl* _this=0;

    if(!_this) {
        const char* s = MAKESTR(INTERGEN_GLOBAL_REGISTRAR);
        ireg_t p = (ireg_t)GetProcAddress(0, s);
        if(!p) throw exception() << "no intergen entry point found";
        _this = p();
    }

    return *_this;
}


extern "C" __declspec(dllexport) interface_register_impl* INTERGEN_GLOBAL_REGISTRAR()
{
    return &SINGLETON(interface_register_impl);
}


#else
/*
extern "C" __attribute__ ((visibility("default"))) interface_register_impl* INTERGEN_GLOBAL_REGISTRAR();
{
    return &SINGLETON(interface_register_impl);
}*/

interface_register_impl& interface_register_impl::get()
{
    static interface_register_impl _this;

    return _this;
}

#endif

COID_NAMESPACE_END
