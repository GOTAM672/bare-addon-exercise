#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <bare.h>
#include <js.h>

typedef struct {
    js_env_t *env;
    js_deferred_t *deferred;

    uv_tcp_t socket;
    uv_connect_t connect_req;
    uv_write_t write_req;

    char *write_buffer;
    size_t write_len;

    char *response_data;
    size_t response_len;
    size_t response_capacity;

    char alloc_buffer[65536];
} addon_context_t;

static void free_context (addon_context_t *ctx) {
    if (!ctx)
      return;

    if (ctx->write_buffer)
      free (ctx->write_buffer);

    if (ctx->response_data)
      free (ctx->response_data);

    free (ctx);
}

static void on_close_ctx (uv_handle_t *handle) {
    addon_context_t *ctx = (addon_context_t *)handle->data;
    free_context (ctx);
}

static void reject_and_cleanup (addon_context_t *ctx, const char *err_msg) {
    js_handle_scope_t *scope;
    js_open_handle_scope (ctx->env, &scope);

    js_value_t *error_msg, *error_obj;
    js_create_string_utf8 (ctx->env, (const uint8_t *)err_msg, -1, &error_msg);
    js_create_error (ctx->env, NULL, error_msg, &error_obj);
    js_reject_deferred (ctx->env, ctx->deferred, error_obj);

    js_close_handle_scope (ctx->env, scope);
    
    if (!uv_is_closing ((uv_handle_t *)&ctx->socket)) {
        uv_close ((uv_handle_t *)&ctx->socket, on_close_ctx);
    } else {
        free_context (ctx);
    }
}

static void on_alloc (uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    addon_context_t *ctx = (addon_context_t *)handle->data;
    buf->base = ctx->alloc_buffer;
    buf->len = sizeof(ctx->alloc_buffer);
}

static void on_read (uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    addon_context_t *ctx = (addon_context_t *)stream->data;

    if (nread > 0) {
        if (ctx->response_len + nread > ctx->response_capacity) {
            ctx->response_capacity = (ctx->response_len + nread) * 2;
            ctx->response_data = realloc (ctx->response_data, ctx->response_capacity);
        }
        memcpy (ctx->response_data + ctx->response_len, buf->base, nread);
        ctx->response_len += nread;
    } else if (nread < 0) {
        if (nread == UV_EOF) {
            js_handle_scope_t *scope;
            js_open_handle_scope (ctx->env, &scope);

            js_value_t *js_buffer;
            void *js_memory = NULL;

            int err = js_create_arraybuffer (ctx->env, ctx->response_len, &js_memory, &js_buffer);
            if (err == 0 && js_memory && ctx->response_len > 0) {
                memcpy (js_memory, ctx->response_data, ctx->response_len);

                js_value_t *typed_array;
                js_create_typedarray (ctx->env, js_uint8array, ctx->response_len, js_buffer, 0, &typed_array);
                js_resolve_deferred (ctx->env, ctx->deferred, typed_array);
            } else if (ctx->response_len == 0) {
                js_create_arraybuffer (ctx->env, 0, &js_memory, &js_buffer);
                js_value_t *typed_array;
                js_create_typedarray (ctx->env, js_uint8array, 0, js_buffer, 0, &typed_array);
                js_resolve_deferred (ctx->env, ctx->deferred, typed_array);
            } else {
                js_close_handle_scope (ctx->env, scope);
                reject_and_cleanup (ctx, "Failed initializing host memory ArrayBuffer allocation");
                return;
            }
            
            js_close_handle_scope (ctx->env, scope);
            
            uv_read_stop (stream);
            uv_close ((uv_handle_t *)&ctx->socket, on_close_ctx);
        } else {
            reject_and_cleanup (ctx, uv_err_name(nread));
        }
    }
}

static void on_write (uv_write_t *req, int status) {
    addon_context_t *ctx = (addon_context_t *)req->data;
    if (status < 0) {
        reject_and_cleanup (ctx, "Transmission segment upload sequence dropped");
        return;
    }
    int err = uv_read_start ((uv_stream_t *)&ctx->socket, on_alloc, on_read);
    if (err < 0) {
        reject_and_cleanup (ctx, "Stream reader lifecycle instantiation failed");
    }
}

static void on_connect (uv_connect_t *req, int status) {
    addon_context_t *ctx = (addon_context_t *)req->data;
    if (status < 0) {
        reject_and_cleanup (ctx, "Target network node handshake refused");
        return;
    }
    uv_buf_t buf = uv_buf_init (ctx->write_buffer, ctx->write_len);
    int err = uv_write (&ctx->write_req, (uv_stream_t *)&ctx->socket, &buf, 1, on_write);
    if (err < 0) {
        reject_and_cleanup (ctx, "Pipeline stream payload write step failed");
    }
}

static js_value_t *native_tcp_cat (js_env_t *env, js_callback_info_t *info) {
    size_t argc = 3;
    js_value_t *argv[3];
    js_get_callback_info (env, info, &argc, argv, NULL, NULL);
    
    char ip[64];
    js_get_value_string_utf8 (env, argv[0], (uint8_t *)ip, sizeof(ip), NULL);
    
    int32_t port;
    js_get_value_int32 (env, argv[1], &port);
    
    void *payload_data = NULL;
    size_t payload_len = 0;
    js_get_typedarray_info (env, argv[2], NULL, &payload_data, &payload_len, NULL, NULL);
    
    addon_context_t *ctx = calloc (1, sizeof(addon_context_t));
    ctx->env = env;
    ctx->write_len = payload_len;
    ctx->write_buffer = malloc(payload_len);
    memcpy (ctx->write_buffer, payload_data, payload_len);
    
    ctx->response_capacity = 4096;
    ctx->response_data = malloc (ctx->response_capacity);
    
    js_value_t *promise;
    js_create_promise (env, &ctx->deferred, &promise);
    
    uv_loop_t *loop;
    js_get_env_loop (env, &loop);
    
    uv_tcp_init (loop, &ctx->socket);
    ctx->socket.data = ctx;
    ctx->connect_req.data = ctx;
    ctx->write_req.data = ctx;
    
    struct sockaddr_in dest;
    int err = uv_ip4_addr (ip, port, &dest);
    if (err < 0) {
        reject_and_cleanup (ctx, "Invalid layout structure configuration for target IP");
        return promise;
    }
    
    err = uv_tcp_connect (&ctx->connect_req, &ctx->socket, (const struct sockaddr *)&dest, on_connect);
    if (err < 0) {
        reject_and_cleanup (ctx, "Socket layer assignment instantiation failed");
    }
    
    return promise;
}

static js_value_t *init (js_env_t *env, js_value_t *exports) {
    js_value_t *fn;
    js_create_function (env, "tcpCat", -1, native_tcp_cat, NULL, &fn);
    js_set_named_property (env, exports, "tcpCat", fn);
    return exports;
}

BARE_MODULE (bare_addon_exercise, init)
