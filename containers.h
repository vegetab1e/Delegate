#pragma once

#include <cstddef>

#include <iostream>

#include <memory>
#include <utility>

#include <tuple>

#include <typeinfo>

#include "ifaces.h"
#include "tutils.h"

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

    const Tuple& getData() const& noexcept
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
            // Здесь, как и при создании шаблона аргументов, внешние
            // амперсанды нужны для увеличения вероятности успешного
            // сопоставления типов, потому что у самурая нет цели, а
            // есть только путь=) Потому что в кортеже нет значений,
            // а есть только ссылки, поэтому, если функции требуется
            // значение, то в кортеже может быть либо rvalue-ссылка,
            // либо lvalue-ссылка, и необязательно константная, даже
            // константную rvalue-ссылку исключить нельзя. Вычислить
            // тип ссылки в кортеже на этом этапе уже невозможно, но
            // можно попытаться сохранить его при создании структуры
            // и передать сюда с помощью перечисления. Так же делают
            // при запрете на использование RTTI; когда в vtable нет
            // возможности заглянуть - приходится создавать подобные
            // конструкции. Однако, это нарушит дух условий задания,
            // не букву, но всё же. Также можно попробовать обойтись
            // только lvalue-ссылками, но там будут с константностью
            // проблемы, возможно даже нерешаемые, попробовать стоит
            dynamic_cast<Arguments<Types&&...>&&>(arguments).tuple_,
#else
            dynamic_cast<Arguments<std::decay_t<Types>...>&&>(arguments).getData(),
#endif
#if __cplusplus < 201402L
            typename MakeIndexSequence<sizeof...(Types)>::Type()
#else
            std::make_index_sequence<sizeof...(Types)>()
#endif
        );
    }
    catch (const std::bad_cast& exception)
    {
#ifndef NDEBUG
#if defined(_MSC_VER)
        std::cout << __FUNCSIG__ << '\n';
#elif defined(__GNUC__)
        std::cout << __PRETTY_FUNCTION__ << '\n';
#else
        std::cout << __func__ << '\n';
#endif
        std::cout << "typeid(arguments): " << typeid(arguments).name() << '\n';
#endif
        std::cout << exception.what() << "\n\n";
    }

private:
    template<class Tuple, std::size_t... indexes>
    void invokeMethod(Tuple&& arguments,
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
        if (!raw_pointer || !method_)
            return;
#ifndef PERFECT_FORWARDING
#ifndef SAFE_MODE
        FunctionSignature<Method>::checkReferencesType();
#else
        static_assert(!checkReferencesTypeWrap<Types...>(),
                      "The function requires a non-const reference(s)!");
#endif // !SAFE_MODE
#endif // !PERFECT_FORWARDING
        (raw_pointer->*method_)(
// TODO: Добавить описание
#if defined(FORWARD_AS_IS)
            std::get<indexes>(std::forward<Tuple>(arguments))...
#elif defined(PERFECT_FORWARDING)
            // В данном случае в кортеже только ссылки
            std::get<indexes>(std::move(arguments))...
#else
            // Это не идеальная передача! Используется для того,
            // чтобы дать возможность методу перемещать значения
            // из кортежа, т.е. небольшая оптимизация безнадёжно
            // неэффективного способа передачи аргументов, также
            // способного привести к неожиданному поведению кода
            std::forward<Types>(std::get<indexes>(arguments))...
#endif
            );
    }
    catch (...)
    {
#ifndef NDEBUG
#if defined(_MSC_VER)
        std::cout << __FUNCSIG__ << '\n';
#elif defined(__GNUC__)
        std::cout << __PRETTY_FUNCTION__ << '\n';
#else
        std::cout << __func__ << '\n';
#endif
        std::cout << "typeid(method): " << typeid(method_).name() << '\n';
        std::cout << "typeid(arguments): " << typeid(arguments).name() << '\n';
#endif
        std::cout << "Exception catched!" << "\n\n";
    }

    const std::weak_ptr<Object> object_;
    const Method method_;
};
