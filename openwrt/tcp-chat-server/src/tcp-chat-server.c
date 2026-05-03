/*
 * TCP chat server for OpenWrt using libubox uloop, usock, and ustream.
 * Listens on port 5022, greets clients with "ping!", expects lines ending in CR,
 * echoes each line back with CRLF. SIGHUP disconnects clients with "ending session"
 * and keeps accepting new connections; SIGINT and SIGTERM disconnect clients, shut
 * down the listener, and exit.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <libubox/list.h>
#include <libubox/uloop.h>
#include <libubox/usock.h>
#include <libubox/ustream.h>

#define LISTEN_HOST "0.0.0.0"
#define LISTEN_PORT "5022"

static const char ping_msg[] = "ping!\r\n";
static const char shutdown_msg[] = "ending session\r\n";

static struct uloop_fd server;
static LIST_HEAD(clients);

struct client {
	struct list_head head;
	struct sockaddr_in sin;
	struct ustream_fd s;
};

static void close_client(struct client *cl)
{
	list_del(&cl->head);
	ustream_free(&cl->s.stream);
	close(cl->s.fd.fd);
	free(cl);
}

static void disconnect_all_clients(void)
{
	struct client *cl, *tmp;

	list_for_each_entry_safe(cl, tmp, &clients, head) {
		(void)write(cl->s.fd.fd, shutdown_msg, sizeof(shutdown_msg) - 1);
		close_client(cl);
	}
}

static void client_notify_state(struct ustream *s)
{
	struct client *cl = container_of(s, struct client, s.stream);

	if (!s->eof)
		return;

	if (!s->w.data_bytes)
		close_client(cl);
}

static void client_read_cb(struct ustream *s, int bytes_new)
{
	char *data;
	char *cr;

	(void)bytes_new;

	for (;;) {
		data = ustream_get_read_buf(s, NULL);
		if (!data)
			break;

		cr = strchr(data, '\r');
		if (!cr)
			break;

		ustream_printf(s, "%.*s\r\n", (int)(cr - data), data);
		ustream_consume(s, (int)(cr + 1 - data));
	}
}

static void server_cb(struct uloop_fd *fd, unsigned int events)
{
	struct client *cl;
	socklen_t sl;
	int sfd;

	(void)fd;
	(void)events;

	cl = calloc(1, sizeof(*cl));
	if (!cl) {
		fprintf(stderr, "out of memory\n");
		return;
	}

	sl = sizeof(cl->sin);
	sfd = accept(server.fd, (struct sockaddr *)&cl->sin, &sl);
	if (sfd < 0) {
		free(cl);
		perror("accept");
		return;
	}

	list_add_tail(&cl->head, &clients);

	cl->s.stream.string_data = true;
	cl->s.stream.notify_read = client_read_cb;
	cl->s.stream.notify_state = client_notify_state;
	ustream_fd_init(&cl->s, sfd);

	ustream_write(&cl->s.stream, ping_msg, sizeof(ping_msg) - 1, false);
}

static void on_sighup(struct uloop_signal *sig)
{
	(void)sig;
	disconnect_all_clients();
}

static void terminate_server(void)
{
	disconnect_all_clients();

	uloop_fd_delete(&server);
	if (server.fd >= 0) {
		close(server.fd);
		server.fd = -1;
	}

	uloop_end();
}

static void on_sigint(struct uloop_signal *sig)
{
	(void)sig;
	terminate_server();
}

static void on_sigterm(struct uloop_signal *sig)
{
	(void)sig;
	terminate_server();
}

static struct uloop_signal sighup_watcher = {
	.cb = on_sighup,
	.signo = SIGHUP,
};

static struct uloop_signal sigint_watcher = {
	.cb = on_sigint,
	.signo = SIGINT,
};

static struct uloop_signal sigterm_watcher = {
	.cb = on_sigterm,
	.signo = SIGTERM,
};

int main(void)
{
	int ret;

	if (uloop_init() < 0) {
		fprintf(stderr, "uloop_init failed\n");
		return EXIT_FAILURE;
	}

	signal(SIGPIPE, SIG_IGN);

	server.cb = server_cb;
	server.fd = usock(USOCK_TCP | USOCK_SERVER | USOCK_IPV4ONLY | USOCK_NUMERIC,
			  LISTEN_HOST, LISTEN_PORT);
	if (server.fd < 0) {
		perror("usock");
		uloop_done();
		return EXIT_FAILURE;
	}

	ret = uloop_fd_add(&server, ULOOP_READ);
	if (ret < 0) {
		fprintf(stderr, "uloop_fd_add: %s\n", strerror(errno));
		close(server.fd);
		uloop_done();
		return EXIT_FAILURE;
	}

	ret = uloop_signal_add(&sighup_watcher);
	if (ret < 0) {
		fprintf(stderr, "uloop_signal_add (SIGHUP): %s\n", strerror(errno));
		uloop_fd_delete(&server);
		close(server.fd);
		uloop_done();
		return EXIT_FAILURE;
	}

	ret = uloop_signal_add(&sigint_watcher);
	if (ret < 0) {
		fprintf(stderr, "uloop_signal_add (SIGINT): %s\n", strerror(errno));
		uloop_signal_delete(&sighup_watcher);
		uloop_fd_delete(&server);
		close(server.fd);
		uloop_done();
		return EXIT_FAILURE;
	}

	ret = uloop_signal_add(&sigterm_watcher);
	if (ret < 0) {
		fprintf(stderr, "uloop_signal_add (SIGTERM): %s\n", strerror(errno));
		uloop_signal_delete(&sigint_watcher);
		uloop_signal_delete(&sighup_watcher);
		uloop_fd_delete(&server);
		close(server.fd);
		uloop_done();
		return EXIT_FAILURE;
	}

	uloop_run();

	uloop_done();
	return EXIT_SUCCESS;
}
