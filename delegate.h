#pragma once

#include <cstddef>

#include <iostream>
#include <iomanip>

#include <memory>

#include <tuple>

#include <limits>
#include <type_traits>

#include "ifaces.h"
#include "containers.h"
#include "type_name.h"

class Delegate final
{
public:
    /**
     * @brief Связать делегат с объектом и его методом
     * @details Экземпляр класса Callback хранится как уникальный указатель. Он
     * хранит внутри себя слабый указатель на объект и простой указатель на его
     * метод. Класс шаблонный, создаётся он под каждый конкретный метод каждого
     * конкретного класса, экзмепляр которого передаётся как общий указатель, а
     * хранится как слабый. Есть проверка на нулевые указатели, которая сбросит
     * уникальный указатель на контейнер, хранящий указатели на объект и метод!
     * @tparam Object Тип объекта
     * @tparam Method Тип указателя на метод (по сути сигнатура метода)
     * @param[in] object Общий указатель на объект
     * @param[in] method Простой указатель на метод объекта
     */
    template<class Object, class Method>
    void connect(const std::shared_ptr<Object>& object, Method method);

    /**
     * @brief Вызвать метод объекта с переданными аргументами
     * @details Без макроса PERFECT_FORWARDING все значения будут скопированы в
     * кортеж, с использованием идеальной передачи, но всё же способ безнадёжно
     * неэффективный. Помимо того, такой способ может привести и к неожиданному
     * поведению: метод принимающий изменяемую ссылку будет изменять значение в
     * кортеже! Если эта ссылка будет сохранена, то при попытке её последующего
     * использования вполне вероятно аварийное завершение работы программы, так
     * как время жизни объекта, на который она указывает, закончится при выходе
     * из метода invokeMethod() класса Callback вместе с последней его строкой!
     * @tparam Types Типы аргументов (выводятся по особым правилам)
     * @param[in] arguments Универсальные ссылки на аргументы
     */
    template<class... Types>
    void operator()(Types&&... arguments) const;

private:
    std::unique_ptr<ICallback> callback_;
};


template<class Object, class Method>
inline void Delegate::connect(const std::shared_ptr<Object>& object, Method method)
{
    if (!object || !method)
    {
        callback_.reset(nullptr);
        return;
    }

    callback_ = std::make_unique<Callback<Object, Method>>(object, method);
}

template<class... Types>
inline void Delegate::operator()(Types&&... arguments) const
{
    ICallback const*const raw_pointer = callback_.get();
    if (!raw_pointer)
        return;

#ifndef NDEBUG
#if defined(_MSC_VER)
    std::cout << __FUNCSIG__ << '\n';
#elif defined(__GNUC__)
    std::cout << __PRETTY_FUNCTION__ << '\n';
#else
    std::cout << __func__ << '\n';
#endif
    if (!!sizeof...(Types))
    {
        using namespace std;

        const auto precision{cout.precision()};

        size_t i;
        using Array = decltype(i)[];
        const Array column_sizes = { 0, type_name<decltype(arguments)>().size()... };
        auto info = [&i, &column_sizes, pack_size = sizeof...(Types)](const string& type_name, const auto& value){
            using Type = decay_t<decltype(value)>;
#if __cplusplus < 201703L
            if (is_floating_point<Type>::value)
#else
            if constexpr(is_floating_point<Type>::value)
#endif
                cout << fixed << setprecision(numeric_limits<Type>::max_digits10);
            cout << setw(column_sizes[i]) << type_name << '(' << value << (i < pack_size ? "), " : ")\n");
        };

        (void)Array{ (cout << "Types:\t\t",        i = 1), (info(type_name<Types>(),                               arguments), ++i)... };
        (void)Array{ (cout << "decay_t<Types>:\t", i = 1), (info(type_name<decay_t<Types>>(),                      arguments), ++i)... };
        (void)Array{ (cout << "decltype(args):\t", i = 1), (info(type_name<decltype(arguments)>(),                 arguments), ++i)... };
        (void)Array{ (cout << "forward<Types>:\t", i = 1), (info(type_name<decltype(forward<Types>(arguments))>(), arguments), ++i)... };

        cout << defaultfloat << setprecision(precision) << endl;
    }
#endif

    raw_pointer->invokeMethod(
#ifdef PERFECT_FORWARDING
        // Добавление двух внешних амперсандов (rvalue reference) нужно для того, чтобы помочь коду
        // при динамическом приведении типов, то есть увеличить вероятность успешного сопоставления
        // типов переданного шаблона аргументов с требуемыми вызываемой функцией. На самом деле эти
        // амперсанды уже есть там где это необходимо в кортеже и добавление внешних не повлияет на
        // типы переменных, так как при сворачивании/сжатии ссылок (reference collapsing) они будут
        // просто отброшены: кортеж содержит только rvalue- и lvalue-ссылки, тогда при сворачивании
        // && + && -> && и && + & -> &. Это не решение задачи, это демонстрация самого оптимального
        // способа передачи аргументов в функцию при её вызове "прямо здесь и сейчас", к сожалению,
        // нереализуемого в рамках поставленных условий, так как вариантов может быть множество, но
        // узнать правильный будет невозможно после передачи шаблона через нешаблонный интерфейс...
        Arguments<Types&&...>{std::forward<Types>(arguments)...}
#else
        Arguments<std::decay_t<Types>...>{std::make_tuple(std::forward<Types>(arguments)...)}
#endif
    );
}