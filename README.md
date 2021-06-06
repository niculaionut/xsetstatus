### xsetstatus
Signal-driven status bar for X.

Inspired by [dwmblocks](https://github.com/torrinfail/dwmblocks).

### Dependencies:
+ libx11-dev
+ libfmt
+ c++20-compatible compiler

### Usage example:

* Increase volume by 8% and send signal 63 to xsetstatus. The handler sets the last signal to 63. After the workload for the previous signal is finished, the function that updates the appropriate fields is called:
```bash
pactl set-sink-volume @DEFAULT_SINK@ +8% && pkill --signal 63 -x 'xsetstatus'
```

* Send signal 60 to xsetstatus every 10 seconds. The handler sets the last signal to 60. After the workload for the previous signal is finished, the group update function is called (which updates the fields that are independend of user keyboard input - e.g. time, system load, CPU temperature):
```bash
while true; do
        pkill --signal 60 -x 'xsetstatus'
        sleep 10
done
```
