#include "fixedstr.h"
#include <fmt/core.h>
#include <fmt/format.h>
#include <array>
#include <vector>
#include <numeric>
#include <utility>
#include <unistd.h>
#include <signal.h>
#ifndef NO_X11
#include <X11/Xlib.h>
#endif

/* enums */

/* Defines the order of the status bar fields;
 * First value is the index of the leftmost field. */
enum RootFieldIdx
{
        R_TIME = 0,
        R_LOAD,
        R_TEMP,
        R_VOL,
        R_MIC,
        R_MEM,
        R_GOV,
        R_LANG,
        R_DATE,
        R_SIZE
};

/* global constexpr variables */
static constexpr int N_FIELDS = R_SIZE;
static constexpr int FIELD_MAX_LENGTH = 22;
static constexpr int ROOT_BUFSIZE = N_FIELDS * FIELD_MAX_LENGTH;
static constexpr auto fmt_format_str = []()
{
        using str_t = FixedStr<ROOT_BUFSIZE>;

        constexpr str_t fmt_before_shrink = []()
        {
                str_t default_first  = "[{}";
                str_t default_mid    = " |{}";
                str_t default_last   = " |{}]";

                str_t labels[N_FIELDS] = {};

                std::fill_n(labels, N_FIELDS, default_mid);
                labels[0] = default_first;
                labels[N_FIELDS - 1] = default_last;

                /* custom field labels */

                /* examples:
                 * labels[R_LOAD] = str_t{" |sysload:{}"};
                 * labels[R_MEM]  = str_t{" |memory:{}"};
                 * labels[R_VOL]  = str_t{" |volume:{}"}; */

                return std::accumulate(labels, labels + N_FIELDS, str_t{});
        }();

        return FixedStr<fmt_before_shrink.length()>(fmt_before_shrink);
}();        /* Format string generated at compile-time.
             * Same as manually writing:
             * FixedStr<36> fmt_format_str = "[{} |{} |{} |{} |{} |{} |{} |{} |{}]" */

/* struct declarations */
struct ShellResponse;
struct BuiltinResponse;

/* using declarations */
using field_buffer_t = FixedStr<FIELD_MAX_LENGTH>;
using root_str_buffer_t = FixedStr<ROOT_BUFSIZE>;
using response_tuple_t = std::tuple<const ShellResponse*, const BuiltinResponse*, void (*)()>;
using response_table_t = std::vector<response_tuple_t>;

/* global variables */
static std::array<field_buffer_t, N_FIELDS> rootstrings = {};
static volatile sig_atomic_t last_sig = -1;
static volatile sig_atomic_t running = 1;
static const int CSIGRTMAX = SIGRTMAX;
static const int CSIGRTMIN = SIGRTMIN;
static const int SIGRANGE = CSIGRTMAX - CSIGRTMIN + 1;
#ifndef NO_X11
static Display* dpy = nullptr;
static int screen;
static Window root;
#endif

/* template function declarations */
template<std::size_t>
static void insert_response(auto&, const int, const auto);

template<const auto&, std::size_t...>
static void run_meta_response();

/* function declarations */
static int  exec_cmd(const char*, field_buffer_t&);
static void set_root();
static void xss_exit(const int, const char*);
static void toggle_lang(field_buffer_t&);
static void toggle_cpu_gov(field_buffer_t&);
static void toggle_mic(field_buffer_t&);
static void setup();
static void handle_sig(const int);
static bool already_running();
static void u_sig_handler(const int);
static void terminator(const int);
static void init_terminator();
static void init_statusbar();
static void solve_signals();

/* struct definitions */

/* respond to signal by writing shell command output to the field buffer */
struct ShellResponse
{
public:
        void resolve() const;

public:
        const char* command;
        field_buffer_t& rbuf;
};

/* respond to signal by calling a function that modifies the field buffer */
struct BuiltinResponse
{
public:
        void resolve() const;

public:
        void (*fptr)(field_buffer_t&);
        field_buffer_t& rbuf;
};

/* signal configs  */
static constexpr std::array sr_table = std::to_array<ShellResponse>({
        {   /* time */
            R"(date +%H:%M:%S)",        /* shell command */
            rootstrings[R_TIME]         /* reference to root buffer */
        },
        {   /* sys load*/
            R"(uptime | grep -wo "average: .*," | cut --delimiter=' ' -f2 | head -c4)",
            rootstrings[R_LOAD]
        },
        {   /* cpu temp*/
            R"(sensors | grep -F "Core 0" | awk '{print $3}' | cut -c2-5)",
            rootstrings[R_TEMP]
        },
        {   /* volume */
            R"(amixer sget Master | tail -n1 | get-from-to '[' ']' '--amixer')",
            rootstrings[R_VOL]
        },
        {   /* memory usage */
            R"(xss-get-mem)",
            rootstrings[R_MEM]
        },
        {   /* date */
            R"(date "+%d.%m.%Y")",
            rootstrings[R_DATE]
        }
});

static constexpr std::array br_table = std::to_array<BuiltinResponse>({
       /* pointer to function    reference to root buffer */
        { toggle_lang,           rootstrings[R_LANG] },
        { toggle_cpu_gov,        rootstrings[R_GOV]  },
        { toggle_mic,            rootstrings[R_MIC]  }
});

