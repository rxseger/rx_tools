/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include "getopt/getopt.h"
#endif

#include "convenience.h"
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#define DEFAULT_SAMPLE_RATE		2048000
#define MINIMAL_BUF_LENGTH		512
#define MAXIMAL_BUF_LENGTH		(256 * 8196)
#define MAX_NUM_CHANNELS		256

#define ISFMT(a,b) (!strcmp((a),(b)))

static int do_exit = 0;
static uint32_t samples_to_read = 0;
static uint32_t samples_to_skip = 0;
static SoapySDRDevice *dev = NULL;
static SoapySDRStream *stream = NULL;

void usage(void)
{
	fprintf(stderr,
		"rx_sdr (based on rtl_sdr), an I/Q recorder for RTL2832 based DVB-T receivers\n\n"
		"Usage:\t -f frequency_to_tune_to [Hz]\n"
		"\t[-s samplerate (default: 2048000 Hz)]\n"
		"\t[-w bandwidth (default: samplerate)]\n"
		"\t[-d device key/value query (ex: 0, 1, driver=rtlsdr, driver=hackrf)]\n"
		"\t[-g tuner gain(s) (ex: 20, 40, LNA=40,VGA=20,AMP=0)]\n"
		"\t[-c comma-separated list of channels (ex: 0,1)]\n"
		"\t[-a antenna (ex: 'Tuner 1 50 ohm')]\n"
		"\t[-p ppm_error (default: 0)]\n"
		"\t[-b buffer size (in samples, default: stream MTU)]\n"
		"\t[-n number of samples to read (default: 0, infinite)]\n"
		"\t[-k number of samples skip before recording (default: 0)]\n"
		"\t[-I input format, CU8|CS8|CS12|CS16|CF32 (default: CS16)]\n"
		"\t[-F output format, CU8|CS8|CS12|CS16|CF32 (default: CU8)]\n"
		"\t[-S force sync output (default: async)]\n"
		"\t[-D direct_sampling_mode, 0 (default/off), 1 (I), 2 (Q), 3 (no-mod)]\n"
		"\t[-t SDR settings (ex: rfnotch_ctrl=false,dabnotch_ctrlb=true)]\n"
		"\tfilename (a '-' dumps samples to stdout)\n\n");
	exit(1);
}

char const *parse_fmt(char const *fmt)
{
	if (!fmt || !*fmt)
		return NULL;

	else if (!strcasecmp(optarg, "CU8"))
		return SOAPY_SDR_CU8;

	else if (!strcasecmp(optarg, "CS8"))
		return SOAPY_SDR_CS8;

	else if (!strcasecmp(optarg, "CS12"))
		return SOAPY_SDR_CS12;

	else if (!strcasecmp(optarg, "CS16"))
		return SOAPY_SDR_CS16;

	else if (!strcasecmp(optarg, "CF32"))
		return SOAPY_SDR_CF32;

	else
		return NULL;
}

size_t parse_channels(const char * channels_str, size_t * channels) {
	const char * next = NULL;
	const char * end = channels_str + strlen(channels_str);
	size_t num_channels = 0;
	while (channels_str < end && num_channels < MAX_NUM_CHANNELS) {
		channels[num_channels] = strtol(channels_str, &next, 10);
		if (next != channels_str) {
			num_channels += 1;
		}
		channels_str = next + 1;
	}
	return num_channels;
}

#ifdef _WIN32
BOOL WINAPI
sighandler(int signum)
{
	if (CTRL_C_EVENT == signum) {
		fprintf(stderr, "Signal caught, exiting!\n");
		do_exit = 1;
		return TRUE;
	}
	return FALSE;
}
#else
static void sighandler(int signum)
{
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
}
#endif

