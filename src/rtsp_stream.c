#include <glib.h>
#include <gst/gst.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "config.h"
#include "device.h"

typedef struct {
    int camera_id;
    bool recording;
    bool busy;
    bool snapshot_request;
    bool camera_ready;
    pthread_mutex_t snapshot_mtx;
    char mp4_file_name[PATH_MAX];

    /* Video and image saving pipeline */
    GstElement *pipeline;

    /* RTSP source */
    GstElement *source;

    /* Branch */
    GstElement *tee;

    /* JPEG saving branch */
    GstElement *jpg_queue;
    GstElement *jpg_depay;
    GstElement *jpg_parse;
    GstElement *jpg_decoder;
    GstElement *jpg_convert;
    GstElement *jpg_scale;
    GstElement *jpg_encoder;
    GstElement *jpg_sink;

    /* MP4 saving branch */
    GstElement *mp4_queue;
    GstElement *mp4_depay;
    GstElement *mp4_parse;
    GstElement *mp4_decoder;
    GstElement *mp4_convert;
    GstElement *mp4_scale;
    GstElement *mp4_encoder;
    GstElement *mp4_mux;
    GstPad *osel_src1;
    GstPad *osel_src2;
    GstElement *mp4_osel;
    GstElement *mp4_sink;
    GstElement *fake_sink;
} gst_data_t;

gst_data_t data;

static GstPadProbeReturn eos_handler(GstPad *pad,
                                     GstPadProbeInfo *info,
                                     gst_data_t *data)
{
    if (GST_EVENT_TYPE(info->data) != GST_EVENT_EOS)
        return GST_PAD_PROBE_OK;

    /* Video saving request */
    if (data->busy == true) {
        data->busy = false;
        printf("[Camera %d] %s is saved!\n", data->camera_id,
               data->mp4_file_name);
        g_object_set(G_OBJECT(data->mp4_osel), "active-pad", data->osel_src2,
                     NULL);
    }

    return GST_PAD_PROBE_OK;
}

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

static void on_new_sample_handler(GstElement *sink, gst_data_t *data)
{
    /* Retrieve the frame data */
    GstSample *sample;
    g_signal_emit_by_name(sink, "pull-sample", &sample, NULL);

    if (sample && data->snapshot_request) {
        data->snapshot_request = false;

        /* Obtain JPEG data */
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);

        char timestamp[15] = {0};
        generate_timestamp(timestamp);

        /* Save JPEG file */
        char filename[PATH_MAX];
        char *save_path;
        get_config_param("save_path", &save_path);
        snprintf(filename, sizeof(filename), "%s/%s.jpg", save_path, timestamp);
        FILE *file = fopen(filename, "wb");

        printf("[Camera %d] %s is saved!\n", data->camera_id, filename);
        fwrite(map.data, 1, map.size, file);
        fclose(file);

        /* Release resources */
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
    }
}

void rtsp_stream_save_image(struct camera_dev *cam)
{
    if (!data.camera_ready)
        return;

    pthread_mutex_lock(&data.snapshot_mtx);
    data.snapshot_request = true;
    pthread_mutex_unlock(&data.snapshot_mtx);
}

