#include <fmt/core.h>
#include <fmt/compile.h>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>

// stl wrappers

auto find_if(const auto& container, const auto pred)
{
        return std::find_if(std::begin(container), std::end(container), pred);
}

// config variables and root strings array

constexpr int N_FIELDS = 8;
constexpr int FIELD_MAX_LENGTH = 40;
constexpr int DEFAULT_SLEEP_TIME = 10;
char rootstrings[N_FIELDS][FIELD_MAX_LENGTH] = {};
const std::string fmt_format_str = []()
{
        std::string f = "[{}";

        for(int i = 0; i < N_FIELDS - 2; ++i)
        {
                f += " |{}";
        }

        f += " |{}]";

        return f;
}();

// utilities for executing shell commands

struct CmdResult
{
        const std::string output;
        const int rc;
};

template<bool OMIT_NEWLINE>
CmdResult exec_cmd(const char* cmd)
{
        std::array<char, 128> buffer;
        std::string result;

        FILE* pipe = popen(cmd, "r");

        if(!pipe)
        {
                fmt::print(stderr, "popen() failed!");
                std::exit(EXIT_FAILURE);
        }

        while(fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        {
                result += buffer.data();
        }

        const int rc = pclose(pipe);

        if constexpr(OMIT_NEWLINE)
        {
                if(!result.empty() && result.back() == '\n')
                {
                        result.resize(result.size() - 1);
                }
        }

        return {std::move(result), rc};
}

// functions for builtin responses

std::string toggle_lang()
{
        static constexpr const char* ltable[2] = {
            "EN",
            "RO"
        };

        static constexpr const char* commands[2] = {
            "setxkbmap us; setxkbmap -option numpad:mac",
            "setxkbmap ro -variant std"
        };

        static bool flag = true;

        flag = !flag;

        std::system(commands[flag]);

        return std::string(ltable[flag]);
}

// Responses/handlers for signals and interval-updated fields

struct ShellResponse
{
public:
        void resolve() const;

public:
        const char* command;
        const int pos;
};

struct BuiltinResponse
{
public:
        void resolve() const;

public:
        const char* description;
        std::string (*fptr)();
        const int pos;
};

void ShellResponse::resolve() const
{
        if(pos < 0 || pos >= N_FIELDS)
        {
                fmt::print(stderr,
                           "Erorr: value of Response.pos needs to be between 0 and {}. Value "
                           "passed to function: {}\n",
                           N_FIELDS - 1, pos);

                std::exit(EXIT_FAILURE);
        }

        const auto cmdres = exec_cmd<true>(command);

        if(cmdres.rc != EXIT_SUCCESS)
        {
                fmt::print(stderr, "failure: command '{}' exited with return code {}\n",
                           command, cmdres.rc);

                std::exit(EXIT_FAILURE);
        }

        if(cmdres.output.size() >= FIELD_MAX_LENGTH)
        {
                fmt::print(stderr,
                           "failure: copying output of command '{}' would overflow "
                           "root buffer at index {}\n",
                           command, pos);

                std::exit(EXIT_FAILURE);
        }

        std::strcpy(rootstrings[pos], cmdres.output.data());
}

void BuiltinResponse::resolve() const
{
        if(pos < 0 || pos >= N_FIELDS)
        {
                fmt::print(stderr,
                           "Erorr: value of Response.pos needs to be between 0 and {}. Value "
                           "passed to function: {}\n",
                           N_FIELDS - 1, pos);

                std::exit(EXIT_FAILURE);
        }

        const std::string returnstr = fptr();

        if(returnstr.size() >= FIELD_MAX_LENGTH)
        {
                fmt::print(stderr,
                           "failure: copying return string of builtin '{}' would overflow "
                           "root buffer at index {}\n",
                           description, pos);

                std::exit(EXIT_FAILURE);
        }

        std::strcpy(rootstrings[pos], returnstr.data());
}

enum ResponseRootIdx
{
        R_TIME = 0,
        R_LOAD,
        R_TEMP,
        R_VOL,
        R_MIC,
        R_MEM,
        R_LANG,
        R_DATE
};

constexpr ShellResponse rtable[] = {
/*        shell script/command name   index in root array */
        { "xss-get-time",             R_TIME },
        { "xss-get-load",             R_LOAD },
        { "xss-get-temp",             R_TEMP },
        { "xss-get-vol",              R_VOL  },
        { "xss-get-mic",              R_MIC  },
        { "xss-get-mem",              R_MEM  },
        { "xss-get-date",             R_DATE },
};

constexpr BuiltinResponse brtable[] = {
/*        shell script/command name   pointer to function (handler)   root array index */
        { "toggle kb lang",           toggle_lang,                    R_LANG }
};

constexpr ShellResponse interval_responses[] = {
        rtable[0],
        rtable[1],
        rtable[3],
        rtable[5],
};

const std::pair<int, ShellResponse> sig_shell_responses[] = {
/*        signal value   ShellResponse instance*/
        { SIGRTMAX - 1,  rtable[3] },
        { SIGRTMAX - 2,  rtable[4] }
};

const std::pair<int, BuiltinResponse> sig_builtin_responses[] = {
/*        signal value   BuiltinResponse instance*/
        { SIGRTMAX - 3,  brtable[0] }
};

void run_at_startup()
{
        for(const auto& r : rtable)
        {
                r.resolve();
        }

        for(const auto& r : brtable)
        {
                r.resolve();
        }
}

void run_interval_responses()
{
        for(const auto& r : interval_responses)
        {
                r.resolve();
        }
}

void shell_handler(const int sig)
{
        const auto& r = find_if(sig_shell_responses,
                                [&](const auto& r)
                                {
                                        return r.first == sig;
                                });
        r->second.resolve();
}

void builtin_handler(const int sig)
{
        const auto& r = find_if(sig_builtin_responses,
                                [&](const auto& r)
                                {
                                        return r.first == sig;
                                });
        r->second.resolve();
}

void init_signals()
{
        for(const auto& [sig, r] : sig_shell_responses)
        {
                signal(sig, shell_handler);
        }

        for(const auto& [sig, r] : sig_builtin_responses)
        {
                signal(sig, builtin_handler);
        }
}

std::string get_root_string()
{
        int i = 0;

        std::string f = fmt::format(FMT_COMPILE("[{}"), rootstrings[i++]);

        for(int tmp = 0; tmp < N_FIELDS - 2; ++tmp)
        {
                f += fmt::format(FMT_COMPILE(" |{}"), rootstrings[i++]);
        }

        f += fmt::format(FMT_COMPILE(" |{}]"), rootstrings[i]);

        return f;
}

void set_root()
{
        std::system(fmt::format(FMT_COMPILE("xsetroot -name '{}'"), get_root_string()).data());
}

void update_loop()
{
        while(true)
        {
                run_interval_responses();
                set_root();
                sleep(DEFAULT_SLEEP_TIME);
        }
}

int main()
{
        run_at_startup();
        init_signals();
        update_loop();
}
