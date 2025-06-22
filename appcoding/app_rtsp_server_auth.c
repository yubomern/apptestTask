#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>
#include <glib.h>
#include <sys/inotify.h>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include "ipdevice.h" // Assumes you have a method to get the IP address of the device
#include "app_log.h"
#include <errno.h>
#include <math.h>
#include "logger.h"
#define RTSP_PORT 8553
#define CONFIG_FILE "stream_config.txt"
#define TEXT_FILE "text_overlay.txt"
#define DEFAULT_LATENCY_MS 2
#define MAX_TEXT_READER  2048
#define MAX_SIZE_FILE  1024
#define APP_LOG "service_log.log"
#define NUM_STREAMS 2


typedef struct List List  ; 

static gint flag_session_server  = 0;

struct  List 
{
    /* data */

    int node  ; 
    char  file_input[MAX_SIZE_FILE];
};

int init() {
    initLogQueue();
    logMessage("connectlog message");
    logMessage("client connetct");
    logMessage("client port runner");
    sleep(2);
    closeLogQueue();
    List li  ; 
    li.node  =  rand() % 1000000 ;
    strcpy(li.file_input ,  "logger.service"); 
    return 0;
}

typedef struct {
    gchar *uri;
    gchar *text;
    gchar *username;
    gchar *password;
    gchar *mount_point;
    gint flag_session;
} StreamConfig;

typedef struct {
    gchar *stream1;
    gchar *stream2;
    gint switch_interval;
    guint text_color;
    GMutex lock;
    gchar *config_file;
    gchar *textoverlay_file;
    gint latency_ms;
    guint port;
    GstElement *selector;
    GstElement *textoverlay;
    GstElement *src1;
    GstElement *src2;
    gboolean current_video;
    GMainLoop *loop;
    GstElement *pipeline;
    GstRTSPServer *server;
    gint current_stream_index;
    pthread_t logger_thread;
    pthread_t overlay_thread;
    pthread_t config_thread;
    pthread_t source1_thread;
    pthread_t source2_thread;
    gboolean source1_ready;
    gboolean source2_ready;
    StreamConfig stream_config ;
} AppState;
AppState app_state = {0};
static StreamConfig config = {0};
static AppState app_state_static;
static int pos ; 

void log_message_file(const char *message) {
    FILE *log_file = fopen("app.log", "a");
    if (log_file == NULL) {
        perror("Error opening log file");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[25];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fclose(log_file);
}
// Get current timestamp
int get_current_time(char *timeBuff, int buffSize) {
    if (timeBuff == NULL || buffSize <= 0) {
        g_printerr("Invalid buffer or buffer size.\n");
        return -1;
    }
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    if (strftime(timeBuff, buffSize, "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
        g_printerr("Failed to format timestamp.\n");
        return -1;
    }
    return 0;
}
int Thread_Svideo (int video ,  const char *sink , char d , const char *filename)
{
     FILE *file = NULL  ;
    file  = fopen (filename , "a+") ;

    if(!file ) {
        perror ("error  opened file ") ;
        return -1 ;
      }
      fprintf(file , "video stream %d  %s  %c \n \t " ,  video , sink ,d  ) ;
      return 0;
}


//config seesion 

static gboolean parse_config(StreamConfig *config ) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        g_printerr("Failed to open config file.\n");
        return FALSE;
    }
    const int SIZE_BYTES = 1024 ;

    char line[SIZE_BYTES];
    while (fgets(line, sizeof(line), file)) {
        if (g_str_has_prefix(line, "stream1="))
            config->uri = g_strdup(strchr(line, '=') + 1);
        else if (g_str_has_prefix(line, "text_overlay="))
            config->text = g_strdup(strchr(line, '=') + 1);
        else if (g_str_has_prefix(line, "username="))
            config->username = g_strdup(strchr(line, '=') + 1);
        else if (g_str_has_prefix(line, "password="))
            config->password = g_strdup(strchr(line, '=') + 1);
        else if (g_str_has_prefix(line, "mount="))
            config->mount_point = g_strdup(strchr(line, '=') + 1);
        else if (g_str_has_prefix(line, "flag_session="))
            config->flag_session = g_strdup(strchr(line, '=') + 1);
    }

    fclose(file);
    if (config->uri) g_strstrip(config->uri);
    if (config->text) g_strstrip(config->text);
    if (config->username) g_strstrip(config->username);
    if (config->password) g_strstrip(config->password);
    if (config->mount_point) g_strstrip(config->mount_point);

    return config->uri && config->mount_point;
}


