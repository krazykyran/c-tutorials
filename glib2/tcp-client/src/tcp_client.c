/*
 * Minimal synchronous TCP client using GLib GIO (GSocketClient).
 * Suitable for embedded Linux when GLib is already in the rootfs/SDK.
 *
 * Build (native):  make
 * Run:              ./tcp_client 127.0.0.1 7
 *                    (port 7 = echo on many systems; use your server port)
 *
 * Cross-compile:    make CC=arm-linux-gnueabihf-gcc PKG_CONFIG=arm-linux-gnueabihf-pkg-config
 */

#include <glib.h>
#include <gio/gio.h>

#define LINE_BUF_SIZE 4096

static gboolean
write_all(GOutputStream *ostream, const void *data, gsize len, GError **error)
{
    return g_output_stream_write_all(ostream, data, len, NULL, NULL, error);
}

int main(int argc, char **argv)
{
    const gchar *host;
    guint16 port;
    GError *error = NULL;
    GSocketClient *client = NULL;
    GSocketConnection *conn = NULL;
    GOutputStream *ostream = NULL;
    GInputStream *istream = NULL;
    gint exit_code = 0;

    if (argc != 3) {
        g_printerr("Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    host = argv[1];
    port = (guint16)g_ascii_strtoull(argv[2], NULL, 10);
    if (port == 0) {
        g_printerr("Invalid port: %s\n", argv[2]);
        return 1;
    }

    client = g_socket_client_new();
    /* Reasonable timeout for slow links; tune for your embedded network */
    g_socket_client_set_timeout(client, 15);

    conn = g_socket_client_connect_to_host(client, host, port, NULL, &error);
    if (conn == NULL) {
        g_printerr("connect %s:%u failed: %s\n", host, port, error->message);
        g_clear_error(&error);
        exit_code = 1;
        goto out;
    }

    ostream = g_io_stream_get_output_stream(G_IO_STREAM(conn));
    istream = g_io_stream_get_input_stream(G_IO_STREAM(conn));

    /* Example payload; replace with your protocol */
    {
        static const gchar msg[] = "ping\n";
        if (!write_all(ostream, msg, sizeof(msg) - 1, &error)) {
            g_printerr("write failed: %s\n", error->message);
            g_clear_error(&error);
            exit_code = 1;
            goto out;
        }
    }

    /* Read one chunk from server (blocking until data, EOF, or error) */
    {
        gchar buf[LINE_BUF_SIZE];
        gssize n = g_input_stream_read(istream, buf, sizeof(buf) - 1, NULL, &error);

        if (n < 0) {
            g_printerr("read failed: %s\n", error->message);
            g_clear_error(&error);
            exit_code = 1;
            goto out;
        }
        if (n == 0) {
            g_print("(peer closed without sending data)\n");
        } else {
            buf[n] = '\0';
            g_print("%s", buf);
        }
    }

out:
    if (conn != NULL)
        g_object_unref(conn);
    if (client != NULL)
        g_object_unref(client);

    return exit_code;
}
