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
 * ALTHOUGH THIS SOFTWARE IS MADE OF SCIENCE AND WIN, IT IS PROVIDED BY THE
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
 * This is jack-smf-player, Standard MIDI File player for JACK MIDI.
 *
 * For questions and comments, contact Edward Tomasz Napierala <trasz@FreeBSD.org>.
 */

#include <sysexits.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <smf.h>

#define PROGRAM_NAME		"jmidiplay"
#define PROGRAM_VERSION		"1.0"

#define MIDI_CONTROLLER         0xB0
#define MIDI_ALL_SOUND_OFF      120

jack_port_t *jack_port;
jack_client_t *jack_client = NULL;
int start = 0;
int ctrl_c_pressed = 0;
smf_t *smf = NULL;

static jack_nframes_t seconds_to_nframes(double seconds)
{
	return jack_get_sample_rate(jack_client) * seconds;
}

static void send_all_sound_off(void *port_buffer, jack_nframes_t nframes)
{
	for (int channel = 0; channel < 16; channel++) {
		unsigned char *buffer = jack_midi_event_reserve(port_buffer, 0, 3);
		if (buffer == NULL) {
			g_warning("jack_midi_event_reserve failed, cannot send All Sound Off.");
			break;
		}
		buffer[0] = MIDI_CONTROLLER | channel;
		buffer[1] = MIDI_ALL_SOUND_OFF;
		buffer[2] = 0;
	}

}

static void process_midi_output(jack_nframes_t nframes)
{
	if (ctrl_c_pressed == 2)
		exit(EXIT_SUCCESS);

	void *port_buffer = jack_port_get_buffer(jack_port, nframes);
	if (port_buffer == NULL) {
		g_warning("jack_port_get_buffer failed, cannot send anything.");
		return;
	}
	jack_midi_clear_buffer(port_buffer);

	if (ctrl_c_pressed) {
		send_all_sound_off(port_buffer, nframes);
               /* Exit at the second time process_midi_output gets called.
                  Otherwise, All Sound Off won't be delivered. */
		ctrl_c_pressed = 2;
		return;
	}

	jack_nframes_t last_frame_time = jack_last_frame_time(jack_client);

	for (;;) {
		smf_event_t *event = smf_peek_next_event(smf);
		if (event == NULL) {
			g_debug("End of song.");
			ctrl_c_pressed = 1;
			break;
		}

		/* Skip over metadata events. */
		if (smf_event_is_metadata(event)) {
			char *decoded = smf_event_decode(event);
			if (decoded)
				g_debug("Metadata: %s", decoded);
			smf_skip_next_event(smf);
			continue;
		}

		int t = start + seconds_to_nframes(event->time_seconds) - last_frame_time;
		/* If computed time is too much into the future, we'll need to send it later. */
		if (t >= (int)nframes)
			break;
		/* If computed time is < 0, we missed a cycle because of xrun. */
		if (t < 0)
			t = 0;

		/* We will send this event; remove it from the queue. */
		smf_skip_next_event(smf);
		unsigned char *buffer = jack_midi_event_reserve(port_buffer, t, event->midi_buffer_length);
		if (buffer == NULL) {
			g_warning("jack_midi_event_reserve failed, note lost.");
			break;
		}
		memcpy(buffer, event->midi_buffer, event->midi_buffer_length);
	}
}

static int process_callback(jack_nframes_t nframes, void *arg)
{
	process_midi_output(nframes);
	return 0;
}

/* Connects to the specified input port, disconnecting already connected ports. */
void connect_to_input_port(const char *port)
{
	if (jack_port_disconnect(jack_client, jack_port)) {
		g_critical("Could not disconnect MIDI port.");
		exit(EXIT_FAILURE);
	}

	if (jack_connect(jack_client, jack_port_name(jack_port), port)) {
		g_critical("Could not connect to '%s'.", port);
		exit(EXIT_FAILURE);
	}

	g_message("Connected to %s.", port);
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
			jack_client, "midi_out", JACK_DEFAULT_MIDI_TYPE,
			JackPortIsOutput, 0);
	if (jack_port == NULL) {
		g_critical("Could not register JACK output port 'midi_out'.");
		exit(EXIT_FAILURE);
	}

	if (jack_activate(jack_client)) {
		g_critical("Could not activate JACK client.");
		exit(EXIT_FAILURE);
	}
}

/*
 * This is neccessary for exiting due to jackd being killed, when exit(0)
 * in process_callback won't get called for obvious reasons.
 */
gboolean emergency_exit_timeout(gpointer user_data)
{
	if (!ctrl_c_pressed)
		return TRUE;
	exit(EXIT_SUCCESS);
}

void ctrl_c_handler(int sig)
{
	ctrl_c_pressed = 1;
}

static void log_handler(const gchar *log_domain, GLogLevelFlags log_level,
			const gchar *message, gpointer user_data)
{
	fprintf(stderr, "%s: %s\n", log_domain, message);
}

static void show_version(void)
{
	printf("%s %s, libsmf %s\n", PROGRAM_NAME, PROGRAM_VERSION,
	       smf_get_version());
}

static void usage(void)
{
	printf("usage: jmidiplay [-vh] input:port file_name\n");
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
	if (argc != 3) {
		usage();
		exit(EXIT_FAILURE);
	}

	const char *file_name = argv[2];
	smf = smf_load(file_name);
	if (smf == NULL) {
		g_critical("Loading SMF file failed.");
		exit(EXIT_FAILURE);
	}
	g_message("%s.", smf_decode(smf));

	g_timeout_add(1000, emergency_exit_timeout, NULL);
	signal(SIGINT, ctrl_c_handler);

	init_jack();
	const char *port_name = argv[1];
	connect_to_input_port(port_name);

	start = jack_frame_time(jack_client);

	g_main_loop_run(g_main_loop_new(NULL, TRUE));

	/* Not reached. */
	return 0;
}
