#include <glib.h>
#include <gst/gst.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "config.h"
#include "device.h"

#define GST_DATA(cam) ((gst_data_t *) cam->camera_priv)

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

static void generate_timestamp(char *timestamp)
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

void rtsp_save_image(struct camera_dev *cam)
{
    if (!GST_DATA(cam)->camera_ready)
        return;

    pthread_mutex_lock(&GST_DATA(cam)->snapshot_mtx);
    GST_DATA(cam)->snapshot_request = true;
    pthread_mutex_unlock(&GST_DATA(cam)->snapshot_mtx);
}

void rtsp_change_record_state(struct camera_dev *cam)
{
    if (!GST_DATA(cam)->camera_ready)
        return;

    if (GST_DATA(cam)->busy) {
        printf("[Camera %d] Error, please wait until the video is saved.\n",
               cam->id);
        return;
    }

    if (GST_DATA(cam)->recording) {
        printf("[Camera %d] Stop recording...\n", cam->id);

        /* Send end-of-stream (EOS) request */
        GST_DATA(cam)->busy = true;
        gst_element_send_event(GST_DATA(cam)->mp4_encoder, gst_event_new_eos());
    } else {
        printf("[Camera %d] Start recording...\n", cam->id);

        /* Shutdown the entire pipeline */
        gst_element_set_state(GST_DATA(cam)->pipeline, GST_STATE_NULL);
        gst_element_get_state(GST_DATA(cam)->pipeline, NULL, NULL,
                              GST_CLOCK_TIME_NONE);

        /* Assign new file name */
        char timestamp[15] = {0};
        char *save_path;
        generate_timestamp(timestamp);
        get_config_param("save_path", &save_path);
        sprintf(GST_DATA(cam)->mp4_file_name, "%s/%s.mp4", save_path,
                timestamp);
        g_object_set(G_OBJECT(GST_DATA(cam)->mp4_sink), "location",
                     GST_DATA(cam)->mp4_file_name, NULL);

        /* Redirect data flow from fake sink to the file sink */
        g_object_set(G_OBJECT(GST_DATA(cam)->mp4_osel), "active-pad",
                     GST_DATA(cam)->osel_src1, NULL);

        /* Restart the pipeline */
        gst_element_set_state(GST_DATA(cam)->pipeline, GST_STATE_PLAYING);
        gst_element_get_state(GST_DATA(cam)->pipeline, NULL, NULL,
                              GST_CLOCK_TIME_NONE);
    }

    GST_DATA(cam)->recording = !GST_DATA(cam)->recording;
}