//config gstreamer 
gboolean read_config_file(const gchar *filename, gchar **s1, gchar **s2, gint *interval, guint *color, guint *port) {
    FILE *file = fopen(filename, "r");
    if (!file) return FALSE;

    char line[MAX_TEXT_READER];
    gchar *tmp1 = NULL, *tmp2 = NULL;
    gint tmp_interval = 0;
    guint tmp_color = 0x80808080;
    guint tmp_port = 8554;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "stream1=", 8) == 0) tmp1 = g_strdup(line + 8);
        else if (strncmp(line, "stream2=", 8) == 0) tmp2 = g_strdup(line + 8);
        else if (strncmp(line, "interval=", 9) == 0) tmp_interval = atoi(line + 9);
        else if (strncmp(line, "color=", 6) == 0) tmp_color = (guint)strtoul(line + 6, NULL, 0);
        else if (strncmp(line, "port=", 5) == 0) tmp_port = (guint)strtoul(line + 5, NULL, 0);
    }
    fclose(file);

    if (tmp1) tmp1[strcspn(tmp1, "\r\n")] = 0;
    if (tmp2) tmp2[strcspn(tmp2, "\r\n")] = 0;

    if (tmp1 && tmp2 && tmp_interval > 0 && tmp_port > 0) {
        *s1 = tmp1;
        *s2 = tmp2;
        *interval = tmp_interval;
        *color = tmp_color;
        *port = tmp_port;
        return TRUE;
    }

    g_free(tmp1);
    g_free(tmp2);
    return FALSE;
}

    void cleanup_gst_elements_pipeline() {
         if(app_state.pipeline ){
                gst_element_set_state(app_state.pipeline, GST_STATE_NULL);
		        gst_object_unref(app_state.pipeline);
		        app_state.pipeline = NULL;


        }
    
}
void cleanup_gst_elements() {
    if (app_state.src1) {
        gst_element_set_state(app_state.src1, GST_STATE_NULL);
        gst_object_unref(app_state.src1);
        app_state.src1 = NULL;
    }
    if (app_state.src2) {
        gst_element_set_state(app_state.src2, GST_STATE_NULL);
        gst_object_unref(app_state.src2);
        app_state.src2 = NULL;
    }
    if (app_state.selector) {
        gst_element_set_state(app_state.selector, GST_STATE_NULL);
        gst_object_unref(app_state.selector);
        app_state.selector = NULL;
    }
    if (app_state.textoverlay) {
        gst_element_set_state(app_state.textoverlay, GST_STATE_NULL);
        gst_object_unref(app_state.textoverlay);
        app_state.textoverlay = NULL;
    }
}



// Cleanup function for GStreamer elements
void cleanup_gst_elements_test() {
    if (app_state.src1) {
        gst_object_unref(app_state.src1);
        app_state.src1 = NULL;
    }
    if (app_state.src2) {
        gst_object_unref(app_state.src2);
        app_state.src2 = NULL;
    }
    if (app_state.selector) {
        gst_object_unref(app_state.selector);
        app_state.selector = NULL;
    }
    if (app_state.textoverlay) {
        gst_object_unref(app_state.textoverlay);
        app_state.textoverlay = NULL;
    }
}

// Logger
void *logger_thread_func(void *arg) {
    while (TRUE) {
        g_mutex_lock(&app_state.lock);
        g_print("[Logger] Currently streaming: %s\n", app_state.current_video ? app_state.stream2 : app_state.stream1);
        g_mutex_unlock(&app_state.lock);
        sleep(3);
    }
    return NULL;
}



