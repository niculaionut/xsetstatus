#include <fmt/core.h>
#include <fmt/compile.h>
#include <string>
#include <array>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>

// stl/fmt wrappers

auto find_if(const auto& container, const auto pred)
{
        return std::find_if(std::begin(container), std::end(container), pred);
}

static __always_inline void printerr(auto&&... args)
{
        fmt::print(stderr, args...);
}

// config variables and root strings array
//
constexpr int N_FIELDS = 8;
constexpr int FIELD_MAX_LENGTH = 22;

using str_t = volatile char[FIELD_MAX_LENGTH];
using arr_t = std::array<str_t, N_FIELDS>;

static arr_t rootstrings = {};
static volatile bool running = true;
static const int SIGOFFSET = SIGRTMAX;
static constexpr auto fmt_format_buf = []()
{
        constexpr std::string_view markers[] = {"[{}", " |{}", " |{}]"};

        std::array<char, 1 +
                         markers[0].size() +
                         (N_FIELDS - 2) *
                         markers[1].size() +
                         markers[2].size()> res;
        char* resptr = res.data();

        std::copy(markers[0].begin(), markers[0].end(), resptr);
        resptr += markers[0].size();

        for(int i = 0; i < N_FIELDS - 2; ++i)
        {
                std::copy(markers[1].begin(), markers[1].end(), resptr);
                resptr += markers[1].size();
        }

        std::copy(markers[2].begin(), markers[2].end(), resptr);
        resptr += markers[2].size();
        *resptr = '\0';

        return res;
}();
static constexpr std::string_view fmt_format_sv(fmt_format_buf.data());
static Display* dpy = nullptr;
static int screen;
static Window root;

// xsetroot utils

std::string get_root_string()
{
        return std::apply(
            [&](auto&&... args)
            {
                    return fmt::format(fmt_format_sv, const_cast<char*>(args)...);
            },
            rootstrings);
}

void set_root()
{
        const auto cstatus = get_root_string();
        XStoreName(dpy, root, cstatus.data());
        XFlush(dpy);
}

void xss_exit(const int rc)
{
        XCloseDisplay(dpy);
        std::exit(rc);
}

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
                xss_exit(EXIT_FAILURE);
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
                        result.pop_back();
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
        template<bool>
        void resolve() const;

public:
        const char* command;
        const int pos;
};

struct BuiltinResponse
{
public:
        template<bool>
        void resolve() const;

public:
        const char* description;
        std::string (*fptr)();
        const int pos;
};

template<bool SET_IMMEDIATELY>
void ShellResponse::resolve() const
{
        if(pos < 0 || pos >= N_FIELDS)
        {
                xss_exit(EXIT_FAILURE);
        }

        const auto cmdres = exec_cmd<true>(command);

        if(cmdres.rc != EXIT_SUCCESS ||
           cmdres.output.size() >= FIELD_MAX_LENGTH)
        {
                xss_exit(EXIT_FAILURE);
        }

        std::strcpy(const_cast<char*>(rootstrings[pos]), cmdres.output.data());

        if constexpr(SET_IMMEDIATELY)
        {
                set_root();
        }
}

template<bool SET_IMMEDIATELY>
void BuiltinResponse::resolve() const
{
        if(pos < 0 || pos >= N_FIELDS)
        {
                xss_exit(EXIT_FAILURE);
        }

        const std::string returnstr = fptr();

        if(returnstr.size() >= FIELD_MAX_LENGTH)
        {
                xss_exit(EXIT_FAILURE);
        }

        std::strcpy(const_cast<char*>(rootstrings[pos]), returnstr.data());

        if constexpr(SET_IMMEDIATELY)
        {
                set_root();
        }
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
/*        shell command/script   index in root array */
        { "xss-get-time",        R_TIME },
        { "xss-get-load",        R_LOAD },
        { "xss-get-temp",        R_TEMP },
        { "xss-get-vol",         R_VOL  },
        { "xss-get-mic",         R_MIC  },
        { "xss-get-mem",         R_MEM  },
        { "xss-get-date",        R_DATE },
};

constexpr BuiltinResponse brtable[] = {
/*        description         pointer to function (handler)   root array index */
        { "toggle kb lang",   toggle_lang,                    R_LANG }
};

constexpr ShellResponse interval_responses[] = {
        rtable[0],
        rtable[1],
        rtable[2],
        rtable[5],
};

const std::pair<int, ShellResponse> sig_shell_responses[] = {
/*        signal value    ShellResponse instance*/
        { SIGOFFSET - 1,  rtable[3] },
        { SIGOFFSET - 2,  rtable[4] }
};

const std::pair<int, BuiltinResponse> sig_builtin_responses[] = {
/*        signal value    BuiltinResponse instance*/
        { SIGOFFSET - 3,  brtable[0] }
};

void init_statusbar()
{
        for(const auto& r : rtable)
        {
                r.resolve<false>();
        }

        for(const auto& r : brtable)
        {
                r.resolve<false>();
        }

        set_root();
}

void run_interval_responses(const int)
{
        for(const auto& r : interval_responses)
        {
                r.resolve<false>();
        }

        set_root();
}

void shell_handler(const int sig)
{
        const auto& r = find_if(sig_shell_responses,
                                [&](const auto& r)
                                {
                                        return r.first == sig;
                                });

        r->second.resolve<true>();
}

void builtin_handler(const int sig)
{
        const auto& r = find_if(sig_builtin_responses,
                                [&](const auto& r)
                                {
                                        return r.first == sig;
                                });

        r->second.resolve<true>();
}

void terminator(const int)
{
        running = false;
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

        signal(SIGOFFSET - 4, run_interval_responses);
        signal(SIGTERM, terminator);
        signal(SIGINT, terminator);
}

void setup_x()
{
        dpy = XOpenDisplay(nullptr);
        if(!dpy)
        {
                printerr("xsetstatus: Failed to open display\n");
                std::exit(EXIT_FAILURE);
        }
        screen = DefaultScreen(dpy);
        root = RootWindow(dpy, screen);
}

void interval_loop()
{
        while(running)
        {
                pause();
        }
}

bool already_running()
{
        const auto cmdres = exec_cmd<true>("pgrep -x xsetstatus | wc -l");
        return cmdres.output != "1";
}

int main()
{
        if(already_running())
        {
                printerr("xsetstatus: Another instance is already running. "
                         "Exiting with code {}.\n", EXIT_SUCCESS);
                return EXIT_SUCCESS;
        }

        setup_x();
        init_statusbar();
        init_signals();
        interval_loop();

        XCloseDisplay(dpy);
}
