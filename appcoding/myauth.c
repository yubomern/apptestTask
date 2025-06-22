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

typedef struct List List; 

static gint flag_session_server = 0;

struct List {
    int node; 
    char file_input[MAX_SIZE_FILE];
};

int init() {
    initLogQueue();
    logMessage("connectlog message");
    logMessage("client connect");
    logMessage("client port runner");
    sleep(2);
    closeLogQueue();
    List li; 
    li.node = rand() % 1000000;
    strcpy(li.file_input, "logger.service"); 
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
    StreamConfig stream_config;
} AppState;

AppState app_state = {0};
static StreamConfig config = {0};
static AppState app_state_static;
static int pos; 

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

// Thread functions, configuration parsing, and other utility functions remain unchanged...

// RTSP session configuration
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

// Main runner
int runner(int argc, const char *argv[]) {
    gchar *s1 = NULL, *s2 = NULL;
    gint interval;
    guint color, port;

    if (!read_config_file(app_state.config_file, &s1, &s2, &interval, &color, &port)) {
        g_printerr("Error reading config file\n");
        return EXIT_FAILURE;
    }

    gst_init(&argc, (char ***) &argv);
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
    
    if (port > 0)
        gst_rtsp_server_set_service(app_state.server, g_strdup_printf("%d", port));
    
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(app_state.server);
    if (!mounts) {
        g_printerr("Failed to get mount points.\n");
        cleanup_gst_elements();
        return EXIT_FAILURE;
    }

    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory, "( input-selector name=main_selector ! "
        "videoconvert ! textoverlay name=main_text valignment=center halignment=center font-desc=\"Sans, 20\" text=\"Loading...\" ! "
        "videoscale ! video/x-raw,width=1280,height=720 ! "
        "x264enc tune=zerolatency speed-preset=ultrafast bitrate=2000 ! rtph264pay name=pay0 pt=96 )");

    gst_rtsp_media_factory_set_shared(factory, TRUE);
    gst_rtsp_media_factory_set_latency(factory, app_state.latency_ms);
    g_signal_connect(factory, "media-configure", G_CALLBACK(media_configure), NULL);
    
    gst_rtsp_mount_points_add_factory(mounts, g_strdup_printf("/video%d", port), factory);
    g_object_unref(mounts);

    gst_rtsp_server_attach(app_state.server, NULL);
    g_print("RTSP stream at rtsp://127.0.0.1:%d/video%d\n", port);

    // Create and run threads...
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        g_printerr("Usage: %s <stream_config.txt> <text_overlay.txt>\n", argv[0]);
        init();
        return -1;
    }

    app_state.config_file = g_strdup(argv[1]);
    app_state.textoverlay_file = g_strdup(argv[2]);

    return runner(argc, (const char **)argv);
}