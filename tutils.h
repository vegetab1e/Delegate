#pragma once

#include <cstdio>
#include <cstdint>

#include <algorithm>

#include <type_traits>

#include "type_name.h"

#if __cplusplus >= 201703L
inline
#endif
#if __cplusplus >= 201103L
constexpr
#else
const
#endif
unsigned BUFFER_SIZE = 1024;

#if defined(_MSC_VER)
#define CHAR_SIZE CHAR_BIT 
#elif defined(__GNUC__)
#define CHAR_SIZE __CHAR_BIT__ 
#else
#error Failing compilation
#endif

#ifdef __GNUC__
struct ZeroMemory
{
    char array[0];
};
#endif

template<class Min, class Max, class... Types>
using CheckPackSize = std::enable_if_t<(sizeof...(Types) <= sizeof(Max) * CHAR_SIZE) &&
                                       (sizeof...(Types) >  sizeof(Min) * CHAR_SIZE),
                                       Max>;

template<class... Types>
constexpr
#ifdef __GNUC__
CheckPackSize<ZeroMemory, std::uint8_t, Types...>
#else
std::enable_if_t<sizeof...(Types) <= sizeof(std::uint8_t) * CHAR_SIZE, std::uint8_t>
#endif
getBitMaskSize() noexcept
{
    static_assert(false, "Calling getBitMaskSize() is ill-formed");
}

template<class... Types>
constexpr
CheckPackSize<std::uint8_t, std::uint16_t, Types...>
getBitMaskSize() noexcept
{
    static_assert(false, "Calling getBitMaskSize() is ill-formed");
}

template<class... Types>
constexpr
CheckPackSize<std::uint16_t, std::uint32_t, Types...>
getBitMaskSize() noexcept
{
    static_assert(false, "Calling getBitMaskSize() is ill-formed");
}

template<class... Types>
constexpr
CheckPackSize<std::uint32_t, std::uint64_t, Types...>
getBitMaskSize() = delete;

template<class... Types>
using BitMaskSize = decltype(getBitMaskSize<Types...>());

template<class T, class... Types>
using CheckBitMaskType = std::enable_if_t<std::is_integral<T>::value &&
                                          std::is_unsigned<T>::value &&
                                          (sizeof(T) * CHAR_SIZE >= sizeof...(Types)),
                                          T>;

template<class Type>
#if __cplusplus >= 201703L
inline
#endif
constexpr bool
#if __cplusplus >= 201402L
isNonConstReference = std::is_lvalue_reference<Type>::value &&
                      not std::is_const<std::remove_reference_t<Type>>::value;
#else
isNonConstReference() noexcept
{
    return (std::is_lvalue_reference<Type>::value &&
            not std::is_const<std::remove_reference_t<Type>>::value);
}
#endif

template<class T, class... Types>
constexpr
CheckBitMaskType<T, Types...>
checkReferencesType() noexcept
{
    std::size_t i = 0;
    T bit_mask = 0b0;

    using Array = T[];
    (void)Array{ T(), (
#if __cplusplus >= 201402L
        bit_mask |= (isNonConstReference<Types> << i++)
#else
        bit_mask |= (isNonConstReference<Types>() << i++)
#endif
    )... };

    return bit_mask;
}

template<class... Types>
constexpr
BitMaskSize<Types...>
checkReferencesTypeWrap() noexcept
{
    return checkReferencesType<BitMaskSize<Types...>, Types...>();
}

template<std::size_t N, class T, CheckBitMaskType<T> = T()>
constexpr void
printBits(char (&buffer)[N], T bit_mask) noexcept
{
    constexpr std::size_t bit_count = std::min(N - 1, sizeof(T) * CHAR_SIZE);
    for (std::size_t i = 0; i < bit_count; ++i)
        buffer[i] = !!(bit_mask & (1ULL << i)) + '0';

    buffer[bit_count] = '\0';
}

