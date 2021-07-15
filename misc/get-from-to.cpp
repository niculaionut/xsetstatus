/* Read one line from stdin and print characters starting from the first occurance of
 * the first cli argument. Stop at the first occurance of the second cli argument. */

#include <iostream>
#include <fmt/core.h>
#include <string>
#include <string_view>

int main(int argc, char* argv[])
{
        if(argc < 3 || argc > 4)
        {
                fmt::print(stderr, "get-from-to <from> <to> [--amixer]\n");
                return 1;
        }

        std::string_view fchar = argv[1];
        std::string_view tchar = argv[2];
        if(fchar.size() != 1 || tchar.size() != 1)
        {
                fmt::print(stderr, "<from> and <to> must be single characters\n");
                return 1;
        }

        char c1 = fchar[0];
        char c2 = tchar[0];

        std::string line;
        std::getline(std::cin, line);

        auto i1 = line.find(c1);
        auto i2 = line.find(c2, i1 + 1);

        if(i1 == std::string::npos)
        {
                fmt::print(stderr, "<from>: '{}' not found in string\n", c1);
                return 1;
        }
        if(i2 == std::string::npos)
        {
                fmt::print(stderr, "<to>: '{}' not found in string\n", c2);
                return 1;
        }

        std::string_view result(line.begin() + i1 + 1, line.begin() + i2);

        if(argc == 4 && std::string_view(argv[3]) == "--amixer")
        {
                if(result.empty() || result.back() != '%')
                {
                        fmt::print(stderr, "String is not in expected 'amixer sget Master | "
                                           "tail -n1' format\n");
                        return 1;
                }

                result.remove_suffix(1);
                fmt::print("{}", result);

                if(line.find("[on]") == std::string::npos)
                {
                        fmt::print("*");
                }

                return 0;
        }

        fmt::print("{}", result);

        return 0;
}