void *overlay_updater_thread_func(void *arg) {
    while (TRUE) {
        FILE *file = fopen(app_state.textoverlay_file, "r");
        if (file) {
            char buffer[MAX_TEXT_READER];
            if (fgets(buffer, sizeof(buffer), file)) {
                buffer[strcspn(buffer, "\r\n")] = 0;
                g_mutex_lock(&app_state.lock);
                if (app_state.textoverlay)
                    g_object_set(app_state.textoverlay, "text", buffer, NULL);
                g_mutex_unlock(&app_state.lock);
            }
            fclose(file);
        }
        usleep(20);
    }
    return NULL;
}

// Update stream URI safely
void update_stream_uri_v1(GstElement *uridecodebin, const gchar *uri) {
    if (!uridecodebin || !uri) return;

    GstState old_state, pending;
    GstStateChangeReturn ret = gst_element_get_state(uridecodebin, &old_state, &pending, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to get element state.\n");
        return;
    }

    ret = gst_element_set_state(uridecodebin, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_ASYNC)
        gst_element_get_state(uridecodebin, NULL, NULL, GST_CLOCK_TIME_NONE);

    g_object_set(uridecodebin, "uri", uri, NULL);

    if (old_state == GST_STATE_PLAYING || old_state == GST_STATE_PAUSED) {
        ret = gst_element_set_state(uridecodebin, old_state);
    } else {
        ret = gst_element_set_state(uridecodebin, GST_STATE_PLAYING);
    }

    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to restore element state.\n");
    }
}


// Replace video source URI live
void update_stream_uri(GstElement *uridecodebin, const gchar *uri) {
    if (!uridecodebin || !uri) return;

/*    GstState state;
    gst_element_get_state(uridecodebin, &state, NULL, GST_CLOCK_TIME_NONE);
    gst_element_set_state(uridecodebin, GST_STATE_NULL);
    g_object_set(uridecodebin, "uri", uri, NULL);
    gst_element_set_state(uridecodebin, state);*/
GstState state;
GstStateChangeReturn ret;

// Get current state of the element
ret = gst_element_get_state(uridecodebin, &state, NULL, GST_CLOCK_TIME_NONE);

// Stop the element before updating the URI
gst_element_set_state(uridecodebin, GST_STATE_NULL);

// Update the URI
g_object_set(uridecodebin, "uri", uri, NULL);

// Restore the previous state only if it was PLAYING or PAUSED
if (ret == GST_STATE_CHANGE_SUCCESS && (state == GST_STATE_PLAYING || state == GST_STATE_PAUSED)) {
    gst_element_set_state(uridecodebin, state);
} else {
    // Fallback: default to PAUSED or PLAYING
    gst_element_set_state(uridecodebin, GST_STATE_PLAYING);
}
}


// Config watcher thread
void *config_watcher_thread_func(void *arg) {
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) return NULL;
    int wd = inotify_add_watch(fd, app_state.config_file, IN_MODIFY);
    if (wd < 0) { close(fd); return NULL; }

    char buffer[1024];
    while (TRUE) {
        read(fd, buffer, sizeof(buffer));
        gchar *new_s1 = NULL, *new_s2 = NULL;
        gint new_interval;
        guint new_color;
        guint port ;

        if (read_config_file(app_state.config_file, &new_s1, &new_s2, &new_interval, &new_color,&port)) {
            g_mutex_lock(&app_state.lock);
            if (g_strcmp0(new_s1, app_state.stream1) != 0 && app_state.src1) {
                update_stream_uri(app_state.src1, new_s1);
                g_free(app_state.stream1);
                app_state.stream1 = g_strdup(new_s1);
            }

            if (g_strcmp0(new_s2, app_state.stream2) != 0 && app_state.src2) {
                update_stream_uri(app_state.src2, new_s2);
                g_free(app_state.stream2);
                app_state.stream2 = g_strdup(new_s2);
            }

            app_state.switch_interval = new_interval;

            if (app_state.textoverlay) {
                g_object_set(app_state.textoverlay, "color", new_color, NULL);
            }
            app_state.text_color = new_color;

            g_print("[Config Updated] stream1=%s, stream2=%s, interval=%d, color=0x%x\n",
                    app_state.stream1, app_state.stream2, new_interval, new_color);

            g_mutex_unlock(&app_state.lock);
        }
        sleep(1);
    }
    close(fd);
    return NULL;
}

