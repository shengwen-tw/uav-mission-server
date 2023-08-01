# UART Server

A simple program that serves a serial port over TCP to multiple clients. The first connected
client (in a FIFO fashion) can also send data to the serial port.

## Usage

```shell
$ ./uart-server serial_port config_str [tcp_port]
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

## MAVLink C Code Generation

```
git clone https://github.com/ArduPilot/pymavlink.git
python3 ./pymavlink/tools/mavgen.py --lang=C --wire-protocol=2.0 --output=generated/include/mavlink/v2.0 message_definitions/common.xml
```
