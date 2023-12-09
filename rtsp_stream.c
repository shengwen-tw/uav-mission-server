#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <glib.h>
#include <gst/gst.h>

#define RTSP_STREAM_URL "rtsp://10.20.13.136:8900/live"
#define VIDEO_FORMAT "video/x-raw"
#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720

typedef struct {
    GstElement *source;
    GstElement *depay;
    GstElement *parse;
    GstElement *decoder;
    GstElement *convert;
    GstElement *scale;
    GstElement *encoder;
    GstElement *mux;
    GstElement *sink;
} gst_data_t;

static void pad_added_handler(GstElement *src, GstPad *pad, gst_data_t *data)
{
    /* Get the sink pad */
    GstPad *sink_pad = gst_element_get_static_pad(data->depay, "sink");

    /* Ignore if pad is already lined with the signal */
    if (gst_pad_is_linked(sink_pad)) {
        gst_object_unref(sink_pad);
        return;
    }

    /* Link pad with the signal */
    if (GST_PAD_LINK_FAILED(gst_pad_link(pad, sink_pad)))
        printf("Failed to link pad and signal\n");
}

bool snapshot_request = false;

void gstreamer_take_photo(void)
{
    snapshot_request = true;
}

void generate_timestamp(char *timestamp)
{
    /* Size of the timestamp string:
     * 4 (year) + 2 (month) + 2 (day) + 2 (hour) + 2 (minute) +
     * 2 (second) + 1 (null terminator)
     */

    /* Get current time */
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    /* Format the timestamp string */
    sprintf(timestamp, "%04d%02d%02d%02d%02d%02d", 1900 + timeinfo->tm_year,
            1 + timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour,
            timeinfo->tm_min, timeinfo->tm_sec);
}

static void on_new_sample_handler(GstElement *sink, gpointer data)
{
    /* Retrieve the frame data */
    GstSample *sample;
    g_signal_emit_by_name(sink, "pull-sample", &sample, NULL);

    if (sample && snapshot_request) {
        snapshot_request = false;

        /* Obtain the JPEG image data */
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);

        char timestamp[15] = {0};
        generate_timestamp(timestamp);

        /* Save JPEG data to file */
        gchar filename[50];
        g_snprintf(filename, sizeof(filename), "%s.jpg", timestamp);
        FILE *file = fopen(filename, "wb");

        printf("Photo saved!\n");
        fwrite(map.data, 1, map.size, file);
        fclose(file);

        /* Release resources */
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
    }
}