// Handle Ctrl+C
static void handle_signal(int sig) {
    g_print("\n[Signal] Stopping server...\n");
    g_main_loop_quit(app_state.loop);
}

// Video switcher
static gboolean switch_video(gpointer data) {
    g_mutex_lock(&app_state.lock);
    if (app_state.selector) {
        const gchar *pad_name = app_state.current_video ? "sink_0" : "sink_1";
        GstPad *pad = gst_element_get_static_pad(app_state.selector, pad_name);
        if (pad) {
            g_object_set(app_state.selector, "active-pad", pad, NULL);
            g_print("[Switcher] Now playing: %s\n", app_state.current_video ? app_state.stream2 : app_state.stream1);
            app_state.current_video = !app_state.current_video;
            gst_object_unref(pad);
        }
    }
    g_mutex_unlock(&app_state.lock);
    return G_SOURCE_CONTINUE;
}



// Handle dynamic pads
static void on_pad_added_old(GstElement *src, GstPad *new_pad, gpointer data) {
    GstElement *queue = GST_ELEMENT(data);
    GstPad *sink_pad = gst_element_get_static_pad(queue, "sink");
    gst_pad_link(new_pad, sink_pad);
    gst_object_unref(sink_pad);
}

// --- Callbacks and Utilities ---

static void on_pad_added(GstElement *src, GstPad *new_pad, GstElement *downstream) {
    g_print("Pad added on '%s'. Linking to '%s'.\n", GST_ELEMENT_NAME(src), GST_ELEMENT_NAME(downstream));
    
    GstPad *sink_pad = gst_element_get_static_pad(downstream, "sink");
    if (!sink_pad) {
        g_printerr("Failed to get sink pad from downstream element.\n");
        return;
    }

    if (gst_pad_is_linked(sink_pad)) {
        g_print("Sink pad already linked. Ignoring.\n");
        gst_object_unref(sink_pad);
        return;
    }

    GstCaps *new_pad_caps = gst_pad_get_current_caps(new_pad);
    if (!new_pad_caps) {
        gst_object_unref(sink_pad);
        g_printerr("Failed to get caps from new pad.\n");
        return;
    }

    const gchar *new_pad_type = gst_structure_get_name(gst_caps_get_structure(new_pad_caps, 0));
    if (!g_str_has_prefix(new_pad_type, "video/x-raw")) {
        g_print("Pad is not raw video (%s). Ignoring.\n", new_pad_type);
        gst_caps_unref(new_pad_caps);
        gst_object_unref(sink_pad);
        return;
    }

    GstPadLinkReturn ret = gst_pad_link(new_pad, sink_pad);
    if (ret != GST_PAD_LINK_OK) {
        g_printerr("Pad linking failed (%d).\n", ret);
    }

    gst_caps_unref(new_pad_caps);
    gst_object_unref(sink_pad);
}


