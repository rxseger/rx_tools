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
#include <getopt.h>
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
#define MAX_NUM_CHANNELS                (8)

static int do_exit = 0;
static uint64_t samples_to_read = 0;
static SoapySDRDevice *dev = NULL;
static SoapySDRStream *stream = NULL;

void usage(void)
{
	fprintf(stderr,
		"rx_sdr (based on rtl_sdr), an I/Q recorder for RTL2832 based DVB-T receivers\n\n"
		"Usage:\t -f/--freq frequency_to_tune_to [Hz]\n"
		"\t[-s/--srate samplerate (default: 2048000 Hz)]\n"
		"\t[-d device key/value query (ex: 0, 1, driver=rtlsdr, driver=hackrf)]\n"
		"\t[-g/--gain tuner gain(s) (ex: 20, 40, LNA=40,VGA=20,AMP=0)]\n"
		"\t[-p ppm_error (default: 0)]\n"
		"\t[-b output_block_size (default: 16 * 16384)]\n"
		"\t[-n number of samples to read (default: 0, infinite)]\n"
		"\t[-F output format, CU8|CS8|CS16|CF32 (default: CU8)]\n"
		"\t[-S force sync output (default: async)]\n"
		"\t[-D direct_sampling_mode, 0 (default/off), 1 (I), 2 (Q), 3 (no-mod)]\n"
		"\t[-A/--Antenna ant   Name of antenna to use]\n"
		"\t[-B/--Bandwidth bw  Receiver bandwidth to use]\n"
		"\t[-N Number of receive channels to use]\n"
		"\tfilename0 [filename1.....] (a '-' dumps samples to stdout)\n"
		"\n"
		"\tAll long options have optional channel number, so --gain3 10 sets the\n"
		"\tgain of channel 3 (starting from 0) to be '10'. -g/--gain would set the gain\n"
		"\tof all the channels, for example.\n\n"
		);
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
	char *filename[MAX_NUM_CHANNELS];
	int64_t n_read;

	int r, opt;
	char *gain_str[MAX_NUM_CHANNELS];
	int ppm_error = 0;
	int sync_mode = 0;
	int direct_sampling = 0;
	FILE *file_ch[MAX_NUM_CHANNELS];
	int16_t *buffer_ch[MAX_NUM_CHANNELS];
	double bw[MAX_NUM_CHANNELS];
	
	uint8_t *buf8 = NULL;
	float *fbuf = NULL; // assumed 32-bit
	char *dev_query = NULL;
	uint32_t frequency[MAX_NUM_CHANNELS];
	uint32_t samp_rate[MAX_NUM_CHANNELS];
	uint32_t out_block_size = DEFAULT_BUF_LENGTH;
	char *output_format = SOAPY_SDR_CU8;
	char * ant[MAX_NUM_CHANNELS];
	size_t * channels;
	size_t nchan = 0;
	size_t ch;
	struct option *lg_opts = NULL;
	char * base_opt[] = {"freq", "gain", "srate", "Ant", "Bandwidth"};
	int o, count;
	int option_idx;
	
	lg_opts = malloc(sizeof(struct option) * (MAX_NUM_CHANNELS * (sizeof(base_opt) / sizeof(char *) + 2)));
	if (lg_opts == NULL) {
		fprintf(stderr, "Failed to malloc data for lg_opts!!\n");
		exit(10);
	}
	
	for (ch = 0; ch < MAX_NUM_CHANNELS; ch++) {
		filename[ch] = NULL;
		gain_str[ch] = NULL;
		file_ch[ch] = NULL;
		buffer_ch[ch] = NULL;
		bw[ch] = -1;
		frequency[ch] = 40000000;
		samp_rate[ch] = DEFAULT_SAMPLE_RATE;
		ant[ch] = NULL;
	}
	
	count = 0;
	for (o = 0; o < sizeof(base_opt) / sizeof(char *); o++) {
		char * tmp_name;
		
		lg_opts[count].name = base_opt[o];
		lg_opts[count].has_arg = 1;
		lg_opts[count].flag = NULL;
		lg_opts[count].val = base_opt[0][0];
		count++;
		for(ch = 0; ch < MAX_NUM_CHANNELS; ch++) {
			tmp_name = malloc(strlen(base_opt[o]) + 10);
			if (tmp_name == NULL) {
				fprintf(stderr, "Failed to malloc data for tmp_name!!\n");
				exit(10);
			}
			
			if (MAX_NUM_CHANNELS > 9) {
				snprintf(tmp_name, strlen(base_opt[o]) + 10, "%s%02d", base_opt[o], ch);
			} else {
				snprintf(tmp_name, strlen(base_opt[o]) + 10, "%s%d", base_opt[o], ch);
			}
			lg_opts[count].name = tmp_name;
			lg_opts[count].has_arg = 1;
			lg_opts[count].flag = NULL;
			lg_opts[count].val = 0;
			count++;
		}
		
	}
	lg_opts[count].name = NULL;
	lg_opts[count].has_arg = 0;
	lg_opts[count].flag = NULL;
	lg_opts[count].val = 0;
	
	while ((opt = getopt_long(argc, argv, "d:f:g:s:b:n:p:D:SF:A:N:B:", lg_opts, &option_idx)) != -1) {
		switch (opt) {
		case 0:
			for (o = 0; o < sizeof(base_opt) / sizeof(char *); o++) {
				if (strncmp(base_opt[o], lg_opts[option_idx].name, strlen(base_opt[o])) == 0) {
					sscanf(&lg_opts[option_idx].name[strlen(base_opt[o])], "%d", &ch);
					
					switch (base_opt[o][0]) {
					case 'f':
						frequency[ch] = (uint32_t)atofs(optarg);
						break;
					case 'g':
						gain_str[ch] = optarg;
						break;
					case 's':
						samp_rate[ch] = (uint32_t)atofs(optarg);
						break;
					case  'A':
						ant[ch] = optarg;
						break;
					case  'B':
						bw[ch] = atofs(optarg);
						break;
					}
				}
			}
			break;
		case 'd':
			dev_query = optarg;
			break;
		case 'f':
		  	for (ch = 0; ch < MAX_NUM_CHANNELS; ch++) {			  
				frequency[ch] = (uint32_t)atofs(optarg);
			}
			break;
		case 'g':
		        for (ch = 0; ch < MAX_NUM_CHANNELS; ch++) {
				gain_str[ch] = optarg;
			}
			break;
		case 's':
			for (ch = 0; ch < MAX_NUM_CHANNELS; ch++) {
				samp_rate[ch] = (uint32_t)atofs(optarg);
			}
			break;
		case 'p':
			ppm_error = atoi(optarg);
			break;
		case 'b':
			out_block_size = (uint32_t)atof(optarg);
			break;
		case 'n':
			// half of I/Q pair count (double for one each of I and Q)
			samples_to_read = (uint64_t)atofs(optarg) * 2;
			break;
		case 'S':
			sync_mode = 1;
			break;
		case 'A':
			for (ch = 0; ch < MAX_NUM_CHANNELS; ch++) {
				ant[ch] = optarg;
			}
			printf("Setting antenna to %s\n", optarg);
		        break;
		case 'B':
			for (ch = 0; ch < MAX_NUM_CHANNELS; ch++) {
				bw[ch] = atofs(optarg);
			}
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
                        printf("unknown option! - %d\n", opt);
			usage();
			break;
		}
	}

	if (argc != optind + nchan) {
                printf("\nNot enough args for channel count\n\n");
		usage();
	} else {
		for (ch = 0; ch < nchan; ch++) {
			filename[ch] = argv[optind + ch];
		}
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

	for(ch = 0; ch < nchan; ch++) {
		buffer_ch[ch] = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
		if (buffer_ch[ch] == NULL) {
			fprintf(stderr, "Failed to malloc data for buffer_ch[%d]!!\n", ch);
			exit(10);
		}
	}
	
	if (output_format == SOAPY_SDR_CS8 || output_format == SOAPY_SDR_CU8) {
		buf8 = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS8));
		if (buf8 == NULL) {
			fprintf(stderr, "Failed to malloc data for buf8!!\n");
			exit(10);
		}
	} else if (output_format == SOAPY_SDR_CF32) {
		fbuf = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CF32));
		if (fbuf == NULL) {
			fprintf(stderr, "Failed to malloc data for fbuf!!\n");
			exit(10);
		}
	}
	
	int tmp_stdout = suppress_stdout_start();
	// TODO: allow choosing input format, see https://www.reddit.com/r/RTLSDR/comments/4tpxv7/rx_tools_commandline_sdr_tools_for_rtlsdr_bladerf/d5ohfse?context=3
	
	if (nchan > 0) {
		channels = calloc(nchan, sizeof(size_t));
		if (channels == NULL) {
			fprintf(stderr, "Failed to malloc data for channels!!\n");
			exit(10);
		}
		for(ch = 0; ch < nchan; ch++) {
			channels[ch] = ch;
		}
	} else {
		channels = NULL;
	}
	
	r = verbose_device_search(dev_query, &dev, &stream, SOAPY_SDR_CS16, channels, nchan);
	
	if (r != 0) {
		fprintf(stderr, "Failed to open SDR device matching %s.\n", dev_query);
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
	
	for(ch = 0; ch < nchan; ch++) {
		if (bw[ch] < 0) {
			bw[ch] = 0.9 * samp_rate[ch];
		}
	}
	
	/* Set the sample rate */
	/* Set the frequency */
	/* Set the antenna */
	/* Set the bandwidth */
	if (channels == NULL) {
   	        verbose_set_sample_rate(dev, samp_rate[0], 0);
	        verbose_set_frequency(dev, frequency[0], 0);
	        verbose_set_antenna(dev, ant[0], 0);
		verbose_set_bandwidth(dev, bw[0], 0);
	  	if (NULL == gain_str[0]) {
		        /* Enable automatic gain */
			verbose_auto_gain(dev, 0);
		} else {
			/* Enable manual gain */
			verbose_gain_str_set(dev, gain_str[0], 0);
		}
		verbose_ppm_set(dev, ppm_error, 0);
	} else {
		for(ch = 0; ch < nchan; ch++) {
			verbose_set_sample_rate(dev, samp_rate[ch], channels[ch]);
			verbose_set_frequency(dev, frequency[ch], channels[ch]);
			verbose_set_antenna(dev, ant[ch], channels[ch]);
			verbose_set_bandwidth(dev, bw[ch], channels[ch]);
			if (NULL == gain_str[ch]) {
				/* Enable automatic gain */
				verbose_auto_gain(dev, channels[ch]);
			} else {
				/* Enable manual gain */
				verbose_gain_str_set(dev, gain_str[ch], channels[ch]);
			}
			verbose_ppm_set(dev, ppm_error, channels[ch]);
			
		}
	}
	
	
	for(ch = 0; ch < nchan; ch++) {
		if(strcmp(filename[ch], "-") == 0) { /* Write samples to stdout */
			file_ch[ch] = stdout;
#ifdef _WIN32
			_setmode(_fileno(stdin), _O_BINARY);
#endif
		} else {
			file_ch[ch] = fopen(filename[ch], "wb");
			if (!file_ch[ch]) {
				fprintf(stderr, "Failed to open %s\n", filename[ch]);
				goto out;
			}
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
			int flags = 0;
			long long timeNs = 0;
			long timeoutNs = 1000000;
			int64_t n_read = 0, r, i;
			
			r = SoapySDRDevice_readStream(dev, stream, (void *) buffer_ch, out_block_size,
						      &flags, &timeNs, timeoutNs);
			
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
			
			if ((samples_to_read > 0) && (samples_to_read < (uint64_t)n_read)) {
				n_read = samples_to_read;
				do_exit = 1;
			}
			
			// TODO: read these formats natively from SoapySDR (setupStream) instead of converting ourselves?
			for(ch = 0; ch < nchan; ch++) {
				if (output_format == SOAPY_SDR_CS16) {
					// The "native" format we read in, write out no conversion needed
					// (Always reading in CS16 to support >8-bit devices)
					if (fwrite(buffer_ch[ch], sizeof(int16_t), n_read, file_ch[ch]) != (size_t)n_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						do_exit = 1;
					}
					
				} else if (output_format == SOAPY_SDR_CS8) {
					for (i = 0; i < n_read; ++i) {
						buf8[i] = ( (int16_t)buffer_ch[ch][i] / 32767.0 * 128.0 + 0.4);
					}
					if (fwrite(buf8, sizeof(uint8_t), n_read, file_ch[ch]) != (size_t)n_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						do_exit = 1;
					}
				} else if (output_format == SOAPY_SDR_CU8) {
					for (i = 0; i < n_read; ++i) {
						buf8[i] = ( (int16_t)buffer_ch[ch][i] / 32767.0 * 128.0 + 127.4);
					}
					if (fwrite(buf8, sizeof(uint8_t), n_read, file_ch[ch]) != (size_t)n_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						do_exit = 1;
					}
				} else if (output_format == SOAPY_SDR_CF32) {
					for (i = 0; i < n_read; ++i) {
						fbuf[i] = buffer_ch[ch][i] * 1.0f / SHRT_MAX;
					}
					if (fwrite(fbuf, sizeof(float), n_read, file_ch[ch]) != (size_t)n_read) {
						fprintf(stderr, "Short write, samples lost, exiting!\n");
						do_exit = 1;
					}
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
	
	for(ch = 0; ch < nchan; ch++) {
		if (file_ch[ch] != stdout) {
			fclose(file_ch[ch]);
		}
	}
	
	SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
	SoapySDRDevice_closeStream(dev, stream);
	SoapySDRDevice_unmake(dev);
	
	for(ch = 0; ch < nchan; ch++) {
		free(buffer_ch[ch]);
	}
	
	if (buf8 != NULL) {
		free(buf8);
	}
	
	if (fbuf != NULL) {
		free(fbuf);
	}
 out:
	return r >= 0 ? r : -r;
}
