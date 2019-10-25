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
* Portions created by the Initial Developer are Copyright (C) 2018
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

#include "trait.h"
#include <functional>

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
#ifdef COID_VARIADIC_TEMPLATES

#if SYSTYPE_MSVC > 0 && SYSTYPE_MSVC < 1900
//replacement for make_index_sequence
template <size_t... Ints>
struct index_sequence
{
    using type = index_sequence;
    using value_type = size_t;
};

// --------------------------------------------------------------

template <class Sequence1, class Sequence2>
struct _merge_and_renumber;

template <size_t... I1, size_t... I2>
struct _merge_and_renumber<index_sequence<I1...>, index_sequence<I2...>>
    : index_sequence<I1..., (sizeof...(I1)+I2)...>
{ };

// --------------------------------------------------------------

template <size_t N>
struct make_index_sequence
    : _merge_and_renumber<typename make_index_sequence<N/2>::type,
    typename make_index_sequence<N - N/2>::type>
{ };

template<> struct make_index_sequence<0> : index_sequence<> { };
template<> struct make_index_sequence<1> : index_sequence<0> { };

#else

template<size_t Size>
using make_index_sequence = std::make_index_sequence<Size>;

template<size_t... Ints>
using index_sequence = std::index_sequence<Ints...>;

#endif

////////////////////////////////////////////////////////////////////////////////

template <typename Func, int K, typename A, typename ...Args> struct variadic_call_helper
{
    static void call(const Func& f, A&& a, Args&& ...args) {
        f(K, std::forward<A>(a));
        variadic_call_helper<Func, K+1, Args...>::call(f, std::forward<Args>(args)...);
    }
};

template <typename Func, int K, typename A> struct variadic_call_helper<Func, K, A>
{
    static void call(const Func& f, A&& a) {
        f(K, std::forward<A>(a));
    }
};

///Invoke function on variadic arguments
//@param fn function to invoke, in the form (int k, auto&& p)
template<typename Func, typename ...Args>
inline void variadic_call( const Func& fn, Args&&... args ) {
    variadic_call_helper<Func, 0, Args...>::call(fn, std::forward<Args>(args)...);
}

template<typename Func>
inline void variadic_call( const Func& fn ) {}

////////////////////////////////////////////////////////////////////////////////

///Helper to get types and count of lambda arguments
/// http://stackoverflow.com/a/28213747/2435594

template <typename R, typename ...Args>
struct callable_base {
    virtual ~callable_base() {}
    virtual R operator()( Args ...args ) const = 0;
    virtual callable_base* clone() const = 0;
    virtual size_t size() const = 0;
};

template <bool Const, bool Variadic, typename R, typename... Args>
struct closure_traits_base
{
    using arity = std::integral_constant<size_t, sizeof...(Args) >;
    using is_variadic = std::integral_constant<bool, Variadic>;
    using is_const = std::integral_constant<bool, Const>;
    using returns_void = std::is_void<R>;

    using result_type = R;

    template <size_t i>
    using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;

    using callbase = callable_base<result_type, Args...>;

    template <typename Fn>
    struct callable : callbase
    {
        COIDNEWDELETE(callable);

        callable(const Fn& fn) : fn(fn) {}
        callable(Fn&& fn) : fn(std::forward<Fn>(fn)) {}
        
        R operator()(Args ...args) const override final {
            return fn(std::forward<Args>(args)...);
        }

        callbase* clone() const override final {
            return new callable<Fn>(fn);
        }

        size_t size() const override final {
            return sizeof(*this);
        }

    private:
        Fn fn;
    };

    struct function
    {
        template <typename Rx, typename... Argsx>
        function(Rx (*fn)(Argsx...)) : c(0) {
            if (fn)
                c = new callable<decltype(fn)>(fn);
        }

        template <typename Fn>
        function(Fn&& fn) : c(0) {
            c = new callable<typename std::remove_reference<Fn>::type>(std::forward<Fn>(fn));
        }

        function() : c(0) {}
        function(nullptr_t) : c(0) {}

        function(const function& other) : c(0) {
            if (other.c)
                c = other.c->clone();
        }

        function(function&& other) : c(0) {
            c = other.c;
            other.c = 0;
        }

        ~function() { if (c) delete c; }

        function& operator = (const function& other) {
            if (c) {
                delete c;
                c = 0;
            }
            if (other.c)
                c = other.c->clone();
            return *this;
        }

        function& operator = (function&& other) {
            if (c) delete c;
            c = other.c;
            other.c = 0;
            return *this;
        }

        R operator()(Args ...args) const {
            return (*c)(std::forward<Args>(args)...);
        }

        typedef callbase* function::*unspecified_bool_type;

        ///Automatic cast to unconvertible bool for checking via if
        operator unspecified_bool_type() const { return c ? &function::c : 0; }

        callbase* eject() {
            callbase* r = c;
            c = 0;
            return r;
        }

    protected:
        callbase* c;
    };
};

template <typename T>
struct closure_traits : closure_traits<decltype(&T::operator())> {};

template <class R, class... Args>
struct closure_traits<R(Args...)> : closure_traits_base<false,false,R,Args...>
{};

#define COID_REM_CTOR(...) __VA_ARGS__