void* create_source_branch_thread(void* arg) {
    int index = ((int*)arg)[0];
    const gchar* uri = ((const gchar**)arg)[1];
    free(arg); // Free the allocated argument

    g_print("Creating branch for source %d: %s\n", index, uri);

    GstElement *src = NULL;
    GstElement *decodebin = gst_element_factory_make("decodebin", NULL);
    GstElement *queue = gst_element_factory_make("queue", NULL);
    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);

    if (!decodebin || !queue || !videoconvert) {
        g_printerr("Failed to create common elements for branch %d.\n", index);
        goto error;
    }

    if (g_str_has_prefix(uri, "rtsp://")) {
        src = gst_element_factory_make("rtspsrc", NULL);
        g_object_set(src, "location", uri, "latency", 500, "udp-reconnect", 1, NULL);
    } else if (g_str_has_prefix(uri, "http://") || g_str_has_prefix(uri, "https://")) {
        src = gst_element_factory_make("souphttpsrc", NULL);
        g_object_set(src, "location", uri, NULL);
    } else if (g_str_has_prefix(uri, "file://")) {
        src = gst_element_factory_make("filesrc", NULL);
        g_object_set(src, "location", uri + 7, NULL);
    } else {
        g_printerr("Unsupported URI scheme for source %d: %s\n", index, uri);
        goto error;
    }

    if (!src) {
        g_printerr("Failed to create source element for branch %d.\n", index);
        goto error;
    }

    g_mutex_lock(&app_state.lock);
    gst_bin_add_many(GST_BIN(app_state.pipeline), src, decodebin, queue, videoconvert, NULL);
    g_mutex_unlock(&app_state.lock);

    if (g_str_has_prefix(uri, "rtsp://")) {
        g_signal_connect(src, "pad-added", G_CALLBACK(on_pad_added), decodebin);
    } else {
        if (!gst_element_link(src, decodebin)) {
            g_printerr("Failed to link source to decodebin for branch %d.\n", index);
            goto error;
        }
    }

    g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_pad_added), queue);

    if (!gst_element_link(queue, videoconvert)) {
        g_printerr("Failed to link queue to videoconvert for branch %d.\n", index);
        goto error;
    }

    g_mutex_lock(&app_state.lock);
    gst_element_sync_state_with_parent(src);
    gst_element_sync_state_with_parent(decodebin);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(videoconvert);

    if (index == 1) {
        app_state.src1 = src;
        app_state.source1_ready = TRUE;
    } else {
        app_state.src2 = src;
        app_state.source2_ready = TRUE;
    }
    g_mutex_unlock(&app_state.lock);

    return NULL;

error:
    if (src) gst_object_unref(src);
    if (decodebin) gst_object_unref(decodebin);
    if (queue) gst_object_unref(queue);
    if (videoconvert) gst_object_unref(videoconvert);
    return NULL;
}

static GstElement* create_source_branch(GstElement *pipeline, const gchar *uri, gint index) {
    g_print("Creating branch for source %d: %s\n", index, uri);

    GstElement *src = NULL;
    GstElement *decodebin = gst_element_factory_make("decodebin", NULL);
    GstElement *queue = gst_element_factory_make("queue", NULL);
    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);

    if (!decodebin || !queue || !videoconvert) {
        g_printerr("Failed to create common elements for branch %d.\n", index);
        goto error;
    }

    // Determine source type based on URI
    if (g_str_has_prefix(uri, "rtsp://")) {
        src = gst_element_factory_make("rtspsrc", NULL);
        g_object_set(src, "location", uri, "latency", 500, "udp-reconnect", 1, NULL);
    } else if (g_str_has_prefix(uri, "http://") || g_str_has_prefix(uri, "https://")) {
        src = gst_element_factory_make("souphttpsrc", NULL);
        g_object_set(src, "location", uri, NULL);
    } else if (g_str_has_prefix(uri, "file://")) {
        src = gst_element_factory_make("filesrc", NULL);
        g_object_set(src, "location", uri + 7, NULL); // Skip "file://"
    } else {
        g_printerr("Unsupported URI scheme for source %d: %s\n", index, uri);
        goto error;
    }

    if (!src) {
        g_printerr("Failed to create source element for branch %d.\n", index);
        goto error;
    }

    gst_bin_add_many(GST_BIN(pipeline), src, decodebin, queue, videoconvert, NULL);

    // Link elements based on source type
    if (g_str_has_prefix(uri, "rtsp://")) {
        g_signal_connect(src, "pad-added", G_CALLBACK(on_pad_added), decodebin);
    } else {
        if (!gst_element_link(src, decodebin)) {
            g_printerr("Failed to link source to decodebin for branch %d.\n", index);
            goto error;
        }
    }

    g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_pad_added), queue);

    if (!gst_element_link(queue, videoconvert)) {
        g_printerr("Failed to link queue to videoconvert for branch %d.\n", index);
        goto error;
    }

    // Sync states
    gst_element_sync_state_with_parent(src);
    gst_element_sync_state_with_parent(decodebin);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(videoconvert);

    return videoconvert;

