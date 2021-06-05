#include <fmt/core.h>
#include <fmt/compile.h>
#include <string>
#include <array>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>

// stl/fmt wrappers

static auto find_if(const auto& container, const auto pred)
{
        return std::find_if(std::begin(container), std::end(container), pred);
}

static void printerr(auto&&... args)
{
        fmt::print(stderr, args...);
}

// config variables and root strings array

static constexpr int N_FIELDS = 8;
static constexpr int FIELD_MAX_LENGTH = 22;
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

static std::array<char[FIELD_MAX_LENGTH], N_FIELDS> rootstrings = {};
static volatile sig_atomic_t last_sig = -1;
static volatile sig_atomic_t running = 1;
static const int SIGOFFSET = SIGRTMAX;
static Display* dpy = nullptr;
static int screen;
static Window root;

// xsetroot utils

static std::string get_root_string()
{
        return std::apply(
            [&](auto&&... args)
            {
                    return fmt::format(fmt_format_sv, args...);
            },
            rootstrings);
}

static void set_root()
{
        const auto cstatus = get_root_string();
        XStoreName(dpy, root, cstatus.data());
        XFlush(dpy);
}

static void xss_exit(const int rc)
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
static CmdResult exec_cmd(const char* cmd)
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

static std::string toggle_lang()
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
                xss_exit(EXIT_FAILURE);
        }

        const auto cmdres = exec_cmd<true>(command);

        if(cmdres.rc != EXIT_SUCCESS ||
           cmdres.output.size() >= FIELD_MAX_LENGTH)
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

        const std::string returnstr = fptr();

        if(returnstr.size() >= FIELD_MAX_LENGTH)
        {
                xss_exit(EXIT_FAILURE);
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
/*        description         pointer to function (handler)   root array index */
        { "toggle kb lang",   toggle_lang,                    R_LANG }
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

static void setup_x()
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

static void handle_sig(const int sig)
{
        const auto r1 = find_if(sig_shell_responses,
                                [&](const auto& r)
                                {
                                        return r.first == sig;
                                });

        if(r1 != std::cend(sig_shell_responses))
        {
                r1->second.resolve();
                return;
        }

        const auto r2 = find_if(sig_builtin_responses,
                                [&](const auto& r)
                                {
                                        return r.first == sig;
                                });
        if(r2 != std::cend(sig_builtin_responses))
        {
                r2->second.resolve();
                return;
        }

        const auto r3 = find_if(sig_group_responses,
                                [&](const auto& r)
                                {
                                        return r.first == sig;
                                });

        r3->second();
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
        const auto cmdres = exec_cmd<true>("pgrep -x xsetstatus | wc -l");
        return cmdres.output != "1";
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

        set_root();
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
        solve_signals();

        XCloseDisplay(dpy);
}
