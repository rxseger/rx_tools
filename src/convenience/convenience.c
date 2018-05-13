/*
 * Copyright (C) 2014 by Kyle Keen <keenerd@gmail.com>
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

/* a collection of user friendly tools
 * todo: use strtol for more flexible int parsing
 * */

#include "convenience.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#define _USE_MATH_DEFINES
#endif

#include <math.h>

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#ifdef _MSC_VER
struct tm *localtime_r (const time_t *timer, struct tm *result)
{
	struct tm *local_result = localtime (timer);
	if (local_result == NULL || result == NULL) return NULL;
	memcpy (result, local_result, sizeof (struct tm));
	return result;
}

//http://unixpapa.com/incnote/string.html
char * strsep(char **sp, char *sep)
{
	char *p, *s;
	if (sp == NULL || *sp == NULL || **sp == '\0') return(NULL);
	s = *sp;
	p = s + strcspn(s, sep);
	if (*p != '\0') *p++ = '\0';
	*sp = p;
	return(s);
}
#endif

double atofs(char *s)
/* standard suffixes */
{
	char last;
	int len;
	double suff = 1.0;
	len = strlen(s);
	last = s[len-1];
	s[len-1] = '\0';
	switch (last) {
		case 'g':
		case 'G':
			suff *= 1e3;
		case 'm':
		case 'M':
			suff *= 1e3;
		case 'k':
		case 'K':
			suff *= 1e3;
			suff *= atof(s);
			s[len-1] = last;
			return suff;
	}
	s[len-1] = last;
	return atof(s);
}

double atoft(char *s)
/* time suffixes, returns seconds */
{
	char last;
	int len;
	double suff = 1.0;
	len = strlen(s);
	last = s[len-1];
	s[len-1] = '\0';
	switch (last) {
		case 'h':
		case 'H':
			suff *= 60;
		case 'm':
		case 'M':
			suff *= 60;
		case 's':
		case 'S':
			suff *= atof(s);
			s[len-1] = last;
			return suff;
	}
	s[len-1] = last;
	return atof(s);
}

double atofp(char *s)
/* percent suffixes */
{
	char last;
	int len;
	double suff = 1.0;
	len = strlen(s);
	last = s[len-1];
	s[len-1] = '\0';
	switch (last) {
		case '%':
			suff *= 0.01;
			suff *= atof(s);
			s[len-1] = last;
			return suff;
	}
	s[len-1] = last;
	return atof(s);
}

int verbose_set_frequency(SoapySDRDevice *dev, uint32_t frequency, size_t chan)
{
	int r;
	
	SoapySDRKwargs args = {0};
        r = (int)SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, chan, (double)frequency, &args);
        if (r != 0) {
                fprintf(stderr, "ERROR: Failed to set center freq. for chan %zu\n", chan);
		exit(10);
        } else {
                fprintf(stderr, "Tuned to %u Hz on channel %zu.\n", frequency, chan);
        }

	return r;
}

int verbose_set_antenna(SoapySDRDevice *dev, const char * ant, size_t chan)
{
	int r;
	if (ant != NULL) {
		r = (int)SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, chan, ant);
	} else {
		r = 0;
	}
	
	if (r != 0) {
		fprintf(stderr, "ERROR: Failed to set antenna for chan %zu.\n", chan);
                exit(11);
	} else {
	        char * antval = SoapySDRDevice_getAntenna(dev, SOAPY_SDR_RX, chan);
		fprintf(stderr, "****Antenna set to %s on chan %zu\n", antval, chan);
	}

	return r;
}

int verbose_set_sample_rate(SoapySDRDevice *dev, uint32_t samp_rate, size_t chan)
{
	int r;
	r = (int)SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, chan, (double)samp_rate);
	if (r != 0) {
		fprintf(stderr, "ERROR: Failed to set sample rate for chan %zu.\n", chan);
                exit(12);
	} else {
		fprintf(stderr, "Sampling at %u S/s on chan %zu.\n", samp_rate, chan);
	}

	return r;
}