error:
    if (src) gst_object_unref(src);
    if (decodebin) gst_object_unref(decodebin);
    if (queue) gst_object_unref(queue);
    if (videoconvert) gst_object_unref(videoconvert);
    return NULL;
}

static gboolean switch_source(gpointer user_data) {
    g_mutex_lock(&app_state.lock);
    
    if (!app_state.selector) {
        g_mutex_unlock(&app_state.lock);
        return G_SOURCE_CONTINUE;
    }

    app_state.current_stream_index = (app_state.current_stream_index + 1) % NUM_STREAMS;
    g_print("=> SWITCHING to stream %d\n", app_state.current_stream_index);

    gchar pad_name[16];
    g_snprintf(pad_name, sizeof(pad_name), "sink_%d", app_state.current_stream_index);
    GstPad *target_pad = gst_element_get_static_pad(app_state.selector, pad_name);
    
    if (target_pad) {
        g_object_set(app_state.selector, "active-pad", target_pad, NULL);
        gst_object_unref(target_pad);
    } else {
        g_printerr("Failed to find pad '%s' for switching.\n", pad_name);
    }

    gchar overlay_text[64];
    g_snprintf(overlay_text, sizeof(overlay_text), "SOURCE %d ACTIVE", app_state.current_stream_index + 1);
    g_object_set(app_state.textoverlay, "text", overlay_text, NULL);

    g_mutex_unlock(&app_state.lock);
    return G_SOURCE_CONTINUE;
}


// Media configuration function
static void media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer user_data) {
   // GstElement *pipeline ;
    app_state.pipeline= gst_rtsp_media_get_element(media);
    g_mutex_lock(&app_state.lock);

    app_state.selector = gst_bin_get_by_name(GST_BIN(app_state.pipeline), "main_selector");
    app_state.textoverlay = gst_bin_get_by_name(GST_BIN(app_state.pipeline), "main_text");

    app_state.src1 = gst_element_factory_make("uridecodebin", "src1");
    GstElement *q1 = gst_element_factory_make("queue", "q1");
    GstElement *c1 = gst_element_factory_make("videoconvert", "c1");

    app_state.src2 = gst_element_factory_make("uridecodebin", "src2");
    GstElement *q2 = gst_element_factory_make("queue", "q2");
    GstElement *c2 = gst_element_factory_make("videoconvert", "c2");
    app_state.latency_ms = DEFAULT_LATENCY_MS;

    g_object_set(app_state.src1, "uri", app_state.stream1, NULL);
    g_object_set(app_state.src2, "uri", app_state.stream2, NULL);
    g_object_set(q1, "max-size-time", (guint64)(app_state.latency_ms * GST_MSECOND), NULL);
    g_object_set(q2, "max-size-time", (guint64)(app_state.latency_ms * GST_MSECOND), NULL);

    gst_bin_add_many(GST_BIN(app_state.pipeline), app_state.src1, q1, c1, app_state.src2, q2, c2, NULL);
    g_signal_connect(app_state.src1, "pad-added", G_CALLBACK(on_pad_added), q1);
    g_signal_connect(app_state.src2, "pad-added", G_CALLBACK(on_pad_added), q2);

    gst_element_link_many(q1, c1, NULL);
    gst_element_link_many(q2, c2, NULL);

    GstPad *srcpad1 = gst_element_get_static_pad(c1, "src");
    GstPad *sinkpad1 = gst_element_request_pad_simple(app_state.selector, "sink_%u");
    gst_pad_link(srcpad1, sinkpad1);
    gst_object_unref(srcpad1);
    gst_object_unref(sinkpad1);

    GstPad *srcpad2 = gst_element_get_static_pad(c2, "src");
    GstPad *sinkpad2 = gst_element_request_pad_simple(app_state.selector, "sink_%u");
    gst_pad_link(srcpad2, sinkpad2);
    gst_object_unref(srcpad2);
    gst_object_unref(sinkpad2);

    GstPad *initial = gst_element_get_static_pad(app_state.selector, "sink_0");
    g_object_set(app_state.selector, "active-pad", initial, NULL);
    gst_object_unref(initial);

    g_object_set(app_state.textoverlay, "color", app_state.text_color, NULL);
    g_mutex_unlock(&app_state.lock);
    gst_element_set_state(app_state.pipeline, GST_STATE_NULL);
    gst_object_unref(app_state.pipeline);
}

