#include <fmt/core.h>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>

struct CmdResult
{
        const std::string output;
        const int rc;
};

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

        return {std::move(result), rc};
}

using handler_t = void (*)(const int);

constexpr int N_FIELDS = 7;
constexpr int FIELD_MAX_LENGTH = 40;
constexpr int DEFAULT_SLEEP_TIME = 10;

char rootstrings[N_FIELDS][FIELD_MAX_LENGTH] = {};

const std::string format_str = []()
{
        std::string f = "[{}";

        for(int i = 0; i < N_FIELDS - 2; ++i)
        {
                f += " |{}";
        }

        f += " |{}]";

        return f;
}();

struct Response
{
        void resolve() const
        {
                if(pos < 0)
                {
                        std::system(command);
                        return;
                }

                const auto cmdres = exec_cmd(command);

                if(cmdres.rc != EXIT_SUCCESS)
                {
                        fmt::print(stderr,
                                   "failure: command '{}' exited with return code {}\n",
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

                std::strcpy(rootstrings[pos], cmdres.output.c_str());
        }

        const char* command;
        const int pos;

private:
};

const std::unordered_map<int, Response> sig_responses = {{SIGRTMAX, {"at-startup", -1}}};

const Response interval_responses[] = {
    {"xss-get-time", 0},
    {"xss-get-load", 1},
    {"xss-get-temp", 2},
};

void run_interval_responses()
{
        for(const auto& r : interval_responses)
        {
                r.resolve();
        }
}

void handler(const int sig)
{
        const auto& r = sig_responses.at(sig);
        r.resolve();
}

void init_signals()
{
        for(const auto& [sig, r] : sig_responses)
        {
                signal(sig, handler);
        }
}

std::string get_root_string()
{
        return fmt::format(format_str, rootstrings[0], rootstrings[1], rootstrings[2],
                           rootstrings[3], rootstrings[4], rootstrings[5], rootstrings[6]);
}

void set_root()
{
        static Display* dpy = nullptr;

        Display* d = XOpenDisplay(nullptr);
        if(d)
        {
                dpy = d;
        }
        const int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
        XStoreName(dpy, root, get_root_string().c_str());
        XCloseDisplay(dpy);
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
        init_signals();

        update_loop();
}
