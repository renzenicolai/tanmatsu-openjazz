# [OpenJazz](https://github.com/AlisterT/openjazz) for Tanmatsu

OpenJazz (Jazz Jackrabbit 1 engine) port for the ESP32-P4 based Tanmatsu.

## Game data

OpenJazz is an engine and does not include Epic's copyrighted game files. Copy the
files from a Jazz Jackrabbit installation to `/sdcard/openjazz` on the Tanmatsu.
At minimum, `BLOCKS.000`, `PLANET.000` and one episode's level files are needed.

## Build

Install ESP-IDF 5.5.1 or newer, then run:

```sh
make prepare
make build
make flash monitor
```
