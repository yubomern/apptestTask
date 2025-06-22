#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    int flag_session;
    const char *username;
    const char *password;
} Config;

Config config = {
    .flag_session = 1,
    .username = "admin",
    .password = "admin"
};

GstRTSPServer *server;

// Dummy config parser (always returns true in this example)
gboolean parse_config(Config *cfg) {
    return TRUE; // Simulate successful config parsing
}

// The runner session function
void* runnersession(void *arg) {
    if (!parse_config(&config)) {
        g_printerr("Error: Invalid config.\n");
        return NULL;
    }

    if (config.flag_session == 1) {
        GstRTSPAuth *auth = gst_rtsp_auth_new();
        gchar *basic = gst_rtsp_auth_make_basic(config.username, config.password);
        gst_rtsp_auth_add_basic(auth, basic, NULL);
        gst_rtsp_server_set_auth(server, auth);
        g_free(basic);
    }

    g_print("RTSP session configured.\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process
        FILE *fp = fopen("/tmp/rtsp_server.pid", "w");
        if (fp) {
            fprintf(fp, "%d\n", getpid());
            fclose(fp);
        } else {
            perror("Failed to write PID file");
            exit(EXIT_FAILURE);
        }

        server = gst_rtsp_server_new();

        // Launch the runnersession in a new thread
        pthread_t session_thread;
        if (pthread_create(&session_thread, NULL, runnersession, NULL) != 0) {
            perror("Failed to create session thread");
            exit(EXIT_FAILURE);
        }

        gst_rtsp_server_attach(server, NULL);

        GMainLoop *loop = g_main_loop_new(NULL, FALSE);
        g_print("Starting RTSP server...\n");
        g_main_loop_run(loop);

        pthread_join(session_thread, NULL);
        g_main_loop_unref(loop);
    } else {
        // Parent process
        printf("Started RTSP child process with PID: %d\n", pid);
    }

    return 0;
}
