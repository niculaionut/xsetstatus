#pragma once
#include <cstdint>
#include <string_view>
#include <array>

template<typename DestIt, typename SrcIt>
static constexpr auto fcpy(DestIt dest, SrcIt src)
{
        while(*src != '\0')
        {
                *dest++ = *src++;
        }
        *dest = '\0';
        return dest;
}

template<typename DestIt, typename SrcIt>
static constexpr auto fncpy(DestIt dest, SrcIt src, std::size_t count)
{
        while(count-- != 0)
        {
                *dest++ = *src++;
        }
        *dest = '\0';
        return dest;
}

template<std::size_t N>
class FixedStr
{
public:
        constexpr friend FixedStr operator+(const FixedStr& s1, const FixedStr& s2)
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

        constexpr FixedStr(const char* cstr)
        {
                csize = fcpy(data(), cstr) - data();
        }

        constexpr FixedStr& operator+=(const FixedStr& other)
        {
                csize += fcpy(data() + csize, other.data()) - (data() + csize);
                return *this;
        }

        constexpr FixedStr& operator+=(const char* cstr)
        {
                csize += fcpy(data() + csize, cstr) - (data() + csize);
                return *this;
        }

        constexpr FixedStr& operator+=(const std::string_view sv)
        {
                fncpy(data() + csize, sv.data(), sv.size());
                csize += sv.size();
                return *this;
        }

        constexpr bool operator==(const FixedStr& other) const
        {
                if(csize != other.csize)
                {
                        return false;
                }

                const auto lim = end();
                auto it1 = begin(), it2 = other.begin();
                while(it1 != lim)
                {
                        if(*it1++ != *it2++)
                        {
                                return false;
                        }
                }
                return true;
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
                const char* ptr = data();
                csize = 0;
                while(*ptr++ != '\0')
                {
                        ++csize;
                }
        }

private:
        std::array<char, N + 1> elements = {};
        std::size_t csize = 0;
};