void* runnersession(void *arg) {
    if (!parse_config(&config)) {
        g_printerr("Error: Invalid config.\n");
        return NULL;
    }

    if (config.flag_session == 1) {
        GstRTSPAuth *auth = gst_rtsp_auth_new();
        gchar *basic = gst_rtsp_auth_make_basic(config.username, config.password);
        gst_rtsp_auth_add_basic(auth, basic, NULL);
        gst_rtsp_server_set_auth(app_state.server, auth);
        g_free(basic);
    }
    g_print("RTSP session configured.\n");
    return NULL;
}


void forking(){

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process
        FILE *fp = fopen("rtsp_server.txt", "w");
        if (fp) {
            fprintf(fp, "%d\n", getpid());
            fclose(fp);
        } else {
            perror("Failed to write PID file");
            exit(EXIT_FAILURE);
        }
}
}

// Main runner
int runner(int argc, const char *argv[]) {
    gchar *s1 = NULL, *s2 = NULL;
    gint interval;
    guint color,port;

    if (!read_config_file(app_state.config_file, &s1, &s2, &interval, &color,&port)) {
        g_printerr("Error reading config file\n");
        return EXIT_FAILURE;
    }
    g_print("port is %d" ,port ) ;




    gst_init(&argc,(char ***) &argv);

     
    g_mutex_init(&app_state.lock);
    
    signal(SIGINT, handle_signal);
    
    signal(SIGTERM, handle_signal);
    
    app_state.stream1 = s1;
    app_state.stream2 = s2;
    app_state.switch_interval = interval;
    app_state.text_color = color;
    app_state.current_video = FALSE;

    app_state.loop = g_main_loop_new(NULL, FALSE);

    app_state.server = gst_rtsp_server_new();
      g_print ("service is   %s" ,  g_strdup_printf("port is %d", port));
    if (port >0 )
       gst_rtsp_server_set_service(app_state.server, g_strdup_printf("%d", port));
    else  g_print ("no port selected ");


    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(app_state.server);
    if (!mounts) {
        g_printerr("Failed to get mount points.\n");
        cleanup_gst_elements();
        return EXIT_FAILURE;
    }
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    gst_rtsp_media_factory_set_launch(factory,
        "( input-selector name=main_selector ! "
        "videoconvert ! textoverlay name=main_text valignment=center halignment=center font-desc=\"Sans, 20\" text=\"Loading...\" ! "
        "videoscale ! video/x-raw,width=1280,height=720 ! "
        "x264enc tune=zerolatency speed-preset=ultrafast bitrate=2000 ! rtph264pay name=pay0 pt=96 )");

    gst_rtsp_media_factory_set_shared(factory, TRUE);

    gst_rtsp_media_factory_set_latency(factory, app_state.latency_ms);
    g_signal_connect(factory, "media-configure", G_CALLBACK(media_configure), NULL);
  
    if (port  > 0 )
    g_print("rtsp runner %s" ,  g_strdup_printf("/video%d", port));
   else g_print ("no port " );
   
   
     

    gst_rtsp_mount_points_add_factory(mounts , g_strdup_printf("/video%d", port), factory);
    g_object_unref(mounts);
    

    Thread_Svideo(1,"sink1", 's' , "log_server.log");

    gst_rtsp_server_attach(app_state.server, NULL);
    g_print("RTSP stream at rtsp://127.0.0.1:%d/video%d\n", port);
   

    char ip_address1[BUF_SIZE];
    get_ip_address(ip_address1);
    char *current_mounts_point  = (char  * ) malloc(sizeof(char ) * 1024 ) ;
    strcpy (current_mounts_point,"/stream") ;
    g_print("rtsp runner ip is %s  mounts  %s" ,g_strdup_printf("%s" , ip_address1),  g_strdup_printf("/video%d", port));
    //g_print("RTSP server running at rtsp://%s:%d%s\n", ip_address1, RTSP_PORT, current_mounts_point);
    if (port >0 )
    g_print("RTSP server running at rtsp://%s:/video%d\n", ip_address1, port);
    else
    g_print("RTSP server running at rtsp://%s:/video%d\n", ip_address1, RTSP_PORT);
    log_message("Application started. port ");
    
    


    g_timeout_add_seconds(app_state.switch_interval, switch_video, NULL);
    pthread_create(&app_state.logger_thread, NULL, logger_thread_func, NULL);
    pthread_create(&app_state.overlay_thread, NULL, overlay_updater_thread_func, NULL);
    pthread_create(&app_state.config_thread, NULL, config_watcher_thread_func, NULL);
    //create_source_branch(app_state.pipeline, app_state.stream1,1);
    //create_source_branch(app_state.pipeline, app_state.stream2,2);
    // Create argument for threads
    void** arg1 = malloc(2 * sizeof(void*));
    arg1[0] = (void*)1;
    arg1[1] = (void*)app_state.stream1;
    
    void** arg2 = malloc(2 * sizeof(void*));
    arg2[0] = (void*)2;
    arg2[1] = (void*)app_state.stream2;

    // Create threads for source branches
    pthread_create(&app_state.source1_thread, NULL, create_source_branch_thread, arg1);
    pthread_create(&app_state.source2_thread, NULL, create_source_branch_thread, arg2);
    /*session*/
    pthread_t session_thread;
    if (pthread_create(&session_thread, NULL, runnersession, NULL) != 0) {
            perror("Failed to create session thread");
            exit(EXIT_FAILURE);
    }
    pthread_join(session_thread, NULL);
    g_main_loop_run(app_state.loop);


    // Cleanup (on exit)
    cleanup_gst_elements();
    cleanup_gst_elements_pipeline();
    g_print("[Cleanup] Stopping server and cleaning up...\n");
    g_main_loop_unref(app_state.loop);
    g_object_unref(app_state.server);

   /* g_free(app_state.stream1);
    g_free(app_state.stream2);*/
    g_mutex_clear(&app_state.lock);


    return 0;
}


