#include "fixedstr.h"
#include <fmt/core.h>
#include <fmt/format.h>
#include <array>
#include <vector>
#include <utility>
#include <unistd.h>
#include <signal.h>
#ifndef NO_X11
#include <X11/Xlib.h>
#endif

/* global constexpr variables */
static constexpr int N_FIELDS = 9;
static constexpr int FIELD_MAX_LENGTH = 22;
static constexpr int ROOT_BUFSIZE = N_FIELDS * FIELD_MAX_LENGTH;
static constexpr auto fmt_format_str = []()
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

/* enums */
enum ResponseRootIdx
{
        R_TIME = 0,
        R_LOAD,
        R_TEMP,
        R_VOL,
        R_MIC,
        R_MEM,
        R_GOV,
        R_LANG,
        R_DATE
};

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
template<bool>
static int exec_cmd(const char*, field_buffer_t&);

template<std::size_t>
static void insert_response(auto&, const int, const auto);

/* function declarations */
static void set_root();
static void xss_exit(const int, const char*);
static void toggle_lang(field_buffer_t&);
static void toggle_cpu_gov(field_buffer_t&);
static void setup();
static void handle_sig(const int);
static bool already_running();
static void u_sig_handler(const int);
static void terminator(const int);
static void init_terminator();
static void init_statusbar();
static void run_interval_responses();
static void solve_signals();

/* struct definitions */
struct ShellResponse
{
public:
        void resolve() const;

public:
        const char* command;
        std::size_t pos;
};

struct BuiltinResponse
{
public:
        void resolve() const;

public:
        void (*fptr)(field_buffer_t&);
        std::size_t pos;
};

/* signal configs  */
static constexpr ShellResponse sr_table[] = {
       /* shell command/script   root array index */
        { "xss-get-time",        R_TIME },
        { "xss-get-load",        R_LOAD },
        { "xss-get-temp",        R_TEMP },
        { "xss-get-vol",         R_VOL  },
        { "xss-get-mic",         R_MIC  },
        { "xss-get-mem",         R_MEM  },
        { "xss-get-date",        R_DATE },
};

static constexpr BuiltinResponse br_table[] = {
       /* pointer to function (handler)   root array index */
        { toggle_lang,                    R_LANG },
        { toggle_cpu_gov,                 R_GOV }
};

static constexpr const ShellResponse* interval_responses[] = {
     /* pointer to ShellResponse instance */
        &sr_table[0],
        &sr_table[1],
        &sr_table[2],
        &sr_table[5],
};

static const response_table_t rt_responses = []()
{
        response_table_t responses(SIGRANGE, {nullptr, nullptr, nullptr});

     /* shell responses               signal value    pointer to ShellResponse instance */
        insert_response<0>(responses, CSIGRTMAX - 1,  &sr_table[3]);
        insert_response<0>(responses, CSIGRTMAX - 2,  &sr_table[4]);

     /* builtin responses             signal value    pointer to BuiltinResponse instance */
        insert_response<1>(responses, CSIGRTMAX - 3,  &br_table[0]);
        insert_response<1>(responses, CSIGRTMAX - 5,  &br_table[1]);

     /* group responses               signal value    pointer to function */
        insert_response<2>(responses, CSIGRTMAX - 4,  &run_interval_responses);

        return responses;
}();

/* member function definitions */
void ShellResponse::resolve() const
{
        const auto rc = exec_cmd<true>(command, rootstrings[pos]);
        if(rc != EXIT_SUCCESS)
        {
                xss_exit(EXIT_FAILURE, "pclose() did not return EXIT_SUCCESS");
        }
}

void BuiltinResponse::resolve() const
{
        fptr(rootstrings[pos]);
}

/* function / template function definitions */
template<bool omit_newline>
int exec_cmd(const char* cmd, field_buffer_t& output_buf)
{
        FILE* pipe = popen(cmd, "r");
        if(!pipe)
        {
                xss_exit(EXIT_FAILURE, "popen() failed");
        }

        if(fgets(output_buf.data(), std::size(output_buf) + 1, pipe) != nullptr)
        {
                output_buf.set_size();

                if constexpr(omit_newline)
                {
                        if(!output_buf.empty() && output_buf.back() == '\n')
                        {
                                output_buf.pop_back();
                        }
                }
        }

        return pclose(pipe);
}

template<std::size_t pos>
void insert_response(auto& arr, const int sig, const auto val)
{
        std::get<pos>(arr[sig - CSIGRTMIN]) = val;
        signal(sig, u_sig_handler);
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

        if(auto* shellptr = std::get<0>(sig_resp))
        {
                shellptr->resolve();
        }
        else if(auto* builtinptr = std::get<1>(sig_resp))
        {
                builtinptr->resolve();
        }
        else
        {
                auto* fptr = std::get<2>(sig_resp);
                fptr();
        }
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
