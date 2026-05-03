/*
 * Minimal libubox uloop demo for OpenWrt.
 * Runs a periodic timer, prints a few ticks, then exits the event loop.
 */
#include <stdio.h>
#include <stdlib.h>
#include <libubox/uloop.h>

#define TICK_MS 1000
#define MAX_TICKS 30

static unsigned int tick_count;

static void tick_cb(struct uloop_timeout *t)
{
	tick_count++;
	printf("uloop tick %u/%d\n", tick_count, MAX_TICKS);

	if (tick_count >= MAX_TICKS) {
		puts("done");
		uloop_end();
		return;
	}

	uloop_timeout_set(t, TICK_MS);
	uloop_timeout_add(t);
}

static struct uloop_timeout tick_timer = {
	.cb = tick_cb,
};

int main(void)
{
	if (uloop_init() < 0) {
		fprintf(stderr, "uloop_init failed\n");
		return EXIT_FAILURE;
	}

	uloop_timeout_set(&tick_timer, TICK_MS);
	uloop_timeout_add(&tick_timer);
	uloop_run();
	uloop_done();

	return EXIT_SUCCESS;
}
