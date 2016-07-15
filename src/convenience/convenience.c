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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

int nearest_gain(SoapySDRDevice *dev, int target_gain)
{
	int i, r, err1, err2, nearest;
	/* TODO: what is equivalent of rtlsdr_set_tuner_gain_mode?
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	*/
	size_t count = 0;
        /* listGains isn't actually rtlsdr_get_tuner_gains() - it returns the
         * types gains you can set ("TUNER"), not the possible gain values!
	char **gains = SoapySDRDevice_listGains(dev, SOAPY_SDR_RX, 0, &count);

	if (count <= 0) {
		return 0;
	}

	fprintf(stderr, "Rx gains: ");
	nearest = atoi(gains[0]);

	for (size_t i = 0; i < count; i++) {
		fprintf(stderr, "%s, ", gains[i]);

		err1 = abs(target_gain - nearest);
		err2 = abs(target_gain - atoi(gains[i]));
		if (err2 < err1) {
			nearest = atoi(gains[i]);
		}
	}

	fprintf(stderr, "\n");

	SoapySDRStrings_clear(&gains, count);
        */
        // TODO: get possible gains
        nearest = target_gain;

	return nearest;
}

int verbose_set_frequency(SoapySDRDevice *dev, uint32_t frequency)
{
	int r;

	SoapySDRKwargs args = {};
	r = (int)SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, 0, (double)frequency, &args);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set center freq.\n");
	} else {
		fprintf(stderr, "Tuned to %u Hz.\n", frequency);
	}
	return r;
}

int verbose_set_sample_rate(SoapySDRDevice *dev, uint32_t samp_rate)
{
	int r;
	r = (int)SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, 0, (double)samp_rate);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");
	} else {
		fprintf(stderr, "Sampling at %u S/s.\n", samp_rate);
	}
	return r;
}

int verbose_set_bandwidth(SoapySDRDevice *dev, uint32_t bandwidth)
{
	int r;
	r = (int)SoapySDRDevice_setBandwidth(dev, SOAPY_SDR_RX, 0, (double)bandwidth);
	uint32_t applied_bw = 0;
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set bandwidth.\n");
	} else if (bandwidth > 0) {
		applied_bw = (uint32_t)SoapySDRDevice_getBandwidth(dev, SOAPY_SDR_RX, 0);
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
		fprintf(stderr, "WARNING: Failed to set direct sampling mode.\n");
		return r;
	}
	if (atoi(set_value) == 0) {
		fprintf(stderr, "Direct sampling mode disabled.\n");}
	if (atoi(set_value) == 1) {
		fprintf(stderr, "Enabled direct sampling mode, input 1/I.\n");}
	if (atoi(set_value) == 2) {
		fprintf(stderr, "Enabled direct sampling mode, input 2/Q.\n");}
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

int verbose_auto_gain(SoapySDRDevice *dev)
{
	int r;
	r = -1;
	/* TODO: not bridged, https://github.com/pothosware/SoapyRTLSDR/search?utf8=✓&q=rtlsdr_set_tuner_gain_mode
	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf(stderr, "Tuner gain set to automatic.\n");
	}
	*/
	return r;
}

int verbose_gain_set(SoapySDRDevice *dev, int gain)
{
	int r;
	/*
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	*/
	double value = gain / 10.0; // tenths of dB -> dB
	r = (int)SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, 0, value);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf(stderr, "Tuner gain set to %0.2f dB.\n", gain/10.0);
	}
	return r;
}

int verbose_ppm_set(SoapySDRDevice *dev, int ppm_error)
{
	int r;
	if (ppm_error == 0) {
		return 0;}
	r = (int)SoapySDRDevice_setFrequencyComponent(dev, SOAPY_SDR_RX, 0, "CORR", (double)ppm_error, NULL);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set ppm error.\n");
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

int verbose_device_search(char *s)
{
	size_t device_count = 0;
	size_t i = 0;
	int device, offset;
	char *s2;
	char vendor[256], product[256], serial[256];

	SoapySDRKwargs args = {}; // https://github.com/pothosware/SoapySDR/wiki/C_API_Example shows passing NULL, but crashes on 0.4.3 - this works
	SoapySDRKwargs *results = SoapySDRDevice_enumerate(&args, &device_count);
	if (!device_count) {
		fprintf(stderr, "No supported devices found.\n");
		return -1;
	}
	fprintf(stderr, "Found %zu device(s):\n", device_count);
	for (i = 0; i < device_count; i++) {
		//rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		//fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
		fprintf(stderr, "  %zu: ", i);
		for (size_t j = 0; j < results[i].size; j++)
		{
			fprintf(stderr, "%s=%s, ", results[i].keys[j], results[i].vals[j]);
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\n");

	// TODO: device search matching by properties above (key/value pairs), right now only returning zeroth device
	// example device:
	//   0: available=Yes, driver=rtlsdr, label=Generic RTL2832U OEM :: 3, manufacturer=Realtek, product=RTL2838UHIDIR, rtl=0, serial=3, tuner=Rafael Micro R820T,
	return 0;
#if 0
	/* does string look like raw id number */
	device = (int)strtol(s, &s2, 0);
	if (s2[0] == '\0' && device >= 0 && device < device_count) {
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string exact match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strcmp(s, serial) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string prefix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strncmp(s, serial, strlen(s)) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string suffix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		offset = strlen(serial) - strlen(s);
		if (offset < 0) {
			continue;}
		if (strncmp(s, serial+offset, strlen(s)) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	fprintf(stderr, "No matching devices found.\n");
#endif
	return -1;
}

// vim: tabstop=8:softtabstop=8:shiftwidth=8:noexpandtab
