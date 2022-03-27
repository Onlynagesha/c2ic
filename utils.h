//
// Created by Onlynagesha on 2022/3/27.
//

#ifndef GRAPH_UTILS_H
#define GRAPH_UTILS_H

#include <cctype>
#include <random>
#include <string>

// Factory function for std::mt19937 pseudo-random generator
inline auto createMT19937Generator(unsigned initialSeed = 0) noexcept {
    return std::mt19937(initialSeed != 0 ? initialSeed : std::random_device()());
}

// Constexpr power function a^n
template <class I>
inline constexpr I quickPow(I a, unsigned n) {
    I res = 1;
    for (; n != 0; n >>= 1) {
        if (n & 1) {
            res *= a;
        }
        a *= a;
    }
    return res;
}

// See: https://en.cppreference.com/w/cpp/string/char_traits
struct ci_char_traits : public std::char_traits<char> {
    static char to_upper(char ch) {
        return (char) std::toupper((unsigned char) ch);
    }
    static bool eq(char c1, char c2) {
        return to_upper(c1) == to_upper(c2);
    }
    static bool lt(char c1, char c2) {
        return to_upper(c1) <  to_upper(c2);
    }
    static int compare(const char* s1, const char* s2, std::size_t n) {
        while ( n-- != 0 ) {
            if ( to_upper(*s1) < to_upper(*s2) ) return -1;
            if ( to_upper(*s1) > to_upper(*s2) ) return 1;
            ++s1; ++s2;
        }
        return 0;
    }
    static const char* find(const char* s, std::size_t n, char a) {
        auto const ua (to_upper(a));
        while ( n-- != 0 )
        {
            if (to_upper(*s) == ua)
                return s;
            s++;
        }
        return nullptr;
    }
};

// Case-insensitive string type
using CaseInsensitiveString = std::basic_string<char, ci_char_traits>;

// Transforms to std::string
inline std::string toString(const CaseInsensitiveString& str) {
    return {str.begin(), str.end()};
}


#endif //GRAPH_UTILS_H