int verbose_set_bandwidth(SoapySDRDevice *dev, uint32_t bandwidth, size_t chan)
{
	int r;
	r = (int)SoapySDRDevice_setBandwidth(dev, SOAPY_SDR_RX, chan, (double)bandwidth);
	uint32_t applied_bw = 0;
	if (r != 0) {
		fprintf(stderr, "ERROR: Failed to set bandwidth on chan %zu.\n", chan);
                exit(13);
	} else if (bandwidth > 0) {
		applied_bw = (uint32_t)SoapySDRDevice_getBandwidth(dev, SOAPY_SDR_RX, chan);
		if (applied_bw)
			fprintf(stderr, "Bandwidth parameter %u Hz resulted in %u Hz on chan %zu.\n",
				bandwidth, applied_bw, chan);
		else
			fprintf(stderr, "Set bandwidth parameter %u Hz on chan %zu.\n", bandwidth, chan);
	} else {
		fprintf(stderr, "Bandwidth set to automatic resulted in %u Hz on chan %zu.\n",
			applied_bw, chan);
	}
	return r;
}

int verbose_direct_sampling(SoapySDRDevice *dev, int on)
{
	int r;
	char *value, *set_value;
	if (on == 0)
		value = "0";
	else if (on == 1)
		value = "1";
	else if (on == 2)
		value = "2";
	else
		return -1;
	SoapySDRDevice_writeSetting(dev, "direct_samp", value);
	set_value = SoapySDRDevice_readSetting(dev, "direct_samp");

	if (set_value == NULL) {
		fprintf(stderr, "ERROR: Failed to set direct sampling mode.\n");
		exit(14);
	}
	if (atoi(set_value) == 0) {
		fprintf(stderr, "Direct sampling mode disabled.\n");}
	if (atoi(set_value) == 1) {
		fprintf(stderr, "Enabled direct sampling mode, input 1/I.\n");}
	if (atoi(set_value) == 2) {
		fprintf(stderr, "Enabled direct sampling mode, input 2/Q.\n");}
	if (on == 3) {
		fprintf(stderr, "Enabled no-mod direct sampling mode.\n");}
	return r;
}

int verbose_offset_tuning(SoapySDRDevice *dev)
{
	int r = 0;
	SoapySDRDevice_writeSetting(dev, "offset_tune", "true");
	char *set_value = SoapySDRDevice_readSetting(dev, "offset_tune");

	if (strcmp(set_value, "true") != 0) {
		/* TODO: detection of failure modes
		if ( r == -2 )
			fprintf(stderr, "WARNING: Failed to set offset tuning: tuner doesn't support offset tuning!\n");
		else if ( r == -3 )
			fprintf(stderr, "WARNING: Failed to set offset tuning: direct sampling not combinable with offset tuning!\n");
		else
		*/
			fprintf(stderr, "WARNING: Failed to set offset tuning.\n");
	} else {
		fprintf(stderr, "Offset tuning mode enabled.\n");
	}
	return r;
}

