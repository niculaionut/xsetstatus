## xsetstatus
Minimal status bar for X written in C++.\
Inspired by [dwmblocks](https://github.com/torrinfail/dwmblocks).\
Needs a window manager that reads the text from the X root window name and prints it accordingly (e.g. [dwm](https://dwm.suckless.org/)).

#### Dependencies:
+ libx11-dev;
+ libfmt-dev;
+ c++20-compatible compiler (at least `g++ version 10.1` or `clang++ version 11.0.0`).

#### Building:

Clone the repo, go in its root directory and run the makefile:

```bash
$ git clone https://github.com/niculaionut/xsetstatus.git
$ cd xsetstatus
$ make xsetstatus
```

#### Getting started:

For a simple config with only 3 fields - average load, time and date - and 1 real-time signal handler that updates them, apply the diff file:
```bash
$ patch -i default_config.diff
```
The shell commands that update the fields in this minimal config only require the coreutils.

Short description of the configuration components:
+ `enum RootFieldIdx` - Defines a unique index for each status field represented by an element in the `rootstrings` array;
+ `fmt_format_str` - The string used by `fmt::format_to_n` to format the status bar output;
+ `sr_table` and `br_table` - Constexpr arrays containing the defined shell responses and builtin responses, respectively;
+ `rt_responses` - Lookup table of size `SIGRTMAX - SIGRTMIN + 1`. The information regarding the handling of a real-time signal with value `i` is stored in the element `rt_responses[i - SIGRTMIN]`. Each element in `rt_responses` is a tuple of 3 pointers. Upon receiving a valid signal, only 1 of those 3 pointers will be non-null (either the pointer to a `ShellResponse` instance, the pointer to a `BuiltinResponse` instance, or the pointer to a void function taking no arguments).


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

![sample](img/1.gif)