/*int  runnersession()
{
    
    if (!parse_config(&config)) {
        g_printerr("Error: Invalid config.\n");
        return -1;
    }

    if(config.flag_session ==1){
    GstRTSPAuth *auth = gst_rtsp_auth_new();
    gchar *basic = gst_rtsp_auth_make_basic(config.username, config.password);
    gst_rtsp_auth_add_basic(auth, basic, NULL);
    gst_rtsp_server_set_auth(server, auth);
    g_free(basic);

    }


}
*/



int main(int argc, char *argv[]) {

    char ip_address1[BUF_SIZE];
    get_ip_address(ip_address1);
    char mlop [BUF_SIZE] ;
    sprintf(mlop,"ip =  %s" , ip_address1);

    Thread_Svideo(2,ip_address1, 'i' , "log_server.log");
    
    if (argc < 3) {
        g_printerr("Usage: %s <stream_config.txt> <text_overlay.txt>\n", argv[0]);
        init();
        return -1;
    }
    app_state.config_file = g_strdup(argv[1]);
    app_state.textoverlay_file = g_strdup(argv[2]);
    if (!read_config_file(app_state.config_file, &app_state.stream1, &app_state.stream2,
                          &app_state.switch_interval, &app_state.text_color, &app_state.port)) {
        g_printerr("Failed to read config file %s\n", app_state.config_file);
        return -1;
    }



    g_print("RTSP server running at rtsp://localhost:8554%s\n", config.mount_point);

    

   forking();
   

    return runner(argc, (const char ** )argv);
}










 