int verbose_auto_gain(SoapySDRDevice *dev, size_t chan)
{
	int r;
	r = 0;
	/* TODO: not bridged, https://github.com/pothosware/SoapyRTLSDR/search?utf8=✓&q=rtlsdr_set_tuner_gain_mode
	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf(stderr, "Tuner gain set to automatic.\n");
	}
	*/

	// Per-driver hacks TODO: clean this up
	char *driver = SoapySDRDevice_getDriverKey(dev);
	if (strcmp(driver, "RTLSDR") == 0) {
		// For now, set 40.0 dB, high
		// Note: 26.5 dB in https://github.com/librtlsdr/librtlsdr/blob/master/src/tuner_r82xx.c#L1067 - but it's not the same
		// TODO: remove or change after auto-gain? https://github.com/pothosware/SoapyRTLSDR/issues/21 rtlsdr_set_tuner_gain_mode(dev, 0);
		r = (int)SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, chan, 40.);
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
		} else {
			fprintf(stderr, "Tuner gain semi-automatically set to 40 dB\n");
		}
	} else if (strcmp(driver, "HackRF") == 0) {
		// HackRF has three gains LNA, VGA, and AMP, setting total distributes amongst, 116.0 dB seems to work well,
		// even though it logs HACKRF_ERROR_INVALID_PARAM? https://github.com/rxseger/rx_tools/issues/9
		// Total gain is distributed amongst all gains, 116 = 37,65,1; the LNA is OK (<40) but VGA is out of range (65 > 62)
		// TODO: generic means to set all gains, of any SDR? string parsing LNA=#,VGA=#,AMP=#?
		r = (int)SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, chan, "LNA", 40.); // max 40
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set LNA tuner gain.\n");
		}
		r = (int)SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, chan, "VGA", 20.); // max 65
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set VGA tuner gain.\n");
		}
		r = (int)SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, chan, "AMP", 0.); // on or off
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set AMP tuner gain.\n");
		}

	}
	// otherwise leave unset, hopefully the driver has good defaults

	return r;
}

int verbose_gain_str_set(SoapySDRDevice *dev, char *gain_str, size_t chan)
{
	SoapySDRKwargs args = {0};
	size_t i;
	int r;

	/* TODO: manual gain mode
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	*/

	if (strchr(gain_str, '=')) {
		// Set each gain individually (more control)
		parse_kwargs(gain_str, &args);

		for (i = 0; i < args.size; ++i) {
			char *name = args.keys[i];
			double value = atof(args.vals[i]);

                        fprintf(stderr, "Setting gain element %s: %f dB on chan %zu\n", name, value, chan);
                        r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, chan, name, value);
                        if (r != 0) {
                                fprintf(stderr, "WARNING: setGainElement(%s, %f, %zu) failed: %d\n",
					name, value, chan, r);
                        }
  
		}
	} else {
		// Set overall gain and let SoapySDR distribute amongst components
		double value = atof(gain_str);
		r = SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, chan, value);
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set tuner gain for chan %zu.\n", chan);
		} else {
			fprintf(stderr, "Tuner gain set to %0.2f dB on chan %zu.\n", value, chan);
		}

		// TODO: read back and print each individual getGainElement()s
	}

	return r;
}

int verbose_ppm_set(SoapySDRDevice *dev, int ppm_error, size_t chan)
{
	int r;
	if (ppm_error == 0) {
		return 0;}
	r = (int)SoapySDRDevice_setFrequencyComponent(dev, SOAPY_SDR_RX, chan, "CORR", (double)ppm_error, NULL);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set ppm error for chan %zu.\n", chan);
	} else {
		fprintf(stderr, "Tuner error set to %i ppm for chan %zu.\n", ppm_error, chan);
	}
	return r;
}

int verbose_reset_buffer(SoapySDRDevice *dev)
{
	int r;
	r = -1;
	/* TODO: not bridged
	r = rtlsdr_reset_buffer(dev);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to reset buffers.\n");}
		*/
	return r;
}

