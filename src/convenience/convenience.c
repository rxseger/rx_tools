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
#include <SoapySDR/Version.h>

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

int verbose_set_frequency(SoapySDRDevice *dev, uint32_t frequency, size_t channel)
{
	int r;

	SoapySDRKwargs args = {0};
	r = (int)SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, channel, (double)frequency, &args);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set center freq: %s\n", SoapySDRDevice_lastError());
	} else {
		fprintf(stderr, "Tuned to %u Hz.\n", frequency);
	}
	return r;
}

int verbose_set_sample_rate(SoapySDRDevice *dev, uint32_t samp_rate, size_t channel)
{
	int r;
	r = (int)SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, channel, (double)samp_rate);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set sample rate: %s\n", SoapySDRDevice_lastError());
	} else {
		fprintf(stderr, "Sampling at %u S/s.\n", samp_rate);
	}
	return r;
}

int verbose_set_bandwidth(SoapySDRDevice *dev, uint32_t bandwidth, size_t channel)
{
	int r;
	r = (int)SoapySDRDevice_setBandwidth(dev, SOAPY_SDR_RX, channel, (double)bandwidth);
	uint32_t applied_bw = 0;
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set bandwidth: %s\n", SoapySDRDevice_lastError());
	} else if (bandwidth > 0) {
		applied_bw = (uint32_t)SoapySDRDevice_getBandwidth(dev, SOAPY_SDR_RX, channel);
		if (applied_bw)
			fprintf(stderr, "Bandwidth parameter %u Hz resulted in %u Hz.\n", bandwidth, applied_bw);
		else
			fprintf(stderr, "Set bandwidth parameter %u Hz.\n", bandwidth);
	} else {
		fprintf(stderr, "Bandwidth set to automatic resulted in %u Hz.\n", applied_bw);
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
		fprintf(stderr, "WARNING: Failed to set direct sampling mode: %s\n", SoapySDRDevice_lastError());
		return r;
	}
	if (atoi(set_value) == 0) {
		fprintf(stderr, "Direct sampling mode disabled.\n");}
	if (atoi(set_value) == 1) {
		fprintf(stderr, "Enabled direct sampling mode, input 1/I.\n");}
	if (atoi(set_value) == 2) {
		fprintf(stderr, "Enabled direct sampling mode, input 2/Q.\n");}
	if (on == 3) {
		fprintf(stderr, "Enabled no-mod direct sampling mode.\n");}
	free(set_value);
	return r;
}

int verbose_offset_tuning(SoapySDRDevice *dev)
{
	int r = 0;
	SoapySDRDevice_writeSetting(dev, "offset_tune", "true");
	char *set_value = SoapySDRDevice_readSetting(dev, "offset_tune");

	if (set_value == NULL) {
		fprintf(stderr, "WARNING: Failed to set offset tuning: %s\n", SoapySDRDevice_lastError());
	} else if (strcmp(set_value, "true") != 0) {
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
	free(set_value);
	return r;
}

int verbose_auto_gain(SoapySDRDevice *dev, size_t channel)
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
		r = (int)SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, channel, 40.);
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set tuner gain: %s\n", SoapySDRDevice_lastError());
		} else {
			fprintf(stderr, "Tuner gain semi-automatically set to 40 dB\n");
		}
	} else if (strcmp(driver, "HackRF") == 0) {
		// HackRF has three gains LNA, VGA, and AMP, setting total distributes amongst, 116.0 dB seems to work well,
		// even though it logs HACKRF_ERROR_INVALID_PARAM? https://github.com/rxseger/rx_tools/issues/9
		// Total gain is distributed amongst all gains, 116 = 37,65,1; the LNA is OK (<40) but VGA is out of range (65 > 62)
		// TODO: generic means to set all gains, of any SDR? string parsing LNA=#,VGA=#,AMP=#?
		r = (int)SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, channel, "LNA", 40.); // max 40
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set LNA tuner gain: %s\n", SoapySDRDevice_lastError());
		}
		r = (int)SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, channel, "VGA", 20.); // max 65
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set VGA tuner gain: %s\n", SoapySDRDevice_lastError());
		}
		r = (int)SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, channel, "AMP", 0.); // on or off
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set AMP tuner gain: %s\n", SoapySDRDevice_lastError());
		}

	}
	// otherwise leave unset, hopefully the driver has good defaults
	free(driver);
	return r;
}

int verbose_gain_str_set(SoapySDRDevice *dev, char *gain_str, size_t channel)
{
	int r = 0;

	/* TODO: manual gain mode
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	*/

	if (strchr(gain_str, '=')) {
		// Set each gain individually (more control)
		SoapySDRKwargs args = SoapySDRKwargs_fromString(gain_str);

		for (size_t i = 0; i < args.size; ++i) {
			const char *name = args.keys[i];
			double value = atof(args.vals[i]);

			fprintf(stderr, "Setting gain element %s: %f dB\n", name, value);
			r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, channel, name, value);
			if (r != 0) {
				fprintf(stderr, "WARNING: setGainElement(%s, %f) failed: %s\n", name, value,  SoapySDRDevice_lastError());
			}
		}

		SoapySDRKwargs_clear(&args);
	} else {
		// Set overall gain and let SoapySDR distribute amongst components
		double value = atof(gain_str);
		r = SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, channel, value);
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set tuner gain: %s\n", SoapySDRDevice_lastError());
		} else {
			fprintf(stderr, "Tuner gain set to %0.2f dB.\n", value);
		}
		// TODO: read back and print each individual getGainElement()s
	}
	return r;
}

