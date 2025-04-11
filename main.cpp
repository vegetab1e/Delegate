#include <cstddef>

#include <iostream>

#include <memory>

#include "delegate.h"
#include "type_name.h"

struct Printer
{
    template<class... Types>
    void print(Types&&... args)
    {
        using std::cout;
#ifndef NDEBUG
#if defined(_MSC_VER)
        cout << __FUNCSIG__ << '\n';
#elif defined(__GNUC__)
        cout << __PRETTY_FUNCTION__ << '\n';
#else
        cout << __func__ << '\n';
#endif
#endif
        std::size_t i = 0;
        constexpr std::size_t pack_size = sizeof...(Types);
        if (!pack_size)
            return;

        cout << "decltype(args):\t";
#if __cplusplus < 201703L
        using Array = int[];
        (void)Array{
            (cout << type_name<decltype(args)>()
                  << '(' << args << (++i < pack_size ? "), " : ")\n"), 0)...
        };
#else
        (void)(
            (cout << type_name<decltype(args)>()
                  << '(' << args << (++i < pack_size ? "), " : ")\n")), ...
        );
#endif
        cout << '\n';
    }

    void nonTemplatePrint(int&& arg1, long double arg2, long double arg3, void*& arg4)
    {
        using std::cout;
#ifndef NDEBUG
#if defined(_MSC_VER)
        cout << __FUNCSIG__ << '\n';
#elif defined(__GNUC__)
        cout << __PRETTY_FUNCTION__ << '\n';
#else
        cout << __func__ << '\n';
#endif
#endif
        cout << "decltype(args):\t";
        cout << type_name<decltype(arg1)>() << '(' << arg1 << "), ";
        cout << type_name<decltype(arg2)>() << '(' << arg2 << "), ";
        cout << type_name<decltype(arg3)>() << '(' << arg3 << "), ";
        cout << type_name<decltype(arg4)>() << '(' << arg4 << ")\n\n";
    }
};

int main(int argc, char* argv[])
{
    using std::cout;

    const std::shared_ptr<Printer> printer{new Printer};
    Delegate delegate;

    // Пример #1
    int val1 = 5;
    int const& val2 = val1;

    cout << "EXAMPLE #1\n";
    delegate.connect(printer,
        static_cast<void(Printer::*)(int&&, int&&, float&&, int const&, int&, int const&)>(&Printer::print)
    );
#ifdef PERFECT_FORWARDING
    // Специально для кода под макросом PERFECT_FORWARDING, работает при инстанцировании
    // шаблона аргументов типами с двумя внешними по отношению к этим типам амперсандами
    delegate(1, 2, 3.14159F, static_cast<int const&>(4), val1, val2);
#else
    delegate(1, 2, 3.14159F, 4, val1, val2);
#endif

    // Пример #2
    const char* val3 = "cstring";

    cout << "EXAMPLE #2\n";
    delegate.connect(printer,
        static_cast<void(Printer::*)(float const&&, char const*&, bool&&, long double&&)>(&Printer::print)
    );
#ifdef PERFECT_FORWARDING
    // Специально для кода под макросом PERFECT_FORWARDING, работает при инстанцировании
    // шаблона аргументов типами с двумя внешними по отношению к этим типам амперсандами
    delegate(static_cast<float const&&>(- 1.0F), val3, false, 2.718281828459045L);
#else
    delegate(-1.0F, val3, false, 2.718281828459045L);
#endif

    // Пример #3
    long double val4 = 6.62606957L;
    void* val5 = static_cast<void*>(printer.get());

    cout << "EXAMPLE #3\n";
    delegate.connect(printer,
        &Printer::nonTemplatePrint
    );
    delegate(1, 2.718281828459045L, val4, val5);

    return 0;
}
