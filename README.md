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
$ build/mission-server -s serial_port -b config_str [-p tcp_port] [-r,--print-rc]
```

**Command Sending (The server must be launched first):**

To play a tune with designated track number:

```shell
$ build/mission-server --send-tune track-number
```

### Arguments

* Serial Port -- The name of the serial port to use (e.g. `/dev/ttyS0` on UNIX).

* Serial Port Configuration -- A string specifying how to configure the serial port. The format of
    the serial port configuration string is `baudrate[,parity[,data-bits[,stop-bits]]]`.

    * **baudrate** - The baud rate to use, e.g. 115200 (required)
    * **parity** - N for none, O for odd, E for even, M for mark, S for space (optional, default is N)
    * **data-bits** - 5, 6, 7, or 8 (optional, default is 8)
    * **stop-bits** - 1, 1.5, or 2 (optional, default is 1)

    Optional parameters can be omitted entirely if they're at the end, or left empty if you want to use
    the default and specify a parameter after them. For example, to configure a baud rate of 19200 with 7
    data bits you can simply pass `19200,,7` as the configuration string.

* TCP Port -- The TCP to accept connections on (optional, the default is 8278).

### Device configuration

**Camera and Gimbal Configuration:**

Currently, the `uav-mission-server` supports up to 6 camera-gimbal pairs defined in [devices.yaml](https://github.com/shengwen-tw/uav-mission-server/blob/master/configs/devices.yaml).

A valid device setting includes a device type name, the configuration file corresponds with the device type, and should be enabled with `true`. For example:

```yaml
device0_config: devices/siyi_a8_mini.yaml
device0_type: siyi
device0_enabled: true
```

Different device types have distinguished configuration formats (Though currently, the `uav-mission-server` supports `siyi` as the sole type only). Explore the directory [config/devices/](https://github.com/shengwen-tw/uav-mission-server/tree/master/configs/devices) for more information.