#define COID_CLOSURE_TRAIT(cv, var, is_var)                                 \
template <typename C, typename R, typename... Args>                         \
struct closure_traits<R (C::*) (Args... COID_REM_CTOR var) cv>              \
    : closure_traits_base<std::is_const<int cv>::value, is_var, R, Args...> \
{};

COID_CLOSURE_TRAIT(const, (,...), 1)
COID_CLOSURE_TRAIT(const, (), 0)
COID_CLOSURE_TRAIT(, (,...), 1)
COID_CLOSURE_TRAIT(, (), 0)


template <typename Fn>
using function = typename closure_traits<Fn>::function;


////////////////////////////////////////////////////////////////////////////////

///Helper to force casts in callbacks
template <class T>
struct base_cast {
    base_cast(T&& v) : value(std::move(v)) {}
    base_cast(const T& v) : value(v) {}

    T value;
};


///A callback function that may contain a member function, a static one or a lambda.
/**
    Size: 2*sizeof(size_t)
    Usage:
        struct something
        {
            static int funs(int, void*) { return 0; }
            int funm(int, void*) { return value; }

            int value = 1;
        };

        int z = 2;
        callback<something, int(int, void*)> fns = &something::funs;
        callback<something, int(int, void*)> fnm = &something::funm;
        callback<something, int(int, void*)> fnl = [](int, void*) { return -1; };
        callback<something, int(int, void*)> fnz = [z](int, void*) { return z; };

        something s;

        DASSERT(fns(&s, 1, 0) == 0);
        DASSERT(fnm(&s, 1, 0) == 1);
        DASSERT(fnl(&s, 1, 0) == -1);
        DASSERT(fnz(&s, 1, 0) == 2);

**/
template <class T, class Fn>
struct callback
{};

template <class T, class R, class ...Args>
struct callback<T, R(Args...)>
{
private:

    union hybrid {
        R(*function)(Args...) = 0;
        R(T::* member)(Args...);
        R(T::* memberc)(Args...) const;
        callable_base<R, Args...>* lambda;
    } _fn;

public:

    callback() {}

    ///A plain function
    callback(R(*fn)(Args...)) {
        _fn.function = fn;
        _caller = &call_static;
    }

    ///A member function pointer
    callback(R(T::* fn)(Args...) const) {
        _fn.memberc = fn;
        _caller = &call_member;
    }

    ///A member function pointer
    callback(R(T::* fn)(Args...)) {
        _fn.member = fn;
        _caller = &call_member;
    }

    ///A derived class member function pointer
    template <class Td, typename std::enable_if<std::is_base_of<T, Td>::value, bool>::type = true>
    callback(R(Td::* fn)(Args...)) {
        static_assert(false, "unsafe cast to a base class, use base_cast(&class::memberfn) in the calling code to override");
    }

    ///A derived class member function pointer
    template <class Td, typename std::enable_if<std::is_base_of<T, Td>::value, bool>::type = true>
    callback(base_cast<R(__cdecl Td::*)(Args...)>&& fn) {
        _fn.member = static_cast<R(T::*)(Args...)>(fn.value);
        _caller = &call_member;
    }

    ///A direct function object
    callback(function<R(Args...)>&& fn) {
        _fn.lambda = fn.eject();
        _caller = &call_lambda;
    }

    ///A non-capturing lambda
    template <class Fn, typename std::enable_if<std::is_constructible<R(*)(Args...), Fn>::value, bool>::type = true>
    callback(Fn&& lambda) {
        _fn.function = lambda;
        _caller = &call_static;
    }

    ///Capturing lambda
    template <class Fn, typename std::enable_if<!std::is_constructible<R(*)(Args...), Fn>::value, bool>::type = true>
    callback(Fn&& lambda) {
        function<R(Args...)> fn = lambda;
        _fn.lambda = fn.eject();
        _caller = &call_lambda;
    }

    ~callback() {
        if (_caller == &call_lambda && _fn.lambda)
            delete _fn.lambda;
    }

    callback(const callback& fn) {
        _caller = fn._caller;
        _fn.lambda = _caller == &call_lambda && fn._fn.lambda
            ? fn._fn.lambda->clone()
            : fn._fn.lambda;
    }

    callback(callback&& fn) {
        _fn.lambda = fn._fn.lambda;
        fn._fn.lambda = 0;
        _caller = fn._caller;
    }

    ///Invoked with T* pointer, which is used only if the bound function was a member pointer
    R operator()(const T* this__, Args ...args) const {
        return _caller(_fn, this__, std::forward<Args>(args)...);
    }

private:

    static R call_static(const hybrid& h, const T* this__, Args ...args) {
        return h.function(std::forward<Args>(args)...);
    }

    static R call_member(const hybrid& h, const T* this__, Args ...args) {
        return (this__->*(h.memberc))(std::forward<Args>(args)...);
    }

    static R call_lambda(const hybrid& h, const T* this__, Args ...args) {
        return (*h.lambda)(std::forward<Args>(args)...);
    }

    R(*_caller)(const hybrid&, const T*, Args...) = 0;
};



COID_NAMESPACE_END

////////////////////////////////////////////////////////////////////////////////

//std function is known to have a non-trivial rebase
namespace coid {
template<class F>
struct has_trivial_rebase<std::function<F>> {
    static const bool value = false;
};
}

#endif //COID_VARIADIC_TEMPLATES
