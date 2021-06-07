#include "fixedstr.h"
#include <fmt/core.h>
#include <fmt/format.h>
#include <array>
#include <utility>
#include <unistd.h>
#include <signal.h>
#ifndef NO_X11
#include <X11/Xlib.h>
#endif

// stl/fmt wrappers

static auto find_if(const auto& container, const auto pred)
{
        return std::find_if(std::begin(container), std::end(container), pred);
}

// config variables and root strings array

static constexpr int N_FIELDS = 8;
static constexpr int FIELD_MAX_LENGTH = 22;
static constexpr int ROOT_BUFSIZE = N_FIELDS * FIELD_MAX_LENGTH;
static constexpr auto fmt_format_buf = []()
{
        constexpr std::string_view markers[] = {"[{}", " |{}", " |{}]"};

        fixed_str<markers[0].size() +
                  (N_FIELDS - 2) *
                  markers[1].size() +
                  markers[2].size()> res;

        res += markers[0];
        for(int i = 0; i < N_FIELDS - 2; ++i)
        {
                res += markers[1];
        }
        res += markers[2];

        return res;
}();
static constexpr std::string_view fmt_format_sv(fmt_format_buf.data());

using field_buffer = fixed_str<FIELD_MAX_LENGTH>;
using root_str_buffer = fixed_str<ROOT_BUFSIZE>;

static std::array<char[FIELD_MAX_LENGTH], N_FIELDS> rootstrings = {};
static volatile sig_atomic_t last_sig = -1;
static volatile sig_atomic_t running = 1;
static const int SIGOFFSET = SIGRTMAX;
#ifndef NO_X11
static Display* dpy = nullptr;
static int screen;
static Window root;
#endif

// xsetroot utils

static root_str_buffer get_root_string()
{
        root_str_buffer buf;

        *std::apply(
            [&](auto&&... args)
            {
                    return fmt::format_to(buf.data(), fmt_format_sv, args...);
            },
            rootstrings) = '\0';

        return buf;
}

static void set_root()
{
        const auto cstatus = get_root_string();

#ifndef NO_X11
        XStoreName(dpy, root, cstatus.data());
        XFlush(dpy);
#else
        fmt::print("{}\n", cstatus.data());
#endif
}

static void xss_exit(const int rc)
{
#ifndef NO_X11
        XCloseDisplay(dpy);
#endif
        std::exit(rc);
}

// utilities for executing shell commands

struct CmdResult
{
        const field_buffer output;
        const int rc;
};

template<bool OMIT_NEWLINE>
static CmdResult exec_cmd(const char* cmd)
{
        field_buffer buf;

        FILE* pipe = popen(cmd, "r");
        if(!pipe)
        {
                xss_exit(EXIT_FAILURE);
        }

        fgets(buf.data(), std::size(buf), pipe);
        buf.set_size();
        const int rc = pclose(pipe);

        if constexpr(OMIT_NEWLINE)
        {
                if(!buf.empty() && buf.back() == '\n')
                {
                        buf.pop_back();
                }
        }

        return {buf, rc};
}

// functions for builtin responses

static field_buffer toggle_lang()
{
        static constexpr field_buffer ltable[2] = {
            {"US"},
            {"RO"}
        };
        static constexpr const char* commands[2] = {
            "setxkbmap us; setxkbmap -option numpad:mac",
            "setxkbmap ro -variant std"
        };
        static bool flag = true;

        flag = !flag;
        std::system(commands[flag]);
        return ltable[flag];
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
        field_buffer (*fptr)();
        const int pos;
};

void ShellResponse::resolve() const
{
        if(pos < 0 || pos >= N_FIELDS)
        {
                xss_exit(EXIT_FAILURE);
        }

        const auto cmdres = exec_cmd<true>(command);

        if(cmdres.rc != EXIT_SUCCESS)
        {
                xss_exit(EXIT_FAILURE);
        }

        std::strcpy(rootstrings[pos], cmdres.output.data());
}

