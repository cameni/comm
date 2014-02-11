
//@file  javascript interface dispatcher generated by intergen

#include "ifc/thingface.js.h"

#include <ot/glm/glm_meta.h>

#include <comm/metastream/metastream.h>
#include <comm/metastream/fmtstream_v8.h>
#include <comm/binstream/filestream.h>
#include <comm/binstream/binstreambuf.h>

using namespace coid;


////////////////////////////////////////////////////////////////////////////////
static void throw_js_error( v8::TryCatch& tc, const char* str )
{
    v8::String::Utf8Value error(tc.Exception());

    v8::HandleScope handle_scope;
    v8::String::Utf8Value exc(tc.Exception());

    v8::Handle<v8::Message> message = tc.Message();
    if(message.IsEmpty()) {
        throw exception() << str << *exc;
    }
    else {
        v8::String::Utf8Value filename(message->GetScriptResourceName());
        const char* filename_string = *filename;
        int linenum = message->GetLineNumber();

        throw exception() << filename_string << '(' << linenum << "): " << *exc;
    }
}

static void _js_release_callback(v8::Persistent<v8::Value> object, void* p)
{
    if(p)
        static_cast<intergen_interface*>(p)->release_refcount();
    object.Dispose();
}

////////////////////////////////////////////////////////////////////////////////
//
// javascript handler of interface thingface of class thing
//
////////////////////////////////////////////////////////////////////////////////