static const response_table_t rt_responses = []()
{
        response_table_t responses(SIGRANGE, {nullptr, nullptr, nullptr});

     /* shell responses               signal value    pointer to ShellResponse instance */
        insert_response<0>(responses, CSIGRTMAX - 1,  &sr_table[3]);

     /* builtin responses             signal value    pointer to BuiltinResponse instance */
        insert_response<1>(responses, CSIGRTMAX - 3,  &br_table[0]);
        insert_response<1>(responses, CSIGRTMAX - 5,  &br_table[1]);
        insert_response<1>(responses, CSIGRTMAX - 2,  &br_table[2]);

     /* meta responses                signal value    pointer to void (*)() function */
        insert_response<2>(responses, CSIGRTMAX - 4,  &run_meta_response<sr_table, 0, 1, 2, 4>);

        return responses;
}();

/* member function definitions */
void ShellResponse::resolve() const
{
        const int rc = exec_cmd(command, rbuf);
        if(rc != EXIT_SUCCESS)
        {
                xss_exit(EXIT_FAILURE, "pclose() did not return EXIT_SUCCESS");
        }
}

void BuiltinResponse::resolve() const
{
        fptr(rbuf);
}

/* function / template function definitions */
template<std::size_t pos>
void insert_response(auto& arr, const int sig, const auto ptr)
{
        std::get<pos>(arr[sig - CSIGRTMIN]) = ptr;
        signal(sig, u_sig_handler);
}

template<const auto& resp_table, std::size_t... indexes>
void run_meta_response()
{
        (resp_table[indexes].resolve(), ...);
}

int exec_cmd(const char* cmd, field_buffer_t& output_buf)
{
        FILE* pipe = popen(cmd, "r");
        if(pipe == nullptr)
        {
                xss_exit(EXIT_FAILURE, "popen() failed");
        }

        output_buf.clear();
        if(fgets(output_buf.data(), std::size(output_buf) + 1, pipe) != nullptr)
        {
                output_buf.set_length();

                /* check for trailing newline and delete it */
                if(!output_buf.empty() && output_buf.back() == '\n')
                {
                        output_buf.pop_back();
                }
        }

        return pclose(pipe);
}

void set_root()
{
        root_str_buffer_t buf;

        const auto format_res = std::apply(
            [&](auto&&... args)
            {
                    return fmt::format_to_n(buf.data(),
                                            std::size(buf),
                                            std::string_view(fmt_format_str),
                                            std::string_view(args)...);
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

void xss_exit(const int rc, const char* why)
{
#ifndef NO_X11
        XCloseDisplay(dpy);
#endif
        fmt::print(stderr, "{}\n", why);
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

        static std::size_t idx = 1;

        idx = !idx;
        std::system(commands[idx]);
        output_buf = ltable[idx];
}

void toggle_cpu_gov(field_buffer_t& output_buf)
{
        static constexpr field_buffer_t freq_table[2] = {
                {"*"},
                {"$"}
        };

        static constexpr const char* commands[2] = {
                "xss-set-save",
                "xss-set-perf"
        };

        static std::size_t idx = 1;

        idx = !idx;
        std::system(commands[idx]);
        output_buf = freq_table[idx];
}

void toggle_mic(field_buffer_t& output_buf)
{
        static constexpr field_buffer_t mic_status_table[2] = {
                {"0"},
                {"1"}
        };

        static constexpr const char* command = "pactl set-source-mute @DEFAULT_SOURCE@ toggle";

        static std::size_t idx = 1;

        idx = !idx;
        std::system(command);
        output_buf = mic_status_table[idx];
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
        const auto& sig_resp = rt_responses[sig];

        auto* shellptr = std::get<0>(sig_resp);
        if(shellptr != nullptr)
        {
                shellptr->resolve();
                return;
        }

        auto* builtinptr = std::get<1>(sig_resp);
        if(builtinptr != nullptr)
        {
                builtinptr->resolve();
                return;
        }

        auto* fptr = std::get<2>(sig_resp);
        fptr();
}

bool already_running()
{
#ifndef IGNORE_ALREADY_RUNNING
        field_buffer_t output;

        exec_cmd("pgrep -x xsetstatus | wc -l", output);
        if(output == "0")
        {
                return false;
        }
        if(output != "1")
        {
                return true;
        }

        exec_cmd("pgrep -x xsetstatus", output);
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

void init_terminator()
{
        for(int sig = CSIGRTMIN; sig <= CSIGRTMAX; ++sig)
        {
                struct sigaction current;

                sigaction(sig, nullptr, &current);
                if(current.sa_handler == nullptr)
                {
                        signal(sig, terminator);
                }
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

void solve_signals()
{
        while(running)
        {
                if(last_sig != -1)
                {
                        handle_sig(last_sig - CSIGRTMIN);
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

#ifdef NO_X11
        fmt::print(stderr,
                   "xsetstatus: running in NO_X11 mode\n"
                   "Status bar content will be printed to stdout. PID is {}.\n\n",
                   getpid());
#endif

        setup();
        init_statusbar();
        init_terminator();
        solve_signals();

#ifndef NO_X11
        XCloseDisplay(dpy);
#endif

        return EXIT_SUCCESS;
}
