/*-
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * ALTHOUGH THIS SOFTWARE IS MADE OF WIN AND SCIENCE, IT IS PROVIDED BY THE
 * AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * This is jack-smf-recorder, Standard MIDI File recorder for JACK MIDI.
 *
 * For questions and comments, contact Edward Tomasz Napierala <trasz@FreeBSD.org>.
 */

#include <stdio.h>
#include <sysexits.h>
#include <glib.h>
#include <jack/jack.h>
#include <jack/midiport.h>

#define PROGRAM_NAME		"jmidirec"
#define PROGRAM_VERSION		"1.0"

jack_client_t *jack_client = NULL;
jack_port_t *jack_port = NULL;

static void process_midi_input(jack_nframes_t nframes)
{
	void *port_buffer = jack_port_get_buffer(jack_port, nframes);
	if (port_buffer == NULL) {
		g_warning("jack_port_get_buffer failed, cannot receive anything.");
		return;
	}

	int events = jack_midi_get_event_count(port_buffer);
	for (int i = 0; i < events; i++) {
		jack_midi_event_t event;
		if (jack_midi_event_get(&event, port_buffer, i)) {
			g_warning("jack_midi_event_get failed, received note lost.");
			continue;
		}

		for (int i = 0; i < event.size; i++)
			printf("%x ", event.buffer[i]);
		printf("\n");
	}
}

static int process_callback(jack_nframes_t nframes, void *arg)
{
	process_midi_input(nframes);
	return 0;
}

/* Connects to the specified input port, disconnecting already connected ports. */
gboolean connect_to_output_port(gpointer user_data)
{
	const char *port = (const char *)user_data;
	if (jack_port_disconnect(jack_client, jack_port)) {
		g_critical("Could not disconnect MIDI port.");
		exit(EXIT_FAILURE);
	}

	if (jack_connect(jack_client, port, jack_port_name(jack_port))) {
		g_warning("Could not connect to '%s'.", port);
		return TRUE;
	}

	g_message("Connected to '%s'. Press ^C to stop.", port);
	return FALSE;
}

static void init_jack()
{
	jack_client = jack_client_open(PROGRAM_NAME, JackNullOption, NULL);
	if (jack_client == NULL) {
		g_critical("Could not connect to the JACK server.");
		exit(EXIT_FAILURE);
	}

	if (jack_set_process_callback(jack_client, process_callback, NULL)) {
		g_critical("Could not register JACK process callback.");
		exit(EXIT_FAILURE);
	}

	jack_port = jack_port_register(
			jack_client, "midi_in", JACK_DEFAULT_MIDI_TYPE,
			JackPortIsInput, 0);
	if (jack_port == NULL) {
		g_critical("Could not register JACK input port 'midi_in'.");
		exit(EXIT_FAILURE);
	}

	if (jack_activate(jack_client)) {
		g_critical("Could not activate JACK client.");
		exit(EXIT_FAILURE);
	}
}

void ctrl_c_handler(int sig)
{
	jack_deactivate(jack_client);
	exit(EXIT_SUCCESS);
}

static void log_handler(const gchar *log_domain, GLogLevelFlags log_level,
			const gchar *message, gpointer notused)
{
	fprintf(stderr, "%s: %s\n", log_domain, message);
}

static void show_version()
{
	printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}

static void usage()
{
	printf("usage: jmidirec [-vh] output:port\n");
}

int main(int argc, char *argv[])
{
	g_log_set_default_handler(log_handler, NULL);

	if (argc == 1) {
		usage();
		exit(EXIT_FAILURE);
	}
	if (!strcmp(argv[1], "-v")) {
		show_version();
		exit(EXIT_SUCCESS);
	}
	if (!strcmp(argv[1], "-h")) {
		usage();
		exit(EXIT_SUCCESS);
	}
	if (argc != 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	init_jack();

	if (connect_to_output_port(argv[1]))
		g_timeout_add(1000, connect_to_output_port, argv[1]);
	signal(SIGINT, ctrl_c_handler);

	g_main_loop_run(g_main_loop_new(NULL, TRUE));

	/* Not reached. */
	return 0;
}
