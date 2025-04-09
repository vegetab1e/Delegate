#pragma once

#include <cstddef>

#include <iostream>
#include <iomanip>

#include <memory>
#include <utility>

#include <tuple>

#include <limits>
#include <type_traits>

#include "type_name.h"

struct IArguments
{
    virtual ~IArguments() = 0;
};

inline IArguments::~IArguments() = default;

#ifdef PERFECT_FORWARDING
template<class... Types>
struct Arguments final: IArguments
{
    explicit Arguments(Types&&... arguments) noexcept
        : tuple_(std::forward_as_tuple(std::forward<Types>(arguments)...))
    {
    }

    const std::tuple<Types&&...> tuple_;
};
#else
template<class... Types>
class Arguments final: public IArguments
{
public:
    using Tuple = std::tuple<Types...>;

    Arguments(Arguments const&) = delete;
    Arguments(Arguments&&) = default;
    Arguments& operator=(Arguments const&) = delete;
    Arguments& operator=(Arguments&&) = default;

    explicit Arguments(Tuple&& tuple) noexcept
        : tuple_(std::move(tuple))
    {
    }

    const Tuple& getData() const & noexcept
    {
        return tuple_;
    }

    Tuple&& getData() && noexcept
    {
        return std::move(tuple_);
    }

private:
    Tuple tuple_;
};
#endif


struct ICallback
{
    virtual void invokeMethod(IArguments&&) const noexcept = 0;

    virtual ~ICallback() = 0;
};

inline ICallback::~ICallback() = default;

#if __cplusplus < 201402L
template<std::size_t...>
struct IndexSequence
{
};

template<std::size_t N, std::size_t... S>
struct MakeIndexSequence: MakeIndexSequence<N - 1, N - 1, S...>
{
};

template<std::size_t... S>
struct MakeIndexSequence<0, S...>
{
    using Type = IndexSequence<S...>;
};
#endif

template<class, class...>
class Callback;

template<class Object, class... Types>
class Callback<Object, void (Object::*)(Types...)> final: public ICallback
{
    using Method = void (Object::*)(Types...);

public:
    Callback(const std::shared_ptr<Object>& object, Method method) noexcept
        : object_(object)
        , method_(method)
    {
    }

    void invokeMethod(IArguments&& arguments) const noexcept override
    try
    {
        invokeMethod(
#ifdef PERFECT_FORWARDING
            dynamic_cast<Arguments<Types...>&&>(arguments).tuple_,
#else
            dynamic_cast<Arguments<std::decay_t<Types>...>&&>(arguments).getData(),
#endif
#if __cplusplus < 201402L
            typename MakeIndexSequence<sizeof...(Types)>::Type()
#else
            std::index_sequence_for<Types...>()
#endif
        );
    }
    catch (const std::bad_cast& exception)
    {
#ifndef NDEBUG
#ifdef _MSC_VER
        std::cout << __FUNCSIG__ << '\n';
#else
        std::cout << __PRETTY_FUNCTION__ << '\n';
#endif
        std::cout << "typeid(arguments): " << typeid(arguments).name() << '\n';
#endif
        std::cout << exception.what() << "\n\n";
    }

private:
    template<class Tuple, std::size_t... indexes>
    void invokeMethod(
        Tuple&& arguments,
#if __cplusplus < 201402L
        IndexSequence<indexes...>
#else
        std::index_sequence<indexes...>
#endif
    ) const noexcept
    try
    {
        const auto shared_pointer{object_.lock()};
        const auto raw_pointer{shared_pointer.get()};
        if (!!raw_pointer && !!method_)
            (raw_pointer->*method_)(
#ifdef PERFECT_FORWARDING
                std::forward<std::tuple_element_t<indexes, Tuple>>(std::get<indexes>(arguments))...
#else
                std::forward<Types>(std::get<indexes>(arguments))...
#endif
            );
    }
    catch (...)
    {
#ifndef NDEBUG
#ifdef _MSC_VER
        std::cout << __FUNCSIG__ << '\n';
#else
        std::cout << __PRETTY_FUNCTION__ << '\n';
#endif
        std::cout << "typeid(method): " << typeid(method_).name() << '\n';
        std::cout << "typeid(arguments): " << typeid(arguments).name() << '\n';
#endif
        std::cout << "Exception catched!" << "\n\n";
    }

    const std::weak_ptr<Object> object_;
    const Method method_;
};


class Delegate final
{
public:
    template<class Object, class Method>
    void connect(const std::shared_ptr<Object>& object, Method method)
    {
        if (object && !!method)
            callback_ = std::make_unique<Callback<Object, Method>>(object, method);
    }

    template<class... Types>
    void operator()(Types&&... arguments) const
    {
        if (const ICallback* raw_pointer = callback_.get())
        {
#ifndef NDEBUG
#ifdef _MSC_VER
            std::cout << __FUNCSIG__ << '\n';
#else
            std::cout << __PRETTY_FUNCTION__ << '\n';
#endif

            if (!!sizeof...(Types))
            {
                using namespace std;

                const auto default_precision{cout.precision()};

                size_t i;
                using Array = decltype(i)[];
                const Array column_sizes = { 0, type_name<decltype(arguments)>().size()... };
                auto info = [&i, &column_sizes, pack_size = sizeof...(Types)](const string& type_name, const auto& value){
                    using Type = decay_t<decltype(value)>;
                    if (is_floating_point<Type>::value)
                        cout << fixed << setprecision(numeric_limits<Type>::max_digits10);
                    cout << setw(column_sizes[i]) << type_name << '(' << value << (i < pack_size ? "), " : ")\n");
                };
                (void)Array{ (cout << "Types:\t\t",        i = 1), (info(type_name<Types>(),                               arguments), ++i)... };
                (void)Array{ (cout << "decay_t<Types>:\t", i = 1), (info(type_name<decay_t<Types>>(),                      arguments), ++i)... };
                (void)Array{ (cout << "decltype(args):\t", i = 1), (info(type_name<decltype(arguments)>(),                 arguments), ++i)... };
                (void)Array{ (cout << "forward<Types>:\t", i = 1), (info(type_name<decltype(forward<Types>(arguments))>(), arguments), ++i)... };

                cout << defaultfloat << setprecision(default_precision) << endl;
            }
#endif

            raw_pointer->invokeMethod(
#ifdef PERFECT_FORWARDING
                // добавление двух внешних амперсандов (rvalue reference) нужно для того, чтобы помочь коду
                // при динамическом приведении типов, то есть увеличить вероятность успешного сопоставления
                // типов переданного шаблона аргументов с требуемыми вызываемой функцией. На самом деле эти
                // амперсанды уже есть там где это необходимо в кортеже и добавление внешних не повлияет на
                // типы переменных, так как при сжатии/сворачивании ссылок (reference collapsing) они будут
                // просто убраны: кортеж содержит только rvalue и lvalue ссылки, а по правилу сжатия ссылок
                // && + && -> && и && + & -> &. Это не решение задачи, это демонстрация самого оптимального
                // способа передачи аргументов в функцию при её вызове "прямо здесь и сейчас", к сожалению,
                // нереализуемого в рамках поставленных условий, так как вариантов может быть множество, но
                // узнать правильный будет невозможно после передачи шаблона через нешаблонный интерфейс.
                Arguments<Types&&...>{std::forward<Types>(arguments)...}
#else
                Arguments<std::decay_t<Types>...>{std::make_tuple(std::forward<Types>(arguments)...)}
#endif
            );
        }
    }

private:
    std::unique_ptr<ICallback> callback_;
};