namespace ifc1 {
namespace ifc2 {
namespace js {

////////////////////////////////////////////////////////////////////////////////
class thingface_js_dispatcher
    : public ifc1::ifc2::thingface
{
    v8::Persistent<v8::Context> _context;
    v8::Persistent<v8::Script> _script;
    v8::Persistent<v8::Object> _object;
    v8::Persistent<v8::Function> _events[1];
    bool _bound_events;

    coid::metastream _meta;
    coid::fmtstream_v8 _fmtv8;
    
    iref<policy_intrusive_base> _extref;
    
public:

    v8::Handle<v8::Object> create_interface_object( bool make_weak );

    static v8::Handle<v8::Script> load_script( const coid::token& scriptfile, const coid::token& file_name );
    void bind_events( v8::Handle<v8::Context> context, bool force );

    thingface_js_dispatcher( v8::Persistent<v8::Context> context ) : _bound_events(false) {
        _context = context;
        _meta.bind_formatting_stream(_fmtv8);
    }

    thingface_js_dispatcher( v8::Handle<v8::Context> context ) : _bound_events(false) {
        _context = v8::Persistent<v8::Context>::New(context);
        _meta.bind_formatting_stream(_fmtv8);
    }

    thingface_js_dispatcher( v8::Handle<v8::Context> context, iref<ifc1::ifc2::thingface>& orig ) : _bound_events(false) {
        _context = v8::Persistent<v8::Context>::New(context);
        _meta.bind_formatting_stream(_fmtv8);

        thingface_js_dispatcher* disp = static_cast<thingface_js_dispatcher*>(orig.get());
        _host.swap(disp->_host);
        _vtable = disp->_vtable;
        _cleaner = disp->_cleaner;
        disp->_cleaner = 0;
        _cleaner(this, this);
    }

    ~thingface_js_dispatcher() {
        _events[0].Dispose();
        _object.Dispose();
        _script.Dispose();
        _context.Dispose();
    }

    // --- creators ---

    static iref<thingface_js_dispatcher> get( const script_handle& scriptpath, const coid::token& bindname, v8::Handle<v8::Context>* );

    static v8::Handle<v8::Value> v8creator_get0(const v8::Arguments& args);

    ///Handler for generic $query_interface javascript method
    static v8::Handle<v8::Value> v8query_interface( const v8::Arguments& args );
    
    // --- method wrappers ---

    static v8::Handle<v8::Value> v8_hallo0(const v8::Arguments& args);
    static v8::Handle<v8::Value> v8_fallo1(const v8::Arguments& args);

    // --- interface events ---

    virtual void boo( const char* key ) override;
};

////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Value> thingface_js_dispatcher::v8_hallo0(const v8::Arguments& args)
{
    if(args.Length() != 2)  //in/inout arguments
        return v8::ThrowException(v8::String::New("Wrong number of arguments"));

    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = args.Holder();

    ifc1::ifc2::js::thingface_js_dispatcher* ifc = static_cast<ifc1::ifc2::js::thingface_js_dispatcher*>
        (v8::Handle<v8::External>::Cast(obj->GetInternalField(0))->Value());

    v8::Context::Scope context_scope(ifc->_context);

    //stream the arguments in
    static_assert( CHECK::meta_operator_exists<int>::value, "missing metastream operator << for type 'int'" );
    int a = v8_streamer<int>(&ifc->_fmtv8, &ifc->_meta) >> args[0];

    static_assert( CHECK::meta_operator_exists<coid::charstr>::value, "missing metastream operator << for type 'coid::charstr'" );
    coid::charstr b = v8_streamer<coid::charstr>(&ifc->_fmtv8, &ifc->_meta) >> args[1];

    ifc->_meta.stream_acknowledge();

    //invoke
    coid::charstr c;

    int _rval_ = ifc->hallo(a, b, c);

    //stream out
    static_assert( CHECK::meta_operator_exists<int>::value, "missing metastream operator << for type 'int'" );
    v8::Handle<v8::Object> _r_ = v8::Object::New();
    _r_->Set(v8::String::NewSymbol("$ret"), v8_streamer<int>(&ifc->_fmtv8, &ifc->_meta) << _rval_);

    static_assert( CHECK::meta_operator_exists<coid::charstr>::value, "missing metastream operator << for type 'coid::charstr'" );
    _r_->Set(v8::String::NewSymbol("c"), v8_streamer<coid::charstr>(&ifc->_fmtv8, &ifc->_meta) << c);
 
    ifc->_meta.stream_flush();

    return handle_scope.Close(_r_);
}

////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Value> thingface_js_dispatcher::v8_fallo1(const v8::Arguments& args)
{
    if(args.Length() != 2)  //in/inout arguments
        return v8::ThrowException(v8::String::New("Wrong number of arguments"));

    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = args.Holder();

    ifc1::ifc2::js::thingface_js_dispatcher* ifc = static_cast<ifc1::ifc2::js::thingface_js_dispatcher*>
        (v8::Handle<v8::External>::Cast(obj->GetInternalField(0))->Value());

    v8::Context::Scope context_scope(ifc->_context);

    //stream the arguments in
    static_assert( CHECK::meta_operator_exists<bool>::value, "missing metastream operator << for type 'bool'" );
    bool b = v8_streamer<bool>(&ifc->_fmtv8, &ifc->_meta) >> args[0];

    static_assert( CHECK::meta_operator_exists<coid::charstr>::value, "missing metastream operator << for type 'coid::charstr'" );
    coid::charstr str = v8_streamer<coid::charstr>(&ifc->_fmtv8, &ifc->_meta) >> args[1];

    ifc->_meta.stream_acknowledge();

    //invoke
    coid::charstr _rval_ = ifc->fallo(b, str.c_str());

    //stream out
    static_assert( CHECK::meta_operator_exists<coid::charstr>::value, "missing metastream operator << for type 'coid::charstr'" );
    v8::Handle<v8::Value> _r_ = v8_streamer<coid::charstr>(&ifc->_fmtv8, &ifc->_meta) << _rval_;

    ifc->_meta.stream_flush();

    return handle_scope.Close(_r_);
}

////////////////////////////////////////////////////////////////////////////////
void thingface_js_dispatcher::boo( const char* key )
{
    v8::Context::Scope context_scope(_context);
    v8::HandleScope __handle_scope;
    v8::TryCatch __trycatch;

    bind_events(_context, false);

    if(_events[0].IsEmpty() || _events[0]->IsUndefined())
        return;

    static_assert( CHECK::meta_operator_exists<coid::charstr>::value, "missing metastream operator << for type 'coid::charstr'" );
    v8::Handle<v8::Value> __inargs[] = {
        v8_streamer<coid::charstr>(&_fmtv8, &_meta) << key,
    };

    _meta.stream_flush();

    v8::Local<v8::Value> _r_ = _events[0]->Call(_object, sizeof(__inargs)/sizeof(v8::Handle<v8::Value>), __inargs);

    if(__trycatch.HasCaught())
        throw_js_error(__trycatch, "ifc1::ifc2::js::thingface::boo(): ");

    _meta.stream_acknowledge();
}

////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Value> thingface_js_dispatcher::v8query_interface(const v8::Arguments& args)
{
    if(args.Length() < 1)
        return v8::ThrowException(v8::String::New("interface creator name required"));

    v8::HandleScope handle_scope;
    v8::String::AsciiValue key(args[0]);
    
    typedef v8::Handle<v8::Value> (*fn_get)(const v8::Arguments& args);
    fn_get get = reinterpret_cast<fn_get>(
        coid::interface_register::get_interface_creator(coid::token(*key, key.length())));

    if(!get)
        return v8::ThrowException(v8::String::New("interface creator not found"));

    return handle_scope.Close(get(args));
}

////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Object> thingface_js_dispatcher::create_interface_object( bool make_weak )
{
    static v8::Persistent<v8::ObjectTemplate> _objtempl;
    if(_objtempl.IsEmpty())
    {
        _objtempl = v8::Persistent<v8::ObjectTemplate>::New( v8::ObjectTemplate::New() );
        _objtempl->SetInternalFieldCount(2);    //ptr and class hash id

        _objtempl->Set(v8::String::NewSymbol("hallo"), v8::FunctionTemplate::New(&v8_hallo0));
        _objtempl->Set(v8::String::NewSymbol("fallo"), v8::FunctionTemplate::New(&v8_fallo1));
        _objtempl->Set(v8::String::NewSymbol("$query_interface"), v8::FunctionTemplate::New(&v8query_interface));
    }

    v8::Local<v8::Object> obj = _objtempl->NewInstance();

    v8::Handle<v8::External> map_ptr = v8::External::New(this);
    obj->SetInternalField(0, map_ptr);
    v8::Handle<v8::External> hash_ptr = v8::External::New((void*)2012066119);
    obj->SetInternalField(1, hash_ptr);

    if(make_weak) {
        v8::Persistent<v8::Object>::New(obj).MakeWeak(this, _js_release_callback);
        add_refcount();
    }

    return obj;
}

////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Script> thingface_js_dispatcher::load_script( const coid::token& script, const coid::token& fname )
{
    v8::Local<v8::String> scriptv8 = v8::String::New(script.ptr(), script.len());

    // set up an error handler to catch any exceptions the script might throw.
    v8::TryCatch __trycatch;

    v8::Handle<v8::Script> compiled_script = v8::Script::Compile(scriptv8, v8::String::New(fname.ptr(), fname.len()));
    if(__trycatch.HasCaught())
        throw_js_error(__trycatch, "ifc1::ifc2::js::thingface::load_script(): ");

    compiled_script->Run();
    if(__trycatch.HasCaught())
        throw_js_error(__trycatch, "ifc1::ifc2::js::thingface::load_script(): ");

    return compiled_script;
}

////////////////////////////////////////////////////////////////////////////////
void thingface_js_dispatcher::bind_events( v8::Handle<v8::Context> context, bool force )
{
    if(!force && _bound_events)
        return;

    static token names[] = {
        "boo",
    };
    static bool mandatory[] = {
        false,
    };

    v8::TryCatch __trycatch;
    v8::Local<v8::Object> global = context->Global();

    for(int i=0; i<1; ++i) {
        v8::Local<v8::Value> var = global->Get(v8::String::New(names[i].ptr(), names[i].len()));
        if(__trycatch.HasCaught()) {
            if(!mandatory[i]) continue;
            throw_js_error(__trycatch, "ifc1::ifc2::js::thingface::bind_events(): ");
        }

        v8::Local<v8::Function> foo = v8::Local<v8::Function>::Cast(var);
        if(__trycatch.HasCaught() && mandatory[i])
            throw_js_error(__trycatch, "ifc1::ifc2::js::thingface::bind_events(): ");

        _events[i] = v8::Persistent<v8::Function>::New(foo);
    }

    _bound_events = true;
}

////////////////////////////////////////////////////////////////////////////////
iref<thingface_js_dispatcher> thingface_js_dispatcher::get( const script_handle& script, const coid::token& bindname, v8::Handle<v8::Context>* ctx )
{
    v8::Persistent<v8::Context> context_per;
    v8::Handle<v8::Context> context;

    iref<policy_intrusive_base> extref;

    // check if an external context provider is required
    if(script.is_path()) {
        coid::token path = script.str();
        path.cut_right('?');
        
        bool ispage = path.begins_with("http://") || path.begins_with("https://") || path.ends_with(".html");
        bool isscript = path.ends_with(".js");

        if(ispage && !isscript) {
            typedef iref<policy_intrusive_base> (*fn_getctx)(const coid::token&, v8::Handle<v8::Context>*);

            fn_getctx ctxgetter = reinterpret_cast<fn_getctx>(
                    coid::interface_register::get_interface_creator("~html@system_context_getter"));

            if(!ctxgetter)
                throw coid::exception() << "unable to acquire the system context creator interface";

            extref = ctxgetter(script.str(), &context);
        }
    }

    if(extref)
        DASSERT(!context.IsEmpty());
    else if(script.is_context())
        context = script.context();
    else {
        context_per = v8::Context::New();
        context = context_per;
    }

    v8::Context::Scope context_scope(context);
    v8::HandleScope scope;

    // create interface object
    iref<thingface_js_dispatcher> ifc;

    if(extref || script.is_context()) {
        ifc = ifc1::ifc2::thingface::get(new thingface_js_dispatcher(context));
        
        ifc->_object = v8::Persistent<v8::Object>::New(ifc->create_interface_object(true));
    }
    else {
        ifc = ifc1::ifc2::thingface::get(new thingface_js_dispatcher(context_per));

        coid::token script_tok, script_path;
        coid::charstr script_tmp;
        if(script.is_path()) {
            script_path = script.str();
            coid::bifstream bif(script_path);
            if(!bif.is_open())
                throw coid::exception() << script_path << " not found";
                
            script_tmp = script.prefix();

            coid::binstreambuf buf;
            buf.swap(script_tmp);
            buf.transfer_from(bif);
            buf.swap(script_tmp);
            
            script_tok = script_tmp;
        }
        else if(script.prefix()) {
            script_tmp << script.prefix() << script.str();
            script_tok = script_tmp;
            script_path = "thingface";
        }
        else {
            script_tok = script.str();
            script_path = "thingface";
        }

        v8::Handle<v8::Script> compiled_script = load_script(script_tok, script_path);

        ifc->_script = v8::Persistent<v8::Script>::New(compiled_script);
        ifc->_object = v8::Persistent<v8::Object>::New(ifc->create_interface_object(false));
    }

    if(extref)
        extref.swap(ifc->_extref);

    if(bindname)
        context->Global()->Set(v8::String::New(bindname.ptr(), bindname.len()), ifc->_object);

    if(ctx) *ctx = context;

    return ifc;
}

////////////////////////////////////////////////////////////////////////////////
///Creator methods for access from JS
v8::Handle<v8::Value> thingface_js_dispatcher::v8creator_get0(const v8::Arguments& args)
{
    if(args.Length() != 1+0)  //fnc name + in/inout arguments
        return v8::ThrowException(v8::String::New("Wrong number of arguments"));

    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = args.Holder();

    ifc1::ifc2::js::thingface_js_dispatcher* ifc = static_cast<ifc1::ifc2::js::thingface_js_dispatcher*>
        (v8::Handle<v8::External>::Cast(obj->GetInternalField(0))->Value());

    v8::Context::Scope context_scope(ifc->_context);

    //stream the arguments in
    ifc->_meta.stream_acknowledge();

    //invoke
    iref<thingface_js_dispatcher> nifc = ifc1::ifc2::thingface::get(new thingface_js_dispatcher(ifc->_context));

    //stream out
    v8::Handle<v8::Value> _r_ = nifc->create_interface_object(true);

    ifc->_meta.stream_flush();

    return handle_scope.Close(_r_);
}

////////////////////////////////////////////////////////////////////////////////
void* register_binders_for_thingface()
{
    interface_register::register_interface_creator(
        "ifc1::ifc2::js::thingface::get@creator", (void*)&thingface_js_dispatcher::get);
    interface_register::register_interface_creator(
        "ifc1::ifc2::js::thingface::get", (void*)&thingface_js_dispatcher::v8creator_get0);

    return (void*)&register_binders_for_thingface;
}

//auto-register the bind function
static void* autoregger_thingface = register_binders_for_thingface();

} //namespace js
} //namespace
} //namespace

////////////////////////////////////////////////////////////////////////////////
///Create JS wrapper from existing interface object
v8::Handle<v8::Object> js_ifc1_ifc2_thingface_create_wrapper( iref<ifc1::ifc2::thingface>& orig, v8::Handle<v8::Context> context )
{
    // check that the orig points to an object
    if(!orig) return v8::Handle<v8::Object>();

    // create interface object
    iref<ifc1::ifc2::js::thingface_js_dispatcher> ifc;
    ifc.create(new ifc1::ifc2::js::thingface_js_dispatcher(context, orig));

    v8::Context::Scope context_scope(context);
    v8::HandleScope scope;

    v8::Handle<v8::Object> obj = ifc->create_interface_object(true);

    return scope.Close(obj);
}
