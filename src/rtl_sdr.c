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
#define DEFAULT_BUF_LENGTH		(16 * 16384)
#define MINIMAL_BUF_LENGTH		512
#define MAXIMAL_BUF_LENGTH		(256 * 16384)

#define ISFMT(a,b) (!strcmp((a),(b)))

static int do_exit = 0;
static uint32_t samples_to_read = 0;
static SoapySDRDevice *dev = NULL;
static SoapySDRStream *stream = NULL;

void usage(void)
{
	fprintf(stderr,
		"rx_sdr (based on rtl_sdr), an I/Q recorder for RTL2832 based DVB-T receivers\n\n"
		"Usage:\t -f frequency_to_tune_to [Hz]\n"
		"\t[-s samplerate (default: 2048000 Hz)]\n"
		"\t[-d device key/value query (ex: 0, 1, driver=rtlsdr, driver=hackrf)]\n"
		"\t[-g tuner gain(s) (ex: 20, 40, LNA=40,VGA=20,AMP=0)]\n"
		"\t[-c channel number (ex: 0)]\n"
		"\t[-a antenna (ex: 'Tuner 1 50 ohm')]\n"
		"\t[-p ppm_error (default: 0)]\n"
		"\t[-b output_block_size (default: 16 * 16384)]\n"
		"\t[-n number of samples to read (default: 0, infinite)]\n"
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
	int channel = 0;
	char *antenna_str = NULL;
	int ppm_error = 0;
	int sync_mode = 0;
	int direct_sampling = 0;
	FILE *file;
	int16_t *buffer;
	uint8_t *buf8 = NULL;
	int16_t *buf16 = NULL;
	float *fbuf = NULL; // assumed 32-bit
	char *dev_query = "";
	uint32_t frequency = 100000000;
	uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
	uint32_t out_block_size = DEFAULT_BUF_LENGTH;
	char const *input_format = SOAPY_SDR_CS16;
	char const *output_format = SOAPY_SDR_CU8;
	char *sdr_settings = NULL;

	while ((opt = getopt(argc, argv, "d:f:g:c:a:s:b:n:p:D:SI:F:t:")) != -1) {
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
			channel = (int)atoi(optarg);
			break;
		case 'a':
			antenna_str = optarg;
			break;
		case 's':
			samp_rate = (uint32_t)atofs(optarg);
			break;
		case 'p':
			ppm_error = atoi(optarg);
			break;
		case 'b':
			out_block_size = (uint32_t)atof(optarg);
			break;
		case 'n':
			// full I/Q pair count
			samples_to_read = (uint32_t)atofs(optarg);
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

	if(out_block_size < MINIMAL_BUF_LENGTH ||
	   out_block_size > MAXIMAL_BUF_LENGTH ){
		fprintf(stderr,
			"Output block size wrong value, falling back to default\n");
		fprintf(stderr,
			"Minimal length: %u\n", MINIMAL_BUF_LENGTH);
		fprintf(stderr,
			"Maximal length: %u\n", MAXIMAL_BUF_LENGTH);
		out_block_size = DEFAULT_BUF_LENGTH;
	}

	buffer = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
	if (ISFMT(output_format, SOAPY_SDR_CS8) || ISFMT(output_format, SOAPY_SDR_CU8)) {
		buf8 = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS8));
	} else if (ISFMT(output_format, SOAPY_SDR_CS16)) {
		buf16 = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
	} else if (ISFMT(output_format, SOAPY_SDR_CF32)) {
		fbuf = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CF32));
	}
	size_t input_elem_size = SoapySDR_formatToSize(input_format);

	int tmp_stdout = suppress_stdout_start();
	// TODO: allow choosing input format, see https://www.reddit.com/r/RTLSDR/comments/4tpxv7/rx_tools_commandline_sdr_tools_for_rtlsdr_bladerf/d5ohfse?context=3
	r = verbose_device_search(dev_query, &dev);

	if (r != 0) {
		fprintf(stderr, "Failed to open sdr device matching '%s'.\n", dev_query);
		exit(1);
	}

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

	/* Set the sample rate */
	verbose_set_sample_rate(dev, samp_rate, channel);

	/* Set the frequency */
	verbose_set_frequency(dev, frequency, channel);

	if (NULL == gain_str) {
		 /* Enable automatic gain */
		verbose_auto_gain(dev, channel);
	} else {
		/* Enable manual gain */
		verbose_gain_str_set(dev, gain_str, channel);
	}

	/* Set the antenna */
	if (NULL != antenna_str){
		r = verbose_antenna_str_set(dev, channel, antenna_str);
		if(r != 0){
			fprintf(stderr, "Failed to set antenna");
		}
	}

	verbose_ppm_set(dev, ppm_error, channel);

	if(strcmp(filename, "-") == 0) { /* Write samples to stdout */
		file = stdout;
#ifdef _WIN32
		_setmode(_fileno(stdin), _O_BINARY);
#endif
	} else {
		file = fopen(filename, "wb");
		if (!file) {
			fprintf(stderr, "Failed to open %s\n", filename);
			goto out;
		}
	}

	r = verbose_setup_stream(dev, &stream, channel, input_format);
	if(r != 0){
		fprintf(stderr, "Failed to setup stream\n");
	}
	/* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer(dev);

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
			void *buffs[] = {buffer};
			int flags = 0;
			long long timeNs = 0;
			long timeoutNs = 1000000;
			int n_read = 0, bytes_read = 0, elems_read, i;

			elems_read = SoapySDRDevice_readStream(dev, stream, buffs, out_block_size, &flags, &timeNs, timeoutNs);

			//fprintf(stderr, "readStream ret=%d, flags=%d, timeNs=%lld\n", elems_read, flags, timeNs);
			if (elems_read >= 0) {
				// elems_read is number of complex pairs of I+Q elements read
				n_read = elems_read * 2; // one element read is I and Q
				bytes_read = elems_read * input_elem_size;
			} else {
				if (elems_read == SOAPY_SDR_OVERFLOW) {
					fprintf(stderr, "O");
					fflush(stderr);
					continue;
				}
				fprintf(stderr, "WARNING: sync read failed. %d\n", elems_read);
			}

			if ((samples_to_read > 0) && (samples_to_read < (uint32_t)elems_read)) {
				// truncate to requested sample count
				n_read = samples_to_read * 2;
				bytes_read = samples_to_read * input_elem_size;
				do_exit = 1;
			}

			if (ISFMT(output_format, input_format)) {
				// The "native" format we read in, write out no conversion needed
				if (fwrite(buffer, sizeof(uint8_t), bytes_read, file) != (size_t)bytes_read) {
					fprintf(stderr, "Short write, samples lost, exiting!\n");
					break;
				}
			} else if (ISFMT(input_format, SOAPY_SDR_CS12) && ISFMT(output_format, SOAPY_SDR_CS16)) {
				uint8_t *src = (uint8_t *)buffer;
				for (i = 0; i < elems_read; ++i) {
					uint8_t b0 = *src++;
					uint8_t b1 = *src++;
					uint8_t b2 = *src++;
					buf16[i * 2 + 0] = (b1 << 12) | (b0 << 4);
					buf16[i * 2 + 1] = (b2 << 8) | (b1 & 0xf0);
				}
				if (fwrite(buf16, sizeof(int16_t), n_read, file) != (size_t)n_read) {
					fprintf(stderr, "Short write, samples lost, exiting!\n");
					break;
				}
			} else if (ISFMT(output_format, SOAPY_SDR_CS8)) {
				for (i = 0; i < n_read; ++i) {
					buf8[i] = ( (int16_t)buffer[i] / 32767.0 * 128.0 + 0.4);
				}
				if (fwrite(buf8, sizeof(int8_t), n_read, file) != (size_t)n_read) {
					fprintf(stderr, "Short write, samples lost, exiting!\n");
					break;
				}
			} else if (ISFMT(output_format, SOAPY_SDR_CU8)) {
				for (i = 0; i < n_read; ++i) {
					buf8[i] = ( (int16_t)buffer[i] / 32767.0 * 128.0 + 127.4);
				}
				if (fwrite(buf8, sizeof(uint8_t), n_read, file) != (size_t)n_read) {
					fprintf(stderr, "Short write, samples lost, exiting!\n");
					break;
				}
			} else if (ISFMT(output_format, SOAPY_SDR_CF32)) {
				for (i = 0; i < n_read; ++i) {
					fbuf[i] = buffer[i] * 1.0f / SHRT_MAX;
				}
				if (fwrite(fbuf, sizeof(float), n_read, file) != (size_t)n_read) {
					fprintf(stderr, "Short write, samples lost, exiting!\n");
					break;
				}
			}


                        // TODO: hmm.. n_read 8192, but out_block_size (16 * 16384) is much larger TODO: loop? or accept 8192? rtl_fm ok with it
                        /*
			if ((uint32_t)n_read < out_block_size) {
				fprintf(stderr, "Short read, samples lost, exiting! (%d < %d)\n", n_read, out_block_size);
				break;
			}
                        */

			if (samples_to_read > 0)
				samples_to_read -= elems_read;
		}
	}

	if (do_exit)
		fprintf(stderr, "\nUser cancel, exiting...\n");
	else
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

	if (file != stdout)
		fclose(file);

	SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
	SoapySDRDevice_closeStream(dev, stream);
	SoapySDRDevice_unmake(dev);

out:
	free(buffer);
	free(buf8);
	free(buf16);
	free(fbuf);

	return r >= 0 ? r : -r;
}
