/* Read one line from stdin and print characters starting from the first occurance of
 * the first cli argument. Stop at the first occurance of the second cli argument. */

#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char* argv[])
{
        if(argc < 3 || argc > 4)
        {
                std::cerr << "get-from-to <from-char> <to-char> [--amixer]\n";
                return 1;
        }

        std::string_view fchar = argv[1];
        std::string_view tchar = argv[2];
        if(fchar.size() != 1 || tchar.size() != 1)
        {
                std::cerr << "<from> and <to> must be single characters\n";
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
                std::cerr << "first argument '" << c1 << "' not found in string\n";
                return 1;
        }
        if(i2 == std::string::npos)
        {
                std::cerr << "second argument '" << c2 << "' not found in string\n";
                return 1;
        }

        std::string_view result(line.begin() + i1 + 1, line.begin() + i2);

        if(argc == 4 && std::string_view(argv[3]) == "--amixer")
        {
                if(result.empty() || result.back() != '%')
                {
                        std::cerr << "Input not in expected 'amixer sget Master | tail -n1' "
                                     "format\n";
                        return 1;
                }
                result.remove_suffix(1);
                std::cout << result;

                if(line.find("[on]") == std::string::npos)
                {
                        std::cout << '*';
                }

                return 0;
        }

        std::cout << result;

        return 0;
}
