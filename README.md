## xsetstatus
Minimal status bar for X written in C++.\
Inspired by [dwmblocks](https://github.com/torrinfail/dwmblocks).\
Needs a window manager that reads the text from the X root window name and displays it accordingly (e.g. [dwm](https://dwm.suckless.org/)).

#### Dependencies:
+ libx11-dev;
+ libfmt-dev;
+ c++20-compatible compiler (at least `g++ version 10.1` or `clang++ version 11.0.0`).

#### Building:

Clone the repo, go in its root directory, apply the diff file and run the makefile:

```bash
$ git clone https://github.com/niculaionut/xsetstatus.git
$ cd xsetstatus
$ patch -i default_config.diff
$ make xsetstatus
```

#### Getting started:

For testing purposes, if you want the status to be written to standard output, run the makefile with the target `nox11`:
```bash
$ make nox11
```

The default config has 3 fields - average load, time and date. The shell commands that update those fields only require the coreutils.

Short description of the configuration components:
+ `class Response` - Response interface;
+ `class ShellResponse` - Responds to a signal by writing a shell command's output to the field buffer;
+ `class BuiltinResponse` - Responds to signal by calling a function that takes a reference to the field buffer and modifies it;
+ `class MetaResponse` - Responds to a signal by calling other responses through a `void (*)()` function pointer;
+ `enum RootFieldIdx` - Defines a unique index for each status field represented by an element in the `rootstrings` array;
+ `fmt_format_str` - The string used by `fmt::format_to_n` to format the status bar output;
+ `sr_table`, `br_table`, `mr_table` - Constexpr arrays containing the defined shell responses, builtin responses and meta responses, respectively;
+ `rt_responses` - Lookup table of size `SIGRTMAX - SIGRTMIN + 1`. The information regarding the handling of a real-time signal with value `i` is stored in the element `rt_responses[i - SIGRTMIN]`. Each element in `rt_responses` is a pointer to the `Response` abstract base class. Upon receiving a valid real-time signal, the virtual method `resolve` is called, which deals with the signal based on the runtime type of the response.


#### Usage example:

At start-up, run ```xsetstatus``` in the background. Run a script or a shell command that modifies some properties and then signals the ```xsetstatus``` process appropriately for the fields that need to be updated.

Assuming the `<config-signal>` value is between `SIGRTMIN` and `SIGRTMAX`, some examples are as follows:

* Increase the volume by 8% and send `<config-signal>` to `xsetstatus`. After the workload for the previous signal is finished, the function that updates the volume field is called.
```bash
$ pactl set-sink-volume @DEFAULT_SINK@ +8% && pkill --signal <config-signal> -x 'xsetstatus'
```

* Send `<config-signal>` to `xsetstatus` every 10 seconds. After the workload for the previous signal is finished, the meta-response function is called (which updates the fields that don't depend on user keyboard input - e.g. time, system load, CPU temperature).
```bash
#!/bin/sh
while true; do
        pkill --signal <config-signal> -x 'xsetstatus'
        sleep 10
done
```

#### Output sample (with dwm - modifying volume, microphone, keyboard language, etc.):

![sample](https://raw.githubusercontent.com/niculaionut/xsetstatus/main/img/1.gif)
