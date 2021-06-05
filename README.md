### xsetstatus
Signal-driven status bar for X.

Inspired by [dwmblocks](https://github.com/torrinfail/dwmblocks).

### Dependencies:
+ libx11-dev
+ libfmt
+ c++20-compatible compiler

### Usage example:

* Increase volume by 8% and send signal 63 to xsetstatus. The handler will run the script that gets current volume and will update the appropriate status bar field:
```bash
pactl set-sink-volume @DEFAULT_SINK@ +8% && pkill --signal 63 -x 'xsetstatus'
```

* Send signal 60 to xsetstatus every 10 seconds. The handler will update the configured set of fields (those which do not depend on user keyboard input - e.g. current time, system load, CPU temperature):
```bash
while true; do
        pkill --signal 60 -x 'xsetstatus'
        sleep 10
done
```