void *rtsp_jpeg_saver(void *args)
{
    gst_init(NULL, NULL);

    /* Create pipeline */
    GstElement *pipeline = gst_pipeline_new("rtsp-streaming-pipeline");

    /* Create elements */
    gst_data_t data;
    data.source = gst_element_factory_make("rtspsrc", "source");
    data.depay = gst_element_factory_make("rtph264depay", "depay");
    data.parse = gst_element_factory_make("h264parse", "parse");
    data.decoder = gst_element_factory_make("avdec_h264", "decoder");
    data.convert = gst_element_factory_make("videoconvert", "convert");
    data.scale = gst_element_factory_make("videoscale", "scale");
    data.encoder = gst_element_factory_make("jpegenc", "encoder");
    data.sink = gst_element_factory_make("appsink", "sink");

    if (!pipeline || !data.source || !data.depay || !data.parse ||
        !data.decoder || !data.convert || !data.scale || !data.encoder ||
        !data.sink) {
        printf("Failed to create one or multiple gst elements\n");
        return NULL;
    }

    /* clang-format off */
    GstCaps *caps =
        gst_caps_new_simple(VIDEO_FORMAT,
                            "width", G_TYPE_INT, FRAME_WIDTH,
                            "height", G_TYPE_INT, FRAME_HEIGHT,
                            NULL);
    /* clang-format on */

    /* clang-format off */
    g_object_set(G_OBJECT(data.source),
                 "location", RTSP_STREAM_URL,
                 "latency", 0,
                 NULL);
    /* clang-format on */
    g_object_set(G_OBJECT(data.encoder), "quality", 90, NULL);
    g_object_set(G_OBJECT(data.sink), "emit-signals", TRUE, NULL);

    /* Add all elements into the pipe line */
    gst_bin_add_many(GST_BIN(pipeline), data.source, data.depay, data.parse,
                     data.decoder, data.convert, data.scale, data.encoder,
                     data.sink, NULL);

    /* Link all elements in the pipeline */
    if (!gst_element_link_many(data.depay, data.parse, data.decoder,
                               data.convert, data.scale, NULL)) {
        g_printerr("Failed to link elements (Stage 1)\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    if (!gst_element_link_filtered(data.scale, data.encoder, caps)) {
        g_printerr("Failed to link elements (Stage 2)\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    if (!gst_element_link_many(data.encoder, data.sink, NULL)) {
        g_printerr("Failed to link elements (Stage 3)\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    /* Attach signal handlers */
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler),
                     &data);
    g_signal_connect(data.sink, "new-sample", G_CALLBACK(on_new_sample_handler),
                     NULL);

    /* Start GStreamer */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    printf("gstreamer: Start playing...\n");

    /* Wait until received error or EOS message */
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                               GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    printf("gstreamer: Bye\n");

    return NULL;
}

GstElement *pipeline;

void sigintHandler(int unused)
{
    g_print("Sending EoS");
    gst_element_send_event(pipeline, gst_event_new_eos());
}

int rtsp_mp4_saver(void)
{
    signal(SIGINT, sigintHandler);

    gst_init(NULL, NULL);

    /* Create pipeline */
    pipeline = gst_pipeline_new("rtsp-streaming-pipeline");

    /* Create elements */
    gst_data_t data;
    data.source = gst_element_factory_make("rtspsrc", "source");
    data.depay = gst_element_factory_make("rtph264depay", "depay");
    data.parse = gst_element_factory_make("h264parse", "parse");
    data.decoder = gst_element_factory_make("avdec_h264", "decoder");
    data.convert = gst_element_factory_make("videoconvert", "convert");
    data.scale = gst_element_factory_make("videoscale", "scale");
    data.encoder = gst_element_factory_make("x264enc", "encoder");
    data.mux = gst_element_factory_make("mp4mux", "multiplexer");
    data.sink = gst_element_factory_make("filesink", "sink");

    if (!pipeline || !data.source || !data.depay || !data.parse ||
        !data.decoder || !data.convert || !data.scale || !data.encoder ||
        !data.mux || !data.sink) {
        printf("Failed to create one or multiple gst elements\n");
        return -1;
    }

    /* clang-format off */
    g_object_set(G_OBJECT(data.source),
                 "location", RTSP_STREAM_URL,
                 "latency", 0,
                 NULL);
    /* clang-format on */
    g_object_set(G_OBJECT(data.sink), "location", "record.mp4", NULL);

    /* Add all elements into the pipe line */
    gst_bin_add_many(GST_BIN(pipeline), data.source, data.depay, data.parse,
                     data.decoder, data.convert, data.scale, data.encoder,
                     data.mux, data.sink, NULL);

    /* Link all elements in the pipeline */
    if (!gst_element_link_many(data.depay, data.parse, data.decoder,
                               data.convert, data.scale, data.encoder, data.mux,
                               data.sink, NULL)) {
        g_printerr("Failed to link elements\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Attach signal handlers */
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler),
                     &data);

    /* Start GStreamer */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    printf("gstreamer: Start playing...\n");

    /* Wait until received error or EOS message */
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                               GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}

int rtsp_stream_display(void)
{
    gst_init(NULL, NULL);

    /* Create new pipeline */
    GstElement *pipeline = gst_pipeline_new("my-pipeline");

    // Create elements
    gst_data_t data;
    data.source = gst_element_factory_make("rtspsrc", "rtspsrc0");
    data.depay = gst_element_factory_make("rtph264depay", "depay");
    data.parse = gst_element_factory_make("h264parse", "parse");
    data.decoder = gst_element_factory_make("avdec_h264", "decoder");
    data.convert = gst_element_factory_make("videoconvert", "convert");
    data.scale = gst_element_factory_make("videoscale", "scale");
    data.sink = gst_element_factory_make("autovideosink", "sink");

    if (!pipeline || !data.source || !data.depay || !data.parse ||
        !data.decoder || !data.convert || !data.scale || !data.sink) {
        printf("Failed to create one or multiple gst elements\n");
        return -1;
    }

    /* clang-format off */
    GstCaps *caps =
        gst_caps_new_simple(VIDEO_FORMAT,
                            "width", G_TYPE_INT, FRAME_WIDTH,
                            "height", G_TYPE_INT, FRAME_HEIGHT,
                            NULL);
    /* clang-format on */

    /* clang-format off */
    g_object_set(G_OBJECT(data.source),
                 "location", RTSP_STREAM_URL,
                 "latency", 0,
                 NULL);
    /* clang-format on */

    /* Add all elements into the pipe line */
    gst_bin_add_many(GST_BIN(pipeline), data.source, data.depay, data.parse,
                     data.decoder, data.convert, data.scale, data.sink, NULL);

    /* Link all elements in the pipeline */
    if (!gst_element_link_many(data.depay, data.parse, data.decoder,
                               data.convert, data.scale, NULL)) {
        g_printerr("Failed to link elements (Stage 1)\n");
        gst_object_unref(pipeline);
        return -1;
    }

    if (!gst_element_link_filtered(data.scale, data.sink, caps)) {
        g_printerr("Failed to link elements (Stage 2)\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Attach signal handler */
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler),
                     &data);

    /* Start GStreamer */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    printf("gstreamer: Start playing...\n");

    /* Wait until received error or EOS message */
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                               GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}
