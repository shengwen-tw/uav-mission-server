# UAV Mission Server

The UAV mission server functions as a go-between for the flight control unit and the UAV mission,
enabling multiple clients to communicate via a serial port over TCP and processing ground control commands using the MAVLink protocol.
This server permits the initial client that connects (following a first-come, first-served approach) to send data to the serial port.

![architecture diagram](docs/arch.png?raw=true)

## Prerequisites

Install GStreamer:
```shell
$ sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

Install libYAML:
```shell
$ sudo apt install libyaml-dev
```

[future](https://pypi.org/project/future/) is required to generate MAVLink headers.
```shell
$ pip3 install future
```

## Build

```shell
$ make
```

## Usage

**Start Video Streaming:**

```shell
$ scripts/streaming-server.sh
```

**Launch Mission Server:**

```shell
$ build/mission-server [-p tcp_port] [-r,--print-rc]
```

**Command Sending (The server must be launched first):**

To play a tune with designated track number:

```shell
$ build/mission-server --send-tune track-number
```

### Arguments

* TCP Port -- The TCP to accept connections on (optional, the default is 8278).

## Configuration

### Serial Port Configuration

The serial port can be configured via [serial.yaml](https://github.com/shengwen-tw/uav-mission-server/blob/master/configs/serial.yaml), where the default settings are given as follows:

```yaml
port: /dev/ttyUSB0
baudrate: 57600
parity: N
data-bits: 8
stop-bits: 1
```
Note that:

* `port` is the name of the serial port to use (e.g., `/dev/ttyS0` on UNIX).
* `baudrate` is the baud rate to use, e.g., 115200.
* `parity` can be N for none, O for odd, E for even, M for mark, or S for space.
* `data-bits` can be 5, 6, 7, or 8.
* `stop-bits` can be 1, 1.5, or 2.

### Camera and Gimbal Configuration

Currently, the `uav-mission-server` supports up to 6 camera-gimbal pairs defined in [devices.yaml](https://github.com/shengwen-tw/uav-mission-server/blob/master/configs/devices.yaml).

A valid device setting includes a device type name, the configuration file corresponds with the device type, and should be enabled with `true`. For example:

```yaml
device0_config: devices/siyi_a8_mini.yaml
device0_type: siyi
device0_enabled: true
```

Different device types have distinguished configuration formats (Though currently, the `uav-mission-server` supports `siyi` as the sole type only). Explore the directory [config/devices/](https://github.com/shengwen-tw/uav-mission-server/tree/master/configs/devices) for more information.