static void *rtsp_saver(void *args)
{
    gst_data_t *gst = (gst_data_t *) args;

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

    pthread_mutex_init(&gst->snapshot_mtx, NULL);

    gst_init(NULL, NULL);

    /* Create pipeline */
    gst->pipeline = gst_pipeline_new("rtsp-pipeline");

    /*=================*
     * Create elements *
     *=================*/

    /* Source and tee components */
    gst->source = gst_element_factory_make("rtspsrc", "source");
    gst->tee = gst_element_factory_make("tee", "tee");

    /* JPEG branch components */
    gst->jpg_queue = gst_element_factory_make("queue", "jpg_queue");
    gst->jpg_depay = gst_element_factory_make(depay, "jpg_depay");
    gst->jpg_parse = gst_element_factory_make(parser, "jpg_parse");
    if (rb5_codec)
        gst->jpg_decoder = gst_element_factory_make("qtivdec", "jpg_decoder");
    else
        gst->jpg_decoder = gst_element_factory_make(decoder, "jpg_decoder");
    gst->jpg_convert = gst_element_factory_make("videoconvert", "jpg_convert");
    gst->jpg_scale = gst_element_factory_make("videoscale", "jpg_scale");
    gst->jpg_encoder = gst_element_factory_make("jpegenc", "jpg_encoder");
    gst->jpg_sink = gst_element_factory_make("appsink", "jpg_sink");

    /* MP4 branch components */
    gst->mp4_queue = gst_element_factory_make("queue", "mp4_queue");
    gst->mp4_depay = gst_element_factory_make(depay, "mp4_depay");
    gst->mp4_parse = gst_element_factory_make(parser, "mp4_parse");
    if (rb5_codec)
        gst->mp4_decoder = gst_element_factory_make("qtivdec", "mp4_decoder");
    else
        gst->mp4_decoder = gst_element_factory_make(decoder, "mp4_decoder");
    gst->mp4_convert = gst_element_factory_make("videoconvert", "mp4_convert");
    gst->mp4_scale = gst_element_factory_make("videoscale", "mp4_scale");
    gst->mp4_encoder = gst_element_factory_make("x264enc", "mp4_encoder");
    gst->mp4_mux = gst_element_factory_make("mp4mux", "mp4_mux");
    gst->mp4_osel = gst_element_factory_make("output-selector", "osel");
    gst->mp4_sink = gst_element_factory_make("filesink", "mp4_sink");
    gst->fake_sink = gst_element_factory_make("fakesink", "fake_sink");

    if (!gst->source || !gst->tee || !gst->jpg_queue || !gst->jpg_depay ||
        !gst->jpg_parse || !gst->jpg_decoder || !gst->jpg_convert ||
        !gst->jpg_scale || !gst->jpg_encoder || !gst->jpg_sink ||
        !gst->mp4_queue || !gst->mp4_depay || !gst->mp4_parse ||
        !gst->mp4_decoder || !gst->mp4_convert || !gst->mp4_scale ||
        !gst->mp4_encoder || !gst->mp4_mux || !gst->mp4_osel ||
        !gst->mp4_sink || !gst->fake_sink) {
        printf("Failed to create one or multiple gst elements\n");
        exit(1);
    }

    /* clang-format off */
    GstCaps *caps =
        gst_caps_new_simple(video_format,
                            "width", G_TYPE_INT, image_width,
                            "height", G_TYPE_INT, image_height,
                            NULL);

    g_object_set(G_OBJECT(gst->source),
                 "location", rtsp_stream_url,
                 "latency", 0,
                 NULL);
    /* clang-format on */

    /* Add all elements into the pipeline */
    gst_bin_add_many(
        GST_BIN(gst->pipeline), gst->source, gst->tee, gst->jpg_queue,
        gst->jpg_depay, gst->jpg_parse, gst->jpg_decoder, gst->jpg_convert,
        gst->jpg_scale, gst->jpg_encoder, gst->jpg_sink, gst->mp4_queue,
        gst->mp4_depay, gst->mp4_parse, gst->mp4_decoder, gst->mp4_convert,
        gst->mp4_scale, gst->mp4_encoder, gst->mp4_mux, gst->mp4_osel,
        gst->mp4_sink, gst->fake_sink, NULL);

    /*====================*
     * JPEG saving branch *
     *====================*/

    g_object_set(G_OBJECT(gst->jpg_encoder), "quality", 90, NULL);
    if (rb5_codec) {
        g_object_set(G_OBJECT(gst->jpg_decoder), "skip-frames", 1, NULL);
        g_object_set(G_OBJECT(gst->jpg_decoder), "turbo", 1, NULL);
    }
    g_object_set(G_OBJECT(gst->jpg_sink), "emit-signals", TRUE, NULL);

    /* Link JPEG saving branch */
    if (!gst_element_link_many(gst->tee, gst->jpg_queue, gst->jpg_depay,
                               gst->jpg_parse, gst->jpg_decoder,
                               gst->jpg_convert, gst->jpg_scale, NULL)) {
        g_printerr("Failed to link elements (JPEG stage 1)\n");
        gst_object_unref(gst->pipeline);
        exit(1);
    }

    if (!gst_element_link_filtered(gst->jpg_scale, gst->jpg_encoder, caps)) {
        g_printerr("Failed to link elements (JPEG stage 2)\n");
        gst_object_unref(gst->pipeline);
        exit(1);
    }

    if (!gst_element_link_many(gst->jpg_encoder, gst->jpg_sink, NULL)) {
        g_printerr("Failed to link elements (JPEG stage 3)\n");
        gst_object_unref(gst->pipeline);
        exit(1);
    }

    /* Attach signal handlers */
    g_signal_connect(gst->source, "pad-added", G_CALLBACK(pad_added_handler),
                     gst);
    g_signal_connect(gst->jpg_sink, "new-sample",
                     G_CALLBACK(on_new_sample_handler), gst);

    /*===================*
     * MP4 Saving branch *
     *===================*/

    g_object_set(G_OBJECT(gst->mp4_encoder), "tune", 0x00000004, NULL);
    if (rb5_codec) {
        g_object_set(G_OBJECT(gst->mp4_decoder), "skip-frames", 1, NULL);
        g_object_set(G_OBJECT(gst->mp4_decoder), "turbo", 1, NULL);
    }

    g_object_set(G_OBJECT(gst->mp4_sink), "location", "/tmp/.empty.mp4", NULL);

    /* Link MP4 saving branch */
    if (!gst_element_link_many(
            gst->tee, gst->mp4_queue, gst->mp4_depay, gst->mp4_parse,
            gst->mp4_decoder, gst->mp4_convert, gst->mp4_scale,
            gst->mp4_encoder, gst->mp4_mux, gst->mp4_osel, NULL)) {
        g_printerr("Failed to link elements (MP4 stage 1)\n");
        gst_object_unref(gst->pipeline);
        exit(1);
    }

    gst->osel_src1 = gst_element_get_request_pad(gst->mp4_osel, "src_%u");
    gst->osel_src2 = gst_element_get_request_pad(gst->mp4_osel, "src_%u");

    GstPad *sinkpad1 = gst_element_get_static_pad(gst->mp4_sink, "sink");
    GstPad *sinkpad2 = gst_element_get_static_pad(gst->fake_sink, "sink");

    if ((gst_pad_link(gst->osel_src1, sinkpad1) != GST_PAD_LINK_OK) ||
        (gst_pad_link(gst->osel_src2, sinkpad2) != GST_PAD_LINK_OK)) {
        printf("Failed to link output selector\n");
        exit(1);
    }

    g_object_set(G_OBJECT(gst->mp4_osel), "resend-latest", TRUE, NULL);
    g_object_set(G_OBJECT(gst->mp4_sink), "async", FALSE, NULL);
    g_object_set(G_OBJECT(gst->fake_sink), "async", FALSE, NULL);
    g_object_set(G_OBJECT(gst->mp4_sink), "sync", FALSE, NULL);
    g_object_set(G_OBJECT(gst->fake_sink), "sync", FALSE, NULL);

    g_object_set(G_OBJECT(gst->mp4_osel), "active-pad", gst->osel_src2, NULL);

    GstPad *src_pad = gst_element_get_static_pad(gst->mp4_mux, "src");
    gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                      (GstPadProbeCallback) eos_handler, gst, NULL);

    g_object_set(G_OBJECT(gst->pipeline), "message-forward", TRUE, NULL);

    /*=================*
     * Start GStreamer *
     *=================*/

    printf("GStreamer: Start playing...\n");
    gst_element_set_state(gst->pipeline, GST_STATE_PLAYING);
    gst_element_get_state(gst->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

    gst->camera_ready = true;

    GstBus *bus = gst_element_get_bus(gst->pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR);

    printf("GStreamer: Bye!\n");

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(gst->pipeline, GST_STATE_NULL);
    gst_object_unref(gst->pipeline);

    exit(0);

    return NULL;
}

void rtsp_open(struct camera_dev *cam)
{
    cam->camera_priv = malloc(sizeof(gst_data_t));
    memset(GST_DATA(cam), 0, sizeof(gst_data_t));
    GST_DATA(cam)->camera_id = cam->id;

    pthread_t gstreamer_tid;
    pthread_create(&gstreamer_tid, NULL, rtsp_saver, (void *) GST_DATA(cam));
    pthread_detach(gstreamer_tid);
}

void rtsp_close(struct camera_dev *cam)
{
    free(cam->camera_priv);
}