int verbose_antenna_str_set(SoapySDRDevice *dev, int channel, char *antenna_str)
{
	int r;
	fprintf(stderr, "Using antenna '%s' on channel %i\n", antenna_str, channel);
	r = SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, channel, antenna_str);
	return r;
}

int verbose_ppm_set(SoapySDRDevice *dev, int ppm_error, size_t channel)
{
	int r;
	if (ppm_error == 0) {
		return 0;}
	r = SoapySDRDevice_setFrequencyCorrection(dev, SOAPY_SDR_RX, channel, (double)ppm_error);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set ppm error: %s\n", SoapySDRDevice_lastError());
	} else {
		fprintf(stderr, "Tuner error set to %i ppm.\n", ppm_error);
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

int verbose_settings(SoapySDRDevice *dev, const char *sdr_settings)
{
	int status = 0;

	SoapySDRKwargs settings = SoapySDRKwargs_fromString(sdr_settings);
	for (size_t i = 0; i < settings.size; ++i) {
		const char *key = settings.keys[i];
		const char *value = settings.vals[i];
		fprintf(stderr, "set key=|%s|, value=|%s|\n", key, value);
		if(SoapySDRDevice_writeSetting(dev, key, value) != 0) {
			status = 1;
			fprintf(stderr, "WARNING: key set failed: %s\n", SoapySDRDevice_lastError());
		}
	}
	SoapySDRKwargs_clear(&settings);

	return status;
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

	size_t num_channels = SoapySDRDevice_getNumChannels(dev, SOAPY_SDR_RX);
	fprintf(stderr, "Found %zu channel(s) :\n", num_channels);
	for(int c = 0; c<num_channels; ++c){
		fprintf(stderr, "Channel %i :\n", c);
		antennas = SoapySDRDevice_listAntennas(dev, direction, channel, &len);
		fprintf(stderr, "  Found %zu antenna(s): ", len);
		for (i = 0; i < len; ++i) {
			fprintf(stderr, "%s ", antennas[i]);
		}
		fprintf(stderr, "\n");


		gains = SoapySDRDevice_listGains(dev, direction, channel, &len);
		fprintf(stderr, "  Found %zu gain(s): ", len);
		for (i = 0; i < len; ++i) {
			fprintf(stderr, "%s ", gains[i]);
		}
		fprintf(stderr, "\n");

		frequencies = SoapySDRDevice_listFrequencies(dev, direction, channel, &len);
		fprintf(stderr, "  Found %zu frequencies: ", len);
		for (i = 0; i < len; ++i) {
			fprintf(stderr, "%s ", frequencies[i]);
		}
		fprintf(stderr, "\n");

		rates = SoapySDRDevice_listSampleRates(dev, direction, channel, &len);
		fprintf(stderr, "  Found %zu sample rates: ", len);
		for (i = 0; i < len; ++i) {
			fprintf(stderr, "%.0f ", rates[i]);
		}
		fprintf(stderr, "\n");

		bandwidths = SoapySDRDevice_listBandwidths(dev, direction, channel, &len);
		fprintf(stderr, "  Found %zu bandwidths: ", len);
		for (i = 0; i < len; ++i) {
			fprintf(stderr, "%.0f ", bandwidths[i]);
		}
		fprintf(stderr, "\n");
	}
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


int verbose_device_search(char *s, SoapySDRDevice **devOut)
{
	size_t device_count = 0;
	size_t i = 0;
	int device, offset;
	char *s2;
	char vendor[256], product[256], serial[256];
	SoapySDRDevice *dev = NULL;


	dev = SoapySDRDevice_makeStrArgs(s);
	if (!dev) {
		fprintf(stderr, "SoapySDRDevice_make failed\n");
		return -1;
	}

	show_device_info(dev);

	*devOut = dev;
	return 0;
}

int verbose_setup_stream(SoapySDRDevice *dev, SoapySDRStream **streamOut, size_t *channels, size_t num_channels, const char *format)
{
	SoapySDRKwargs stream_args = {0};

	size_t max_dev_channels = SoapySDRDevice_getNumChannels(dev, SOAPY_SDR_RX);
	for (size_t idx=0; idx<num_channels; ++idx) {
		if (channels[idx] >= max_dev_channels) {
			fprintf(stderr, "Invalid channel %d selected\n", channels[idx]);
			return -3;
		}
	}
	*streamOut = SoapySDRDevice_setupStream(dev, SOAPY_SDR_RX, format, channels, num_channels, &stream_args);
	if (*streamOut == NULL) {
		fprintf(stderr, "SoapySDRDevice_setupStream failed: %s\n", SoapySDRDevice_lastError());
		return -3;
	}
	return 0;
}

int verbose_set_properties(SoapySDRDevice *dev, uint32_t samp_rate, int frequency, char *gain_str, char *antenna_str, int ppm_error, size_t channel) {

	int r = 0;

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
}

// vim: tabstop=8:softtabstop=8:shiftwidth=8:noexpandtab
