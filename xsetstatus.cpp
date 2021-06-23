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

/* stl wrappers */
static auto find_if(const auto& container, const auto pred)
{
        return std::find_if(std::begin(container), std::end(container), pred);
}

/* global constexpr variables */
static constexpr int N_FIELDS = 8;
static constexpr int FIELD_MAX_LENGTH = 22;
static constexpr int ROOT_BUFSIZE = N_FIELDS * FIELD_MAX_LENGTH;
static constexpr auto fmt_format_buf = []()
{
        constexpr std::string_view markers[] = {"[{}", " |{}", " |{}]"};

        FixedStr<markers[0].size() +
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

/* using declarations */
using field_buffer_t = FixedStr<FIELD_MAX_LENGTH>;
using root_str_buffer_t = FixedStr<ROOT_BUFSIZE>;

/* enums */
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

/* global variables */
static std::array<field_buffer_t, N_FIELDS> rootstrings = {};
static volatile sig_atomic_t last_sig = -1;
static volatile sig_atomic_t running = 1;
static const int SIGOFFSET = SIGRTMAX;
#ifndef NO_X11
static Display* dpy = nullptr;
static int screen;
static Window root;
#endif

/* structs */
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
        void (*fptr)(field_buffer_t&);
        const int pos;
};

/* template function declarations*/
template<bool>
static int exec_cmd(const char*, field_buffer_t&);

/* function declarations */
static void set_root();
static void xss_exit(const int);
static void toggle_lang(field_buffer_t&);
static void setup();
static void handle_sig(const int);
static bool already_running();
static void u_sig_handler(const int);
static void terminator(const int);
static void init_signals();
static void init_statusbar();
static void run_interval_responses();
static void solve_signals();

/* signal configs  */
static constexpr ShellResponse sr_table[] = {
/*        shell command/script   index in root array */
        { "xss-get-time",        R_TIME },
        { "xss-get-load",        R_LOAD },
        { "xss-get-temp",        R_TEMP },
        { "xss-get-vol",         R_VOL  },
        { "xss-get-mic",         R_MIC  },
        { "xss-get-mem",         R_MEM  },
        { "xss-get-date",        R_DATE },
};

static constexpr BuiltinResponse br_table[] = {
/*        pointer to function (handler)   root array index */
        { toggle_lang,                    R_LANG }
};

static constexpr const ShellResponse* interval_responses[] = {
        &sr_table[0],
        &sr_table[1],
        &sr_table[2],
        &sr_table[5],
};

static const std::pair<int, const ShellResponse&> sig_shell_responses[] = {
/*        signal value    ShellResponse instance */
        { SIGOFFSET - 1,  sr_table[3] },
        { SIGOFFSET - 2,  sr_table[4] }
};

static const std::pair<int, const BuiltinResponse&> sig_builtin_responses[] = {
/*        signal value    BuiltinResponse instance */
        { SIGOFFSET - 3,  br_table[0] }
};

static const std::pair<int, void (*)()> sig_group_responses[] = {
/*        signal value    pointer to function */
        { SIGOFFSET - 4,  run_interval_responses}
};

/* function definitions */
void ShellResponse::resolve() const
{
        if(pos < 0 || pos >= N_FIELDS)
        {
                xss_exit(EXIT_FAILURE);
        }

        const auto rc = exec_cmd<true>(command, rootstrings[pos]);

        if(rc != EXIT_SUCCESS)
        {
                xss_exit(EXIT_FAILURE);
        }
}

void BuiltinResponse::resolve() const
{
        if(pos < 0 || pos >= N_FIELDS)
        {
                xss_exit(EXIT_FAILURE);
        }

        fptr(rootstrings[pos]);
}

template<bool omit_newline>
int exec_cmd(const char* cmd, field_buffer_t& output_buf)
{
        FILE* pipe = popen(cmd, "r");
        if(!pipe)
        {
                xss_exit(EXIT_FAILURE);
        }

        if(fgets(output_buf.data(), std::size(output_buf) + 1, pipe) != nullptr)
        {
                output_buf.set_size();
        }
        const int rc = pclose(pipe);

        if constexpr(omit_newline)
        {
                if(!output_buf.empty() && output_buf.back() == '\n')
                {
                        output_buf.pop_back();
                }
        }

        return rc;
}

void set_root()
{
        root_str_buffer_t buf;

        const auto format_res = std::apply(
            [&](auto&&... args)
            {
                    return fmt::format_to_n(buf.data(), std::size(buf), fmt_format_sv, args.data()...);
            },
            rootstrings);
        *format_res.out = '\0';

#ifndef NO_X11
        XStoreName(dpy, root, buf.data());
        XFlush(dpy);
#else
        fmt::print("{}\n", buf.data());
#endif
}

void xss_exit(const int rc)
{
#ifndef NO_X11
        XCloseDisplay(dpy);
#endif
        std::exit(rc);
}

void toggle_lang(field_buffer_t& output_buf)
{
        static constexpr field_buffer_t ltable[2] = {
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
        output_buf = ltable[flag];
}

void setup()
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

void handle_sig(const int sig)
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

bool already_running()
{
#ifndef IGNORE_ALREADY_RUNNING
        field_buffer_t output;
        int rc;

        rc = exec_cmd<true>("pgrep -x xsetstatus | wc -l", output);
        if(rc != EXIT_SUCCESS)
        {
                std::exit(EXIT_FAILURE);
        }

        if(output == "0")
        {
                return false;
        }
        if(output != "1")
        {
                return true;
        }

        rc = exec_cmd<true>("pgrep -x xsetstatus", output);
        if(rc != EXIT_SUCCESS)
        {
                std::exit(EXIT_FAILURE);
        }

        return std::atoi(output.data()) != getpid();
#else
        return false;
#endif
}

void u_sig_handler(const int sig)
{
        last_sig = sig;
}

void terminator(const int)
{
        running = 0;
}

void init_signals()
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

void init_statusbar()
{
        for(const auto& r : sr_table)
        {
                r.resolve();
        }

        for(const auto& r : br_table)
        {
                r.resolve();
        }
}

void run_interval_responses()
{
        for(const auto& r : interval_responses)
        {
                r->resolve();
        }
}

void solve_signals()
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