template<class T, CheckBitMaskType<T> = T()>
constexpr std::size_t
trueBits(T bit_mask) noexcept
{
    std::size_t bit_count = 0;
    for (std::size_t i = 0; i < sizeof(T) * CHAR_SIZE; ++i)
        bit_count += !!(bit_mask & (1ULL << i));

    return bit_count;
}

template<class, class...>
struct FunctionSignature;

template<class ReturnType, class Object, class... Types>
struct FunctionSignature<ReturnType(Object::*)(Types...)> final
{
    FunctionSignature(FunctionSignature const&) = delete;
    FunctionSignature& operator=(FunctionSignature const&) = delete;
    ~FunctionSignature() = delete;

    static void checkReferencesType()
    {
        std::printf("[INF] Checking requirements of function: %s\n",
                    toString());

#ifdef __GNUC__
        static_assert(sizeof(ZeroMemory) == 0, "sizeof(ZeroMemory) != 0");
#endif
        // Это битовая маска типов аргументов, принимаемых методом, где единицей
        // обозначены неконстантные ссылки, которые будут указывать на объекты в
        // кортеже, что может привести к неожиданному поведению или даже SIGSEGV
        constexpr decltype(auto) bit_mask = checkReferencesTypeWrap<Types...>();
        if (!!bit_mask)
        {
#ifndef NDEBUG
            std::printf("[DBG] Bit mask (non-const references): %s\n",
                        printBits(bit_mask));
#endif
            std::printf("[WRN] The function requires a non-const %s: %s\n",
                        trueBits(bit_mask) > 1 ? "references" : "reference",
                        getTypeNames(bit_mask));
        }

        std::printf("[INF] Done\n\n");
    }

    static const char* toString()
    {
        static char buffer[BUFFER_SIZE];

        std::size_t length = 0, i = 0;
        int char_count;

        using Array = decltype(length)[];
        (void)Array{
            ((char_count = std::snprintf(buffer,
                                         sizeof(buffer),
                                         "%s(",
                                         type_name<ReturnType>().c_str())
             ) > 0
             ? length += char_count
             : 0),
            ((char_count = std::snprintf(buffer + length,
                                         sizeof(buffer) - length,
                                         "%s%s",
                                         type_name<Types>().c_str(),
                                         (++i < sizeof...(Types) ? ", " : ""))
             ) > 0
             ? length += char_count
             : 0)...,
            ((char_count = std::snprintf(buffer + length,
                                         sizeof(buffer) - length,
                                         ")")
             ) > 0
             ? length += char_count
             : 0)
        };

        if (not (char_count >= 0 && length < sizeof(buffer)))
            buffer[std::min(length, sizeof(buffer) - 1)] = '\0';

        return buffer;
    }

private:
    template<class T, CheckBitMaskType<T, Types...> = T()>
    static const char*
    getTypeNames(T bit_mask)
    {
        static char buffer[BUFFER_SIZE];

        std::size_t length = 0, i = 0;
        int char_count = -1;

        // Именно это делает тернарный оператор для возвращаемого значения
        using Array = std::common_type_t<decltype(length), decltype(0)>[];
        (void)Array{ 0, (
            bit_mask & (1ULL << i++)
            ? ((char_count = std::snprintf(buffer + length,
                                           sizeof(buffer) - length,
                                           "%s%s",
                                           length ? ", " : "",
                                           type_name<Types>().c_str())
               ) > 0
               ? length += char_count
               : 0)
            : 0
        )... };

        if (not (char_count >= 0 && length < sizeof(buffer)))
            buffer[std::min(length, sizeof(buffer) - 1)] = '\0';

        return buffer;
    }

    template<class T, CheckBitMaskType<T, Types...> = T()>
    static const char*
    printBits(T bit_mask)
    {
        static char buffer[sizeof...(Types) + 1];

        ::printBits(buffer, bit_mask);

        return buffer;
    }
};

#undef CHAR_SIZE