static void show_device_info(SoapySDRDevice *dev)
{
	size_t len = 0, i = 0;
	char **antennas = NULL;
	char **gains = NULL;
	char **frequencies = NULL;
	double *rates = NULL;
	double *bandwidths = NULL;
	SoapySDRKwargs args;
	char *hwkey = NULL;

	int direction = SOAPY_SDR_RX;
	int channel = 0;

	hwkey = SoapySDRDevice_getHardwareKey(dev);
	fprintf(stderr, "Using device %s: ", hwkey);

	args = SoapySDRDevice_getHardwareInfo(dev);
	for (i = 0; i < args.size; ++i) {
		fprintf(stderr, "%s=%s ", args.keys[i], args.vals[i]);
	}
	fprintf(stderr, "\n");

	antennas = SoapySDRDevice_listAntennas(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu antenna(s): ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%s ", antennas[i]);
	}
	fprintf(stderr, "\n");


	gains = SoapySDRDevice_listGains(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu gain(s): ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%s ", gains[i]);
	}
	fprintf(stderr, "\n");

	frequencies = SoapySDRDevice_listFrequencies(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu frequencies: ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%s ", frequencies[i]);
	}
	fprintf(stderr, "\n");

	rates = SoapySDRDevice_listSampleRates(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu sample rates: ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%.0f ", rates[i]);
	}
	fprintf(stderr, "\n");

	bandwidths = SoapySDRDevice_listBandwidths(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu bandwidths: ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%.0f ", bandwidths[i]);
	}
	fprintf(stderr, "\n");

	args = SoapySDRDevice_getChannelInfo(dev, direction, channel);
	fprintf(stderr, "Channel info:    ");
	for (i = 0; i < args.size; ++i) {
		fprintf(stderr, "%s=%s ", args.keys[i], args.vals[i]);
	}
	fprintf(stderr, "\n");

}

int suppress_stdout_start(void) {
	// Hack to redirect stdout to stderr so it doesn't interfere with audio output on stdout
	// see https://github.com/rxseger/rx_tools/pull/11#issuecomment-233168397
	// because SoapySDR and UHD log there, TOOO: change in SoapySDR_Log?
	// This is restored after stream setup, if it successful.
	int tmp_stdout = dup(STDOUT_FILENO);
	if (dup2(STDERR_FILENO, STDOUT_FILENO) != STDOUT_FILENO) {
		perror("dup2 start");
	}

	return tmp_stdout;
}

void suppress_stdout_stop(int tmp_stdout) {
	// Restore stdout back to stdout
	fflush(stdout);
	if (dup2(tmp_stdout, STDOUT_FILENO) != STDOUT_FILENO) {
		perror("dup2 stop");
	}
}


int verbose_device_search(char *s, SoapySDRDevice **devOut, SoapySDRStream **streamOut, const char *format,
			  const size_t * channels, size_t nchannels)
{
	size_t device_count = 0;
	size_t i = 0;
	int device, offset;
	char *s2;
	char vendor[256], product[256], serial[256];
	SoapySDRDevice *dev = NULL;

	SoapySDRKwargs stream_args = {0};

	dev = SoapySDRDevice_makeStrArgs(s);
	if (!dev) {
		fprintf(stderr, "SoapySDRDevice_make failed\n");
		return -1;
	}

	show_device_info(dev);
	if (nchannels > SoapySDRDevice_getNumChannels(dev, SOAPY_SDR_RX)) {
	  fprintf(stderr, "User asked for more channels (%d) than device supports (%d)\n",
		  nchannels, SoapySDRDevice_getNumChannels(dev, SOAPY_SDR_RX));
	  return -5;
	}
	
	if (SoapySDRDevice_setupStream(dev, streamOut, SOAPY_SDR_RX, format, channels, nchannels,
				       &stream_args) != 0) {
		fprintf(stderr, "SoapySDRDevice_setupStream failed\n");
		return -3;
	}

	*devOut = dev;
	return 0;
}

void parse_kwargs(char *s, SoapySDRKwargs *args)
{
	char *copied, *cursor, *pair, *equals;

	copied = strdup(s);
	cursor = copied;
	while ((pair = strsep(&cursor, ",")) != NULL) {
		char *key, *value;
		//printf("pair = %s\n", pair);

		equals = strchr(pair, '=');
		if (equals) {
			key = pair;
			*equals = '\0';
			value = equals + 1;
		} else {
			key = pair;
			value = "";
		}
		//printf("key=|%s|, value=|%s|\n", key, value);
		SoapySDRKwargs_set(args, key, value);
	}

	free(copied);
}

// vim: tabstop=8:softtabstop=8:shiftwidth=8:noexpandtab
