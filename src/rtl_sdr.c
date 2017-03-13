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
		"\t[-p ppm_error (default: 0)]\n"
		"\t[-b output_block_size (default: 16 * 16384)]\n"
		"\t[-n number of samples to read (default: 0, infinite)]\n"
		"\t[-F output format, CU8|CS8|CS16|CF32 (default: CU8)]\n"
		"\t[-S force sync output (default: async)]\n"
		"\t[-D direct_sampling_mode, 0 (default/off), 1 (I), 2 (Q), 3 (no-mod)]\n"
		"\t[-A Name of antenna to use]\n"
		"\t[-N Number of receive channels to use]\n"
		"\tfilename0 filename1 (a '-' dumps samples to stdout)\n\n");
	exit(1);
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
	char *filename0 = NULL;
        char *filename1 = NULL;
	int n_read;
	int r, opt;
	char *gain_str = NULL;
	int ppm_error = 0;
	int sync_mode = 0;
	int direct_sampling = 0;
	FILE *file_ch0;
        FILE *file_ch1;
	int16_t *buffer_ch0;
        int16_t *buffer_ch1;

	uint8_t *buf8 = NULL;
	float *fbuf = NULL; // assumed 32-bit
	char *dev_query = NULL;
	uint32_t frequency = 100000000;
	uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
	uint32_t out_block_size = DEFAULT_BUF_LENGTH;
	char *output_format = SOAPY_SDR_CU8;
	char * ant = NULL;
	size_t * channels;
	size_t nchan = 0;
	size_t ch;
	
	while ((opt = getopt(argc, argv, "d:f:g:s:b:n:p:D:SF:A:N:")) != -1) {
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
			// half of I/Q pair count (double for one each of I and Q)
			samples_to_read = (uint32_t)atofs(optarg) * 2;
			break;
		case 'S':
			sync_mode = 1;
			break;
		case 'A':
		        ant = optarg;
		        break;
		case 'N':
   		        nchan = atoi(optarg);
			break;
		case 'F':
			if (strcasecmp(optarg, "CU8") == 0) {
				output_format = SOAPY_SDR_CU8;
			} else if (strcasecmp(optarg, "CS8") == 0) {
				output_format = SOAPY_SDR_CS8;
			} else if (strcasecmp(optarg, "CS16") == 0) {
				output_format = SOAPY_SDR_CS16;
			} else if (strcasecmp(optarg, "CF32") == 0) {
				output_format = SOAPY_SDR_CF32;
			} else {
				// TODO: support others? maybe after https://github.com/pothosware/SoapySDR/issues/49 Conversion support
				fprintf(stderr, "Unsupported output format: %s\n", optarg);
				exit(1);
			}
            break;
		case 'D':
			direct_sampling = atoi(optarg);
			break;
		default:
			usage();
			break;
		}
	}

	if (argc <= optind+1) {
		usage();
	} else {
		filename0 = argv[optind];
                filename1 = argv[optind+1];
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

	buffer_ch0 = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
        buffer_ch1 = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));

	if (output_format == SOAPY_SDR_CS8 || output_format == SOAPY_SDR_CU8) {
		buf8 = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS8));
	} else if (output_format == SOAPY_SDR_CF32) {
		fbuf = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CF32));
	}

	int tmp_stdout = suppress_stdout_start();
	// TODO: allow choosing input format, see https://www.reddit.com/r/RTLSDR/comments/4tpxv7/rx_tools_commandline_sdr_tools_for_rtlsdr_bladerf/d5ohfse?context=3

	if (nchan > 0) {
	  channels = calloc(nchan, sizeof(size_t));
	} else {
	  channels = NULL;
	}
	
	r = verbose_device_search(dev_query, &dev, &stream, SOAPY_SDR_CS16, channels, nchan);

	if (r != 0) {
		fprintf(stderr, "Failed to open rtlsdr device matching %s.\n", dev_query);
		exit(1);
	}

	fprintf(stderr, "Using output format: %s (input format %s)\n", output_format, SOAPY_SDR_CS16);

	printf("****Number of channels: %d\n", SoapySDRDevice_getNumChannels(dev, SOAPY_SDR_RX));
	
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
	/* Set the frequency */
	/* Set the antenna */
	if (channels == NULL) {
   	        verbose_set_sample_rate(dev, samp_rate, 0);
	        verbose_set_frequency(dev, frequency, 0);
	        verbose_set_antenna(dev, ant, 0);
	  	if (NULL == gain_str) {
		        /* Enable automatic gain */
		  verbose_auto_gain(dev, 0);
		} else {
		  /* Enable manual gain */
		  verbose_gain_str_set(dev, gain_str, 0);
		}
		verbose_ppm_set(dev, ppm_error, 0);
	} else {
   	       for(ch = 0; ch < nchan; ch++) {
	           verbose_set_sample_rate(dev, samp_rate, channels[ch]);
		   verbose_set_frequency(dev, frequency, channels[ch]);
		   verbose_set_antenna(dev, ant, channels[ch]);
		   if (NULL == gain_str) {
		     /* Enable automatic gain */
		     verbose_auto_gain(dev, channels[ch]);
		   } else {
		     /* Enable manual gain */
		     verbose_gain_str_set(dev, gain_str, channels[ch]);
		   }
		   verbose_ppm_set(dev, ppm_error, channels[ch]);
		   
	       }
	}
	

	if(strcmp(filename0, "-") == 0) { /* Write samples to stdout */
		file_ch0 = stdout;
#ifdef _WIN32
		_setmode(_fileno(stdin), _O_BINARY);
#endif
	} else {
		file_ch0 = fopen(filename0, "wb");
		if (!file_ch0) {
			fprintf(stderr, "Failed to open %s\n", filename0);
			goto out;
		}
                file_ch1 = fopen(filename1, "wb");
                if (!file_ch1) {
                        fprintf(stderr, "Failed to open %s\n", filename1);
                        goto out;
                }

	}

	/* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer(dev);

	if (true || sync_mode) {
		fprintf(stderr, "Reading samples in sync mode...\n");
		SoapySDRKwargs args = {0};
		if (SoapySDRDevice_activateStream(dev, stream, 0, 0, 0) != 0) {
			fprintf(stderr, "Failed to activate stream\n");
                        exit(1);
                }
		suppress_stdout_stop(tmp_stdout);
		while (!do_exit) {
			void *buffs[] = {buffer_ch0, buffer_ch1};
			int flags = 0;
			long long timeNs = 0;
			long timeoutNs = 1000000;
			int n_read = 0, r, i;

			r = SoapySDRDevice_readStream(dev, stream, buffs, out_block_size, &flags, &timeNs, timeoutNs);

			//fprintf(stderr, "readStream ret=%d, flags=%d, timeNs=%lld\n", r, flags, timeNs);
			if (r >= 0) {
				// r is number of elements read, elements=complex pairs of 8-bits, so buffer length in bytes is twice
				n_read = r * 2;
			} else {
				if (r == SOAPY_SDR_OVERFLOW) {
					fprintf(stderr, "O");
					fflush(stderr);
					continue;
				}
				fprintf(stderr, "WARNING: sync read failed. %d\n", r);
			}

			if ((samples_to_read > 0) && (samples_to_read < (uint32_t)n_read)) {
				n_read = samples_to_read;
				do_exit = 1;
			}

			// TODO: read these formats natively from SoapySDR (setupStream) instead of converting ourselves?
			if (output_format == SOAPY_SDR_CS16) {
				// The "native" format we read in, write out no conversion needed
				// (Always reading in CS16 to support >8-bit devices)
				if (fwrite(buffer_ch0, sizeof(int16_t), n_read, file_ch0) != (size_t)n_read) {
					fprintf(stderr, "Short write, samples lost, exiting!\n");
					break;
				}
                                if (fwrite(buffer_ch1, sizeof(int16_t), n_read, file_ch1) != (size_t)n_read) {
                                        fprintf(stderr, "Short write, samples lost, exiting!\n");
                                        break;
                                }

			} else if (output_format == SOAPY_SDR_CS8) {
				for (i = 0; i < n_read; ++i) {
					buf8[i] = ( (int16_t)buffer_ch0[i] / 32767.0 * 128.0 + 0.4);
				}
				if (fwrite(buf8, sizeof(uint8_t), n_read, file_ch0) != (size_t)n_read) {
					fprintf(stderr, "Short write, samples lost, exiting!\n");
					break;
				}
			} else if (output_format == SOAPY_SDR_CU8) {
				for (i = 0; i < n_read; ++i) {
					buf8[i] = ( (int16_t)buffer_ch0[i] / 32767.0 * 128.0 + 127.4);
				}
				if (fwrite(buf8, sizeof(uint8_t), n_read, file_ch0) != (size_t)n_read) {
					fprintf(stderr, "Short write, samples lost, exiting!\n");
					break;
				}
			} else if (output_format == SOAPY_SDR_CF32) {
				for (i = 0; i < n_read; ++i) {
					fbuf[i] = buffer_ch0[i] * 1.0f / SHRT_MAX;
				}
				if (fwrite(fbuf, sizeof(float), n_read, file_ch0) != (size_t)n_read) {
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
				samples_to_read -= n_read;
		}
	}

	if (do_exit)
		fprintf(stderr, "\nUser cancel, exiting...\n");
	else
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

	if (file_ch0 != stdout)
		fclose(file_ch0);
        fclose(file_ch1);

	SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
	SoapySDRDevice_closeStream(dev, stream);
	SoapySDRDevice_unmake(dev);
	free (buffer_ch0);
        free(buffer_ch1);
out:
	return r >= 0 ? r : -r;
}
