#include <libwebsockets.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OBS_WEBSOCKET_URL "ws://localhost:4455"
#define PASSWORD "your_password"

static int callback_ws(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("[OBS] Connected to WebSocket\n");
            // Send authentication payload
            char auth_payload[512];
            snprintf(auth_payload, sizeof(auth_payload),
                     "{\"op\":1,\"d\":{\"rpcVersion\":1,\"authentication\":\"%s\"}}", PASSWORD);
            lws_write(wsi, (unsigned char *)auth_payload + LWS_PRE, strlen(auth_payload), LWS_WRITE_TEXT);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("[OBS] Received: %s\n", (char *)in);

            // Parse JSON message
            cJSON *root = cJSON_Parse((char *)in);
            if (!root) {
                printf("Error parsing JSON\n");
                break;
            }

            // Extract stats if present
            cJSON *op = cJSON_GetObjectItem(root, "op");
            if (op && op->valueint == 0) {  // A successful response
                cJSON *d = cJSON_GetObjectItem(root, "d");
                if (d) {
                    cJSON *stats = cJSON_GetObjectItem(d, "stats");
                    if (stats) {
                        int fps = cJSON_GetObjectItem(stats, "fps")->valueint;
                        int dropped_frames = cJSON_GetObjectItem(stats, "outputSkippedFrames")->valueint;
                        printf("FPS: %d, Dropped Frames: %d\n", fps, dropped_frames);
                    }
                }
            }

            cJSON_Delete(root);
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf("[OBS] Connection Error\n");
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("[OBS] Connection Closed\n");
            break;

        default:
            break;
    }
    return 0;
}

int main() {
    struct lws_context_creation_info info;
    struct lws_client_connect_info ccinfo;
    struct lws_context *context;
    struct lws *client_wsi;

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN; // No server, just a client
    info.protocols = (struct lws_protocols[]) {
        { "ws", callback_ws, 0, 0 }, { NULL, NULL, 0, 0 }
    };

    context = lws_create_context(&info);
    if (!context) {
        printf("Failed to create WebSocket context\n");
        return -1;
    }

    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = "localhost";
    ccinfo.port = 4455;
    ccinfo.path = "/";
    ccinfo.protocol = "ws";

    client_wsi = lws_client_connect_via_info(&ccinfo);
    if (!client_wsi) {
        printf("Failed to connect to OBS WebSocket\n");
        lws_context_destroy(context);
        return -1;
    }

    printf("Connecting to OBS...\n");
    while (lws_service(context, 1000) >= 0);

    lws_context_destroy(context);
    return 0;
}