void rtsp_stream_change_record_state(struct camera_dev *cam)
{
    if (!data.camera_ready)
        return;

    if (data.busy) {
        printf("[Camera %d] Error, please wait until the video is saved.\n",
               cam->id);
        return;
    }

    if (data.recording) {
        printf("[Camera %d] Stop recording...\n", cam->id);

        /* Send end-of-stream (EOS) request */
        data.busy = true;
        gst_element_send_event(data.mp4_encoder, gst_event_new_eos());
    } else {
        printf("[Camera %d] Start recording...\n", cam->id);

        /* Shutdown the entire pipeline */
        gst_element_set_state(data.pipeline, GST_STATE_NULL);
        gst_element_get_state(data.pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

        /* Assign new file name */
        char timestamp[15] = {0};
        char *save_path;
        generate_timestamp(timestamp);
        get_config_param("save_path", &save_path);
        sprintf(data.mp4_file_name, "%s/%s.mp4", save_path, timestamp);
        g_object_set(G_OBJECT(data.mp4_sink), "location", data.mp4_file_name,
                     NULL);

        /* Redirect data flow from fake sink to the file sink */
        g_object_set(G_OBJECT(data.mp4_osel), "active-pad", data.osel_src1,
                     NULL);

        /* Restart the pipeline */
        gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
        gst_element_get_state(data.pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
    }

    data.recording = !data.recording;
}

void *rtsp_stream_saver(void *args)
{
    char *codec = "";
    get_config_param("codec", &codec);

    if (strcmp("h264", codec) && strcmp("h265", codec)) {
        fprintf(stderr, "Invalid codec \"%s\"\n", codec);
        exit(1);
    }

    char depay[20];
    char parser[20];
    char decoder[20];
    snprintf(depay, sizeof(depay), "rtp%sdepay", codec);
    snprintf(parser, sizeof(parser), "%sparse", codec);
    snprintf(decoder, sizeof(decoder), "avdec_%s", codec);

    char *board_name = "";
    get_config_param("board", &board_name);

    bool rb5_codec = false;
    if (strcmp("rb5", board_name) == 0) {
        rb5_codec = true;
        printf("Qualcomm RB5 acceleration enabled\n");
    }

    char *rtsp_stream_url = "";
    get_config_param("rtsp_stream_url", &rtsp_stream_url);

    char *video_format = "";
    get_config_param("video_format", &video_format);

    int image_width = 0, image_height = 0;
    get_config_param("image_width", &image_width);
    get_config_param("image_height", &image_height);

    pthread_mutex_init(&data.snapshot_mtx, NULL);

    gst_init(NULL, NULL);

    /* Create pipeline */
    data.pipeline = gst_pipeline_new("rtsp-pipeline");

    /*=================*
     * Create elements *
     *=================*/

    /* Source and tee components */
    data.source = gst_element_factory_make("rtspsrc", "source");
    data.tee = gst_element_factory_make("tee", "tee");

    /* JPEG branch components */
    data.jpg_queue = gst_element_factory_make("queue", "jpg_queue");
    data.jpg_depay = gst_element_factory_make(depay, "jpg_depay");
    data.jpg_parse = gst_element_factory_make(parser, "jpg_parse");
    if (rb5_codec)
        data.jpg_decoder = gst_element_factory_make("qtivdec", "jpg_decoder");
    else
        data.jpg_decoder = gst_element_factory_make(decoder, "jpg_decoder");
    data.jpg_convert = gst_element_factory_make("videoconvert", "jpg_convert");
    data.jpg_scale = gst_element_factory_make("videoscale", "jpg_scale");
    data.jpg_encoder = gst_element_factory_make("jpegenc", "jpg_encoder");
    data.jpg_sink = gst_element_factory_make("appsink", "jpg_sink");

    /* MP4 branch components */
    data.mp4_queue = gst_element_factory_make("queue", "mp4_queue");
    data.mp4_depay = gst_element_factory_make(depay, "mp4_depay");
    data.mp4_parse = gst_element_factory_make(parser, "mp4_parse");
    if (rb5_codec)
        data.mp4_decoder = gst_element_factory_make("qtivdec", "mp4_decoder");
    else
        data.mp4_decoder = gst_element_factory_make(decoder, "mp4_decoder");
    data.mp4_convert = gst_element_factory_make("videoconvert", "mp4_convert");
    data.mp4_scale = gst_element_factory_make("videoscale", "mp4_scale");
    data.mp4_encoder = gst_element_factory_make("x264enc", "mp4_encoder");
    data.mp4_mux = gst_element_factory_make("mp4mux", "mp4_mux");
    data.mp4_osel = gst_element_factory_make("output-selector", "osel");
    data.mp4_sink = gst_element_factory_make("filesink", "mp4_sink");
    data.fake_sink = gst_element_factory_make("fakesink", "fake_sink");

    if (!data.source || !data.tee || !data.jpg_queue || !data.jpg_depay ||
        !data.jpg_parse || !data.jpg_decoder || !data.jpg_convert ||
        !data.jpg_scale || !data.jpg_encoder || !data.jpg_sink ||
        !data.mp4_queue || !data.mp4_depay || !data.mp4_parse ||
        !data.mp4_decoder || !data.mp4_convert || !data.mp4_scale ||
        !data.mp4_encoder || !data.mp4_mux || !data.mp4_osel ||
        !data.mp4_sink || !data.fake_sink) {
        printf("Failed to create one or multiple gst elements\n");
        exit(1);
    }

    /* clang-format off */
    GstCaps *caps =
        gst_caps_new_simple(video_format,
                            "width", G_TYPE_INT, image_width,
                            "height", G_TYPE_INT, image_height,
                            NULL);

    g_object_set(G_OBJECT(data.source),
                 "location", rtsp_stream_url,
                 "latency", 0,
                 NULL);
    /* clang-format on */

    /* Add all elements into the pipeline */
    gst_bin_add_many(
        GST_BIN(data.pipeline), data.source, data.tee, data.jpg_queue,
        data.jpg_depay, data.jpg_parse, data.jpg_decoder, data.jpg_convert,
        data.jpg_scale, data.jpg_encoder, data.jpg_sink, data.mp4_queue,
        data.mp4_depay, data.mp4_parse, data.mp4_decoder, data.mp4_convert,
        data.mp4_scale, data.mp4_encoder, data.mp4_mux, data.mp4_osel,
        data.mp4_sink, data.fake_sink, NULL);

    /*====================*
     * JPEG saving branch *
     *====================*/

    g_object_set(G_OBJECT(data.jpg_encoder), "quality", 90, NULL);
    if (rb5_codec) {
        g_object_set(G_OBJECT(data.jpg_decoder), "skip-frames", 1, NULL);
        g_object_set(G_OBJECT(data.jpg_decoder), "turbo", 1, NULL);
    }
    g_object_set(G_OBJECT(data.jpg_sink), "emit-signals", TRUE, NULL);

    /* Link JPEG saving branch */
    if (!gst_element_link_many(data.tee, data.jpg_queue, data.jpg_depay,
                               data.jpg_parse, data.jpg_decoder,
                               data.jpg_convert, data.jpg_scale, NULL)) {
        g_printerr("Failed to link elements (JPEG stage 1)\n");
        gst_object_unref(data.pipeline);
        exit(1);
    }

    if (!gst_element_link_filtered(data.jpg_scale, data.jpg_encoder, caps)) {
        g_printerr("Failed to link elements (JPEG stage 2)\n");
        gst_object_unref(data.pipeline);
        exit(1);
    }

    if (!gst_element_link_many(data.jpg_encoder, data.jpg_sink, NULL)) {
        g_printerr("Failed to link elements (JPEG stage 3)\n");
        gst_object_unref(data.pipeline);
        exit(1);
    }

    /* Attach signal handlers */
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler),
                     &data);
    g_signal_connect(data.jpg_sink, "new-sample",
                     G_CALLBACK(on_new_sample_handler), &data);

    /*===================*
     * MP4 Saving branch *
     *===================*/

    g_object_set(G_OBJECT(data.mp4_encoder), "tune", 0x00000004, NULL);
    if (rb5_codec) {
        g_object_set(G_OBJECT(data.mp4_decoder), "skip-frames", 1, NULL);
        g_object_set(G_OBJECT(data.mp4_decoder), "turbo", 1, NULL);
    }

    g_object_set(G_OBJECT(data.mp4_sink), "location", "/tmp/.empty.mp4", NULL);

    /* Link MP4 saving branch */
    if (!gst_element_link_many(
            data.tee, data.mp4_queue, data.mp4_depay, data.mp4_parse,
            data.mp4_decoder, data.mp4_convert, data.mp4_scale,
            data.mp4_encoder, data.mp4_mux, data.mp4_osel, NULL)) {
        g_printerr("Failed to link elements (MP4 stage 1)\n");
        gst_object_unref(data.pipeline);
        exit(1);
    }

    data.osel_src1 = gst_element_get_request_pad(data.mp4_osel, "src_%u");
    data.osel_src2 = gst_element_get_request_pad(data.mp4_osel, "src_%u");

    GstPad *sinkpad1 = gst_element_get_static_pad(data.mp4_sink, "sink");
    GstPad *sinkpad2 = gst_element_get_static_pad(data.fake_sink, "sink");

    if ((gst_pad_link(data.osel_src1, sinkpad1) != GST_PAD_LINK_OK) ||
        (gst_pad_link(data.osel_src2, sinkpad2) != GST_PAD_LINK_OK)) {
        printf("Failed to link output selector\n");
        exit(1);
    }

    g_object_set(G_OBJECT(data.mp4_osel), "resend-latest", TRUE, NULL);
    g_object_set(G_OBJECT(data.mp4_sink), "async", FALSE, NULL);
    g_object_set(G_OBJECT(data.fake_sink), "async", FALSE, NULL);
    g_object_set(G_OBJECT(data.mp4_sink), "sync", FALSE, NULL);
    g_object_set(G_OBJECT(data.fake_sink), "sync", FALSE, NULL);

    g_object_set(G_OBJECT(data.mp4_osel), "active-pad", data.osel_src2, NULL);

    GstPad *src_pad = gst_element_get_static_pad(data.mp4_mux, "src");
    gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                      (GstPadProbeCallback) eos_handler, &data, NULL);

    g_object_set(G_OBJECT(data.pipeline), "message-forward", TRUE, NULL);

    /*=================*
     * Start GStreamer *
     *=================*/

    printf("GStreamer: Start playing...\n");
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    gst_element_get_state(data.pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

    data.camera_ready = true;

    GstBus *bus = gst_element_get_bus(data.pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR);

    printf("GStreamer: Bye!\n");

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);

    exit(0);

    return NULL;
}
