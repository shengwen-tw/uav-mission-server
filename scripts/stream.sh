#!/bin/sh

CAM_STREAM_URL=rtsp://192.168.50.25:8554/main.264
CAM_PORT=8554
REDIR_PORT=8900

# Get wlan0 IP address
IP4="$(ip -o -4 addr list wlan0 | awk '{print $4}' | cut -d/ -f1)"

echo "Streaming from: $IP4:$REDIR_PORT"
echo "Ctl-C to stop"

# This function is called when Ctrl-C is sent
function trap_ctrlc ()
{
    # perform cleanup here
    echo "Ctrl-C caught...performing clean up"

    pkill gst

    # Exit shell script with error code 2
    # if omitted, shell script will continue execution
    exit 2
}

# Initialise trap to call trap_ctrlc function
# when signal 2 (SIGINT) is received
trap "trap_ctrlc" 2

# Set up and launch RTSP server
gst-rtsp-server -a $IP4 -p $REDIR_PORT -m /live \
 "( udpsrc name=pay0 port=$CAM_PORT \
    caps=\"application/x-rtp,media=video,clock-rate=90000, \
    encoding-name=H264,payload=96\" )" &

# Set up and launch gstreamer pipeline
gst-launch-1.0 \
rtspsrc location=$CAM_STREAM_URL latency=0 ! \
rtph265depay ! h265parse ! queue ! qtivdec ! \
omxh264enc target-bitrate=13000000 periodicity-idr=1 interval-intraframes=29 ! \
h264parse config-interval=1 ! rtph264pay pt=96 ! \
udpsink port=$CAM_PORT host=127.0.0.1
