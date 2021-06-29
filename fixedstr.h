#pragma once
#include <string_view>
#include <array>
#include <algorithm>

template<std::size_t N>
class FixedStr
{
public:
        friend constexpr FixedStr operator+(const FixedStr& s1, const FixedStr& s2)
        {
                FixedStr res = s1;
                res += s2;
                return res;
        }

public:
        constexpr FixedStr()
        {
                elements[0] = '\0';
        }

        constexpr FixedStr(const FixedStr& other)
        {
                std::copy_n(other.begin(), other.csize + 1, begin());
                csize = other.csize;
        }

        constexpr FixedStr(const std::string_view sv)
        {
                std::copy_n(sv.data(), sv.size() + 1, begin());
                csize = sv.size();
        }

        constexpr FixedStr(const char* cstr)
            : FixedStr(std::string_view(cstr))
        {
        }

        constexpr operator std::string_view() const
        {
                return std::string_view(elements.data(), csize);
        }

        constexpr FixedStr& operator=(const FixedStr& other)
        {
                std::copy_n(other.begin(), other.csize + 1, begin());
                csize = other.csize;
                return *this;
        }

        constexpr FixedStr& operator=(const std::string_view sv)
        {
                std::copy_n(sv.data(), sv.size() + 1, begin());
                csize = sv.size();
                return *this;
        }

        constexpr FixedStr& operator+=(const FixedStr& other)
        {
                std::copy_n(other.begin(), other.csize + 1, end());
                csize += other.csize;
                return *this;
        }

        constexpr FixedStr& operator+=(const std::string_view sv)
        {
                std::copy_n(sv.data(), sv.size() + 1, end());
                csize += sv.size();
                return *this;
        }

        constexpr bool operator==(const FixedStr& other) const
        {
                return csize == other.csize &&
                       std::char_traits<char>::compare(begin(), other.begin(), csize) == 0;
        }

        constexpr char& operator[](const std::size_t pos)
        {
                return elements[pos];
        }

        constexpr char operator[](const std::size_t pos) const
        {
                return elements[pos];
        }

        constexpr void push_back(const char c)
        {
                elements[csize++] = c;
                elements[csize] = '\0';
        }

        constexpr void pop_back()
        {
                elements[--csize] = '\0';
        }

        constexpr auto data()
        {
                return elements.data();
        }

        constexpr auto data() const
        {
                return elements.data();
        }

        constexpr auto c_str() const
        {
                return elements.data();
        }

        constexpr auto capacity() const
        {
                return N;
        }

        constexpr auto size() const
        {
                return N;
        }

        constexpr auto length() const
        {
                return csize;
        }

        constexpr auto begin()
        {
                return data();
        }

        constexpr auto end()
        {
                return data() + csize;
        }

        constexpr auto begin() const
        {
                return data();
        }

        constexpr auto end() const
        {
                return data() + csize;
        }

        constexpr bool empty() const
        {
                return csize == 0;
        }

        constexpr char& front()
        {
                return elements[0];
        }

        constexpr char& back()
        {
                return elements[csize - 1];
        }

        constexpr char front() const
        {
                return elements[0];
        }

        constexpr char back() const
        {
                return elements[csize - 1];
        }

        constexpr void set_size()
        {
                csize = std::char_traits<char>::length(begin());
        }

private:
        std::array<char, N + 1> elements = {};
        std::size_t csize = 0;
};
