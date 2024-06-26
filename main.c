// © 2024 - BestDeveloper - BestMat, Inc. - All rights reserved.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <time.h>
#include <jansson.h>

#define MAX_PAYLOAD_SIZE 65535

static const char *magicKey = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

struct session_data {
    unsigned char buf[LWS_PRE + MAX_PAYLOAD_SIZE];
    size_t len;
};

// Function to send a message
static void sendMsg(struct lws *wsi, const char *msg) {
    unsigned char *response;
    size_t len = strlen(msg);
    
    response = malloc(LWS_PRE + len + 2);
    if (!response) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    response[LWS_PRE] = 0x81;
    response[LWS_PRE + 1] = len;
    memcpy(&response[LWS_PRE + 2], msg, len);

    lws_write(wsi, &response[LWS_PRE], len + 2, LWS_WRITE_TEXT);
    free(response);
}

// Function to handle incoming messages
static int callback_websockets(struct lws *wsi, enum lws_callback_reasons reason,
                               void *user, void *in, size_t len) {
    struct session_data *data = (struct session_data *)user;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            lwsl_user("Connection established\n");
            break;
        case LWS_CALLBACK_RECEIVE:
            if (len > MAX_PAYLOAD_SIZE) {
                lwsl_err("Payload too large\n");
                return -1;
            }
            memcpy(data->buf + LWS_PRE, in, len);
            data->len = len;

            json_error_t error;
            json_t *json = json_loadb((const char *)(data->buf + LWS_PRE), data->len, 0, &error);
            if (!json) {
                lwsl_err("JSON parse error: %s\n", error.text);
                return -1;
            }

            json_t *message_json = json_object_get(json, "message");
            const char *message = json_string_value(message_json);
            if (message) {
                json_t *response_json = json_pack("{s:s, s:s}", "message", message, "at", json_string(time(NULL)));
                char *response_str = json_dumps(response_json, 0);
                sendMsg(wsi, response_str);
                free(response_str);
                json_decref(response_json);
            }
            json_decref(json);

            break;
        case LWS_CALLBACK_CLOSED:
            lwsl_user("Connection closed\n");
            break;
        default:
            break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "http", lws_callback_http_dummy, 0, 0 },
    { "websocket", callback_websockets, sizeof(struct session_data), MAX_PAYLOAD_SIZE, },
    { NULL, NULL, 0, 0 }
};

int main(void) {
    struct lws_context_creation_info info;
    struct lws_context *context;

    memset(&info, 0, sizeof info);
    info.port = 1337;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return -1;
    }

    lwsl_user("WebSocket server running on port 1337\n");
    while (lws_service(context, 1000) >= 0);

    lws_context_destroy(context);
    return 0;
}