int main(int argc, char **argv)
{
#ifndef _WIN32
	struct sigaction sigact;
#endif
	char *filename = NULL;
	int r, opt;
	char *gain_str = NULL;
	size_t channels[MAX_NUM_CHANNELS] = {0};
	size_t num_channels = 0;
	char *antenna_str = NULL;
	int ppm_error = 0;
	int sync_mode = 0;
	int direct_sampling = 0;
	char *dev_query = "";
	uint32_t frequency = 100000000;
	uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
	uint32_t bandwidth = 0;
	uint32_t buffer_size = 0;
	char const *input_format = SOAPY_SDR_CS16;
	char const *output_format = SOAPY_SDR_CU8;
	char *sdr_settings = NULL;

	while ((opt = getopt(argc, argv, "d:f:g:c:a:s:b:n:p:D:SI:F:t:w:k:")) != -1) {
		switch (opt) {
		case 'd':
			dev_query = optarg;
			break;
		case 'f':
			frequency = (uint32_t)atofs(optarg);
			break;
		case 'g':
			gain_str = optarg;
			break;
		case 'c':
			num_channels = parse_channels(optarg, channels);
			break;
		case 'a':
			antenna_str = optarg;
			break;
		case 's':
			samp_rate = (uint32_t)atofs(optarg);
			break;
		case 'w':
			bandwidth = (uint32_t)atofs(optarg);
			break;
		case 'p':
			ppm_error = atoi(optarg);
			break;
		case 'b':
			buffer_size = (uint32_t)atof(optarg);
			break;
		case 'n':
			// full I/Q pair count
			samples_to_read = (uint32_t)atofs(optarg);
			break;
		case 'k':
			samples_to_skip = (uint32_t)atofs(optarg);
			break;
		case 'S':
			sync_mode = 1;
			break;
		case 'I':
			input_format = parse_fmt(optarg);
			if (!input_format) {
				fprintf(stderr, "Unsupported input format: %s\n", optarg);
				exit(1);
			}
			break;
		case 'F':
			output_format = parse_fmt(optarg);
			if (!output_format) {
				// TODO: support others? maybe after https://github.com/pothosware/SoapySDR/issues/49 Conversion support
				fprintf(stderr, "Unsupported output format: %s\n", optarg);
				exit(1);
			}
            break;
		case 'D':
			direct_sampling = atoi(optarg);
			break;
		case 't':
			sdr_settings = optarg;
			break;
		default:
			usage();
			break;
		}
	}

	// for now only input to output in same format, input CS16 to all, and CS12 to CS16
	if (!ISFMT(input_format, output_format)
			&& !ISFMT(input_format, SOAPY_SDR_CS16)
			&& (!ISFMT(input_format, SOAPY_SDR_CS12) || !ISFMT(output_format, SOAPY_SDR_CS16))) {
		fprintf(stderr, "Unsupported input/output conversion: %s to %s\n", input_format, output_format);
		exit(1);
	}

	if (argc <= optind) {
		usage();
	} else {
		filename = argv[optind];
	}

	int tmp_stdout = suppress_stdout_start();
	// TODO: allow choosing input format, see https://www.reddit.com/r/RTLSDR/comments/4tpxv7/rx_tools_commandline_sdr_tools_for_rtlsdr_bladerf/d5ohfse?context=3
	r = verbose_device_search(dev_query, &dev);
	if (r != 0 || dev == NULL) {
		fprintf(stderr, "Failed to open sdr device matching '%s'.\n", dev_query);
		exit(1);
	}

	size_t max_dev_channels = SoapySDRDevice_getNumChannels(dev, SOAPY_SDR_RX);
	if (num_channels == 0 || num_channels > max_dev_channels) {
		fprintf(stderr, "Invalid channel specification, requested %d channels, maximum available %d\n", num_channels, max_dev_channels);
		exit(1);
	}

	size_t input_elem_size = SoapySDR_formatToSize(input_format);
	fprintf(stderr, "Using output format: %s (input format %s, %d bytes per element)\n", output_format, input_format, (int)input_elem_size);

#ifndef _WIN32
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);
#else
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) sighandler, TRUE );
#endif

	if (direct_sampling) {
		verbose_direct_sampling(dev, direct_sampling);
	}

	if (bandwidth == 0)
		bandwidth = samp_rate;

	for (size_t chan_idx = 0; chan_idx < num_channels; chan_idx++) {
		verbose_set_properties(dev, samp_rate, frequency, gain_str, antenna_str, ppm_error, chan_idx);
		verbose_set_bandwidth(dev, bandwidth, chan_idx);
	}

	FILE *outfiles[num_channels];
	if (strcmp(filename, "-") == 0) {
		if (num_channels != 1) {
			fprintf(stderr, "ERROR: Cannot stream multichannel data to stdout!\n");
			exit(1);
		}
		outfiles[0] = stdout;
#ifdef _WIN32
		_setmode(_fileno(stdin), _O_BINARY);
#endif
	} else {
		for (size_t i = 0; i < num_channels; i++) {
			char fn[PATH_MAX];
			snprintf(fn, PATH_MAX, "%s.rx%d", filename, channels[i]);
			outfiles[i] = fopen(fn, "wb");
			if (!outfiles[i]) {
				fprintf(stderr, "Failed to open %s\n", fn);
				exit(1);
			}
		}
	}

	r = verbose_setup_stream(dev, &stream, channels, num_channels, input_format);
	if(r != 0){
		fprintf(stderr, "Failed to setup stream\n");
		exit(1);
	}
	/* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer(dev);

	// Auto-detect MTU
	if (buffer_size == 0) {
		buffer_size = SoapySDRDevice_getStreamMTU(dev, stream);
	}

	if(buffer_size < MINIMAL_BUF_LENGTH ||
	   buffer_size > MAXIMAL_BUF_LENGTH ){
		fprintf(stderr,
			"Buffer size illegal value, falling back to maximal value\n");
		fprintf(stderr,
			"Minimal length: %u\n", MINIMAL_BUF_LENGTH);
		fprintf(stderr,
			"Maximal length: %u\n", MAXIMAL_BUF_LENGTH);
		buffer_size = MAXIMAL_BUF_LENGTH;
	}

	void ** buffers = malloc(sizeof(void *)*num_channels);
	for (size_t chan_idx=0; chan_idx<num_channels; ++chan_idx) {
		// TODO: Fix the XTRX driver to either actually provide CS12, or to advertise that it provides CS16
		//buffers[chan_idx] = malloc(buffer_size * SoapySDR_formatToSize(input_elem_size));
		buffers[chan_idx] = malloc(buffer_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
	}
	// Output buffer that holds the converted data.  We only convert a single channel at a time.
	void * output_buffer = malloc(buffer_size * SoapySDR_formatToSize(output_format));

	if(sdr_settings)
		verbose_settings(dev, sdr_settings);

	if (true || sync_mode) {
		fprintf(stderr, "Reading samples in sync mode...\n");
		if (SoapySDRDevice_activateStream(dev, stream, 0, 0, 0) != 0) {
			fprintf(stderr, "Failed to activate stream\n");
                        exit(1);
                }
		suppress_stdout_stop(tmp_stdout);
		while (!do_exit) {
			int flags = 0;
			long long timeNs = 0;
			long timeoutNs = 1000000;
			int samples_read = 0;

			// readStream returns the number of samples read (which is the same for each channel)
			samples_read = SoapySDRDevice_readStream(dev, stream, buffers, buffer_size, &flags, &timeNs, timeoutNs);

			//fprintf(stderr, "readStream ret=%d, flags=%d, timeNs=%lld\n", samples_read, flags, timeNs);
			if (samples_read < 0) {
				if (samples_read == SOAPY_SDR_OVERFLOW) {
					fprintf(stderr, "O");
					fflush(stderr);
					continue;
				}
				fprintf(stderr, "WARNING: sync read failed. %d\n", samples_read);
			}

			if ((samples_to_read > 0) && (samples_to_read < (uint32_t)samples_read)) {
				// truncate to requested sample count
				samples_read = samples_to_read;
				do_exit = 1;
			}

			// Don't process samples until we've skipped the requested number of samples
			if (samples_to_skip > 0) {
				if (samples_read > samples_to_skip)
					samples_read = samples_to_skip;
				samples_to_skip -= samples_read;
				continue;
			}

			// For each channel we've received, write it out to its respective file
			for (size_t chan_idx=0; chan_idx < num_channels; ++chan_idx) {
				if (ISFMT(output_format, input_format)) {
					// The "native" format we read in, write out no conversion needed
					if (fwrite(buffers[chan_idx], SoapySDR_formatToSize(output_format), samples_read, outfiles[chan_idx]) != (size_t)samples_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						break;
					}
				} else if (ISFMT(input_format, SOAPY_SDR_CS12) && ISFMT(output_format, SOAPY_SDR_CS16)) {
					// Unpack 12-bit samples into our preallocated 16-bit buffer
					for (int i = 0; i < samples_read; ++i) {
						uint8_t b0 = ((uint8_t *)buffers[chan_idx])[i*3 + 0];
						uint8_t b1 = ((uint8_t *)buffers[chan_idx])[i*3 + 1];
						uint8_t b2 = ((uint8_t *)buffers[chan_idx])[i*3 + 2];
						((uint16_t *)output_buffer)[i * 2 + 0] = (b1 << 12) | (b0 << 4);
						((uint16_t *)output_buffer)[i * 2 + 1] = (b2 << 8) | (b1 & 0xf0);
					}
					if (fwrite(output_buffer, sizeof(uint16_t), samples_read, outfiles[chan_idx]) != (size_t)samples_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						break;
					}
				} else if (ISFMT(input_format, SOAPY_SDR_CS16) && ISFMT(output_format, SOAPY_SDR_CS8)) {
					for (int i = 0; i < samples_read; ++i) {
						((uint8_t *)output_buffer)[i] = (uint8_t)(((int16_t *)buffers[chan_idx])[i] / (float)SHRT_MAX * 128.0 + 0.4);
					}
					if (fwrite(output_buffer, sizeof(uint8_t), samples_read, outfiles[chan_idx]) != (size_t)samples_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						break;
					}
				} else if (ISFMT(input_format, SOAPY_SDR_CS16) && ISFMT(output_format, SOAPY_SDR_CU8)) {
					for (int i = 0; i < samples_read; ++i) {
						((int8_t *)output_buffer)[i] = (int8_t)(((int16_t*)buffers[chan_idx])[i] / (float)SHRT_MAX * 128.0 + 127.4);
					}
					if (fwrite(output_buffer, sizeof(int8_t), samples_read, outfiles[chan_idx]) != (size_t)samples_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						break;
					}
				} else if (ISFMT(input_format, SOAPY_SDR_CS16) && ISFMT(output_format, SOAPY_SDR_CF32)) {
					for (int i = 0; i < samples_read; ++i) {
						((float *)output_buffer)[i] = ((uint16_t*)buffers[chan_idx])[i] * 1.0f / (float)SHRT_MAX;
					}
					if (fwrite(output_buffer, sizeof(float), samples_read, outfiles[chan_idx]) != (size_t)samples_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						break;
					}
				}
			}


                        // TODO: hmm.. n_read 8192, but buffer_size (16 * 16384) is much larger TODO: loop? or accept 8192? rtl_fm ok with it
                        /*
			if ((uint32_t)n_read < buffer_size) {
				fprintf(stderr, "Short read, samples lost, exiting! (%d < %d)\n", n_read, buffer_size);
				break;
			}
                        */

			if (samples_to_read > 0)
				samples_to_read -= samples_read;
		}
	}

	if (do_exit)
		fprintf(stderr, "\nUser cancel, exiting...\n");
	else
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

	if (strcmp(filename, "-") != 0) {
		for (int i = 0; i < num_channels; ++i) {
			fclose(outfiles[i]);
		}
	}

	SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
	SoapySDRDevice_closeStream(dev, stream);
	SoapySDRDevice_unmake(dev);

	// Free our buffers, even though we're about to exit
	for (int chan_idx=0; chan_idx < num_channels; ++chan_idx) {
		free(buffers[chan_idx]);
	}
	free(buffers);

	return r >= 0 ? r : -r;
}
