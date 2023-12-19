#!/bin/sh

# Specify the camera codec
CODEC_H264=1
CODEC_H265=2
CAMERA_CODEC=$CODEC_H265

CAM_STREAM_URL=rtsp://192.168.50.25:8554/main.264
CAM_PORT=8554
REDIR_PORT=8900

# Get wlan0 IP address
# IP4="$(ip -o -4 addr list wlan0 | awk '{print $4}' | cut -d/ -f1)"
IP4=127.0.0.1

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
    encoding-name=H265,payload=96\" )" &

# Set up and launch gstreamer pipeline
# Check the input codec and print messages
if [ "$CAMERA_CODEC" -eq "$CODEC_H264" ]; then
    echo "Streaming from: $IP4:$REDIR_PORT"
    echo "Camera codec is H.264"
    echo "Redirect and streaming with H.265"
    echo "Ctrl-C to stop"
    gst-launch-1.0 \
     rtspsrc location=$CAM_STREAM_URL latency=0 ! \
     rtph265depay ! h265parse ! queue ! qtivdec ! \
     omxh264enc target-bitrate=13000000 periodicity-idr=1 interval-intraframes=29 ! \
     h264parse config-interval=1 ! rtph264pay pt=96 ! \
     udpsink port=$CAM_PORT host=127.0.0.1
elif [ "$CAMERA_CODEC" -eq "$CODEC_H265" ]; then
    echo "Streaming from: $IP4:$REDIR_PORT"
    echo "Camera codec is H.265"
    echo "Redirect and streaming with H.265"
    echo "Ctrl-C to stop"
    gst-launch-1.0 \
     rtspsrc location=$CAM_STREAM_URL latency=0 ! \
     rtph265depay ! h265parse ! queue ! qtivdec ! \
     omxh265enc target-bitrate=13000000 interval-intraframes=29 ! \
     h265parse config-interval=1 ! rtph265pay pt=96 ! \
     udpsink port=$CAM_PORT host=127.0.0.1
else
    echo "Unknown codec!"
    pkill gst
    echo "Abort."
    exit 2
fi