void BuiltinResponse::resolve() const
{
        if(pos < 0 || pos >= N_FIELDS)
        {
                xss_exit(EXIT_FAILURE);
        }

        const field_buffer returnstr = fptr();
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

static constexpr ShellResponse rtable[] = {
/*        shell command/script   index in root array */
        { "xss-get-time",        R_TIME },
        { "xss-get-load",        R_LOAD },
        { "xss-get-temp",        R_TEMP },
        { "xss-get-vol",         R_VOL  },
        { "xss-get-mic",         R_MIC  },
        { "xss-get-mem",         R_MEM  },
        { "xss-get-date",        R_DATE },
};

static constexpr BuiltinResponse brtable[] = {
/*        pointer to function (handler)   root array index */
        { toggle_lang,                    R_LANG }
};

static constexpr ShellResponse interval_responses[] = {
        rtable[0],
        rtable[1],
        rtable[2],
        rtable[5],
};

static const std::pair<int, ShellResponse> sig_shell_responses[] = {
/*        signal value    ShellResponse instance*/
        { SIGOFFSET - 1,  rtable[3] },
        { SIGOFFSET - 2,  rtable[4] }
};

static const std::pair<int, BuiltinResponse> sig_builtin_responses[] = {
/*        signal value    BuiltinResponse instance*/
        { SIGOFFSET - 3,  brtable[0] }
};

static void run_interval_responses()
{
        for(const auto& r : interval_responses)
        {
                r.resolve();
        }
}

static const std::pair<int, void (*)()> sig_group_responses[] = {
        { SIGOFFSET - 4,  run_interval_responses}
};

static void setup()
{
#ifndef NO_X11
        dpy = XOpenDisplay(nullptr);
        if(!dpy)
        {
                fmt::print(stderr, "xsetstatus: Failed to open display\n");
                std::exit(EXIT_FAILURE);
        }
        screen = DefaultScreen(dpy);
        root = RootWindow(dpy, screen);
#endif
}

static void handle_sig(const int sig)
{
        const auto pred = [&](const auto& r)
        {
                return r.first == sig;
        };

        const auto it_shell = find_if(sig_shell_responses, pred);
        if(it_shell != std::cend(sig_shell_responses))
        {
                it_shell->second.resolve();
                return;
        }

        const auto it_builtin = find_if(sig_builtin_responses, pred);
        if(it_builtin != std::cend(sig_builtin_responses))
        {
                it_builtin->second.resolve();
                return;
        }

        const auto it_group = find_if(sig_group_responses, pred);
        it_group->second();
}

static void solve_signals()
{
        while(running)
        {
                if(last_sig != -1)
                {
                        handle_sig(last_sig);
                }
                set_root();
                pause();
        }
}

static bool already_running()
{
#ifndef MULTIPLE_INSTANCES
        const auto cmdres1 = exec_cmd<true>("pgrep -x xsetstatus | wc -l");
        if(cmdres1.output == "0")
        {
                return false;
        }
        if(cmdres1.output != "1")
        {
                return true;
        }
        const auto cmdres2 = exec_cmd<true>("pgrep -x xsetstatus");
        return std::atoi(cmdres2.output.data()) != getpid();
#else
        return false;
#endif
}

static void u_sig_handler(const int sig)
{
        last_sig = sig;
}

static void terminator(const int)
{
        running = 0;
}

static void init_signals()
{
        for(const auto& r : sig_shell_responses)
        {
                signal(r.first, u_sig_handler);
        }

        for(const auto& r : sig_builtin_responses)
        {
                signal(r.first, u_sig_handler);
        }

        for(const auto& r : sig_group_responses)
        {
                signal(r.first, u_sig_handler);
        }

        signal(SIGTERM, terminator);
        signal(SIGINT, terminator);
}

static void init_statusbar()
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

int main()
{
        if(already_running())
        {
                fmt::print(stderr,
                           "xsetstatus: Another instance is already running. "
                           "Exiting with code {}.\n",
                           EXIT_SUCCESS);
                return EXIT_SUCCESS;
        }

        setup();
        init_statusbar();
        init_signals();
        solve_signals();

#ifndef NO_X11
        XCloseDisplay(dpy);
#endif

        return EXIT_SUCCESS;
}
