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
    /* RTSP source */
    GstElement *source;

    /* Branch */
    GstElement *tee;

    /* JPEG saving pipeline */
    GstElement *jpg_queue;
    GstElement *jpg_depay;
    GstElement *jpg_parse;
    GstElement *jpg_decoder;
    GstElement *jpg_convert;
    GstElement *jpg_scale;
    GstElement *jpg_encoder;
    GstElement *jpg_sink;

    /* MP4 saving pipeline */
    GstElement *mp4_queue;
    GstElement *mp4_depay;
    GstElement *mp4_parse;
    GstElement *mp4_decoder;
    GstElement *mp4_convert;
    GstElement *mp4_scale;
    GstElement *mp4_encoder;
    GstElement *mp4_mux;
    GstElement *mp4_sink;
} gst_data_t;

static GstElement *pipeline;
static bool snapshot_request = false;

static void pad_added_handler(GstElement *src, GstPad *pad, gst_data_t *data)
{
    /* Get the sink pad */
    GstPad *sink_pad = gst_element_get_static_pad(data->tee, "sink");

    /* Ignore if pad is already lined with the signal */
    if (gst_pad_is_linked(sink_pad)) {
        gst_object_unref(sink_pad);
        return;
    }

    /* Link pad with the signal */
    if (GST_PAD_LINK_FAILED(gst_pad_link(pad, sink_pad)))
        printf("Failed to link pad and signal\n");
}

void rtsp_stream_save_image(void)
{
    snapshot_request = true;
}

void generate_timestamp(char *timestamp)
{
    /* Size of the timestamp string:
     * 4 (year) + 2 (month) + 2 (day) + 2 (hour) + 2 (minute) +
     * 2 (second) + 1 (null terminator) = 15
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

        /* Obtain JPEG data */
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);

        char timestamp[15] = {0};
        generate_timestamp(timestamp);

        /* Save JPEG file */
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

void rtsp_stream_handle_eos(void)
{
    static bool end = false;

    if (!end) {
        end = true;
        printf("Sending end-of-stream (EoS), wait for termination...\n");
        gst_element_send_event(pipeline, gst_event_new_eos());
    }
}

void *rtsp_stream_saver(void *args)
{
    gst_init(NULL, NULL);

    /* Create pipeline */
    pipeline = gst_pipeline_new("rtsp-pipeline");

    /* Create elements */
    gst_data_t data;
    data.source = gst_element_factory_make("rtspsrc", "source");
    data.tee = gst_element_factory_make("tee", "tee");

    data.jpg_queue = gst_element_factory_make("queue", "jpg_queue");
    data.jpg_depay = gst_element_factory_make("rtph264depay", "jpg_depay");
    data.jpg_parse = gst_element_factory_make("h264parse", "jpg_parse");
    data.jpg_decoder = gst_element_factory_make("avdec_h264", "jpg_decoder");
    data.jpg_convert = gst_element_factory_make("videoconvert", "jpg_convert");
    data.jpg_scale = gst_element_factory_make("videoscale", "jpg_scale");
    data.jpg_encoder = gst_element_factory_make("jpegenc", "jpg_encoder");
    data.jpg_sink = gst_element_factory_make("appsink", "jpg_sink");

    data.mp4_queue = gst_element_factory_make("queue", "mp4_queue");
    data.mp4_depay = gst_element_factory_make("rtph264depay", "mp4_depay");
    data.mp4_parse = gst_element_factory_make("h264parse", "mp4_parse");
    data.mp4_decoder = gst_element_factory_make("avdec_h264", "mp4_decoder");
    data.mp4_convert = gst_element_factory_make("videoconvert", "mp4_convert");
    data.mp4_scale = gst_element_factory_make("videoscale", "mp4_scale");
    data.mp4_encoder = gst_element_factory_make("x264enc", "encoder");
    data.mp4_mux = gst_element_factory_make("mp4mux", "multiplexer");
    data.mp4_sink = gst_element_factory_make("filesink", "sink");

    if (!data.source || !data.tee || !data.jpg_queue || !data.jpg_depay ||
        !data.jpg_parse || !data.jpg_decoder || !data.jpg_convert ||
        !data.jpg_scale || !data.jpg_encoder || !data.jpg_sink ||
        !data.mp4_queue || !data.mp4_depay || !data.mp4_parse ||
        !data.mp4_decoder || !data.mp4_convert || !data.mp4_scale ||
        !data.mp4_encoder || !data.mp4_mux || !data.mp4_sink) {
        printf("Failed to create one or multiple gst elements\n");
        return NULL;
    }

    /* clang-format off */
    GstCaps *caps =
        gst_caps_new_simple(VIDEO_FORMAT,
                            "width", G_TYPE_INT, FRAME_WIDTH,
                            "height", G_TYPE_INT, FRAME_HEIGHT,
                            NULL);

    g_object_set(G_OBJECT(data.source),
                 "location", RTSP_STREAM_URL,
                 "latency", 0,
                 NULL);
    /* clang-format on */

    /* Add all elements into the pipeline */
    gst_bin_add_many(
        GST_BIN(pipeline), data.source, data.tee, data.jpg_queue,
        data.jpg_depay, data.jpg_parse, data.jpg_decoder, data.jpg_convert,
        data.jpg_scale, data.jpg_encoder, data.jpg_sink, data.mp4_queue,
        data.mp4_depay, data.mp4_parse, data.mp4_decoder, data.mp4_convert,
        data.mp4_scale, data.mp4_encoder, data.mp4_mux, data.mp4_sink, NULL);

    /*====================*
     * JPEG saving branch *
     *====================*/

    g_object_set(G_OBJECT(data.jpg_encoder), "quality", 90, NULL);
    g_object_set(G_OBJECT(data.jpg_sink), "emit-signals", TRUE, NULL);

    /* Link JPEG saving branch */
    if (!gst_element_link_many(data.tee, data.jpg_queue, data.jpg_depay,
                               data.jpg_parse, data.jpg_decoder,
                               data.jpg_convert, data.jpg_scale, NULL)) {
        g_printerr("Failed to link elements (JPEG stage 1)\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    if (!gst_element_link_filtered(data.jpg_scale, data.jpg_encoder, caps)) {
        g_printerr("Failed to link elements (JPEG stage 2)\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    if (!gst_element_link_many(data.jpg_encoder, data.jpg_sink, NULL)) {
        g_printerr("Failed to link elements (JPEG stage 3)\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    /* Attach signal handlers */
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler),
                     &data);
    g_signal_connect(data.jpg_sink, "new-sample",
                     G_CALLBACK(on_new_sample_handler), NULL);

    /*===================*
     * MP4 Saving branch *
     *===================*/

    g_object_set(G_OBJECT(data.mp4_sink), "location", "record.mp4", NULL);
    g_object_set(G_OBJECT(data.mp4_encoder), "tune", 0x00000004, NULL);

    /* Link MP4 saving branch */
    if (!gst_element_link_many(
            data.tee, data.mp4_queue, data.mp4_depay, data.mp4_parse,
            data.mp4_decoder, data.mp4_convert, data.mp4_scale,
            data.mp4_encoder, data.mp4_mux, data.mp4_sink, NULL)) {
        g_printerr("Failed to link elements (MP4 stage 1)\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    /*=================*
     * Start GStreamer *
     *=================*/

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    printf("GStreamer: Start playing...\n");

    /* Wait until received error or EOS message */
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                               GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    printf("GStreamer: Bye!\n");

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    exit(0);  // XXX

    return NULL;
}
