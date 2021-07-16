## xsetstatus
Signal-driven status bar for X written in C++.\
Inspired by [dwmblocks](https://github.com/torrinfail/dwmblocks).\
Needs a window manager that reads the text from the X root window name and prints it accordingly (e.g. [dwm](https://dwm.suckless.org/)).

#### Dependencies:
+ libx11-dev;
+ libfmt-dev;
+ c++20-compatible compiler.

#### Building:

Clone the repo, go in its root directory and run the makefile:

```bash
$ git clone https://github.com/niculaionut/xsetstatus.git
$ cd xsetstatus
$ make xsetstatus
```

#### Usage example:

At start-up, run ```xsetstatus``` in the background. Run a script or a shell command that modifies some properties and then signals the ```xsetstatus``` process appropriately for the fields that need to be updated.

* Increase the volume by 8% and send signal 63 to xsetstatus. The handler sets the last signal to 63. After the workload for the previous signal is finished, the function that updates the volume field is called.
```bash
$ pactl set-sink-volume @DEFAULT_SINK@ +8% && pkill --signal 63 -x 'xsetstatus'
```

* Send signal 60 to xsetstatus every 10 seconds. The handler sets the last signal to 60. After the workload for the previous signal is finished, the meta-response function is called (which updates the fields that don't depend on user keyboard input - e.g. time, system load, CPU temperature).
```bash
#!/bin/sh
while true; do
        pkill --signal 60 -x 'xsetstatus'
        sleep 10
done
```

#### Getting started:

For a simple config with only 3 fields - average load, time and date - and 1 real-time signal handler that updates them, apply the diff file:
```bash
$ git apply default_config.diff
```
The shell commands that update the fields in this minimal config only require the coreutils.

#### Output sample (with dwm - modifying volume, microphone, keyboard language, etc.):

![sample](img/1.gif)
