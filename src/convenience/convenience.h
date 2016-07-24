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
#ifndef __CONVENIENCE_H
#define __CONVENIENCE_H

#ifdef _MSC_VER
#define STDOUT_FILENO fileno(stdout)
#define STDERR_FILENO fileno(stderr)
#define bool _Bool
#define false 0
#define true 1
#define strcasecmp _stricmp
char * strsep(char **sp, char *sep);
#include <time.h>
struct tm *localtime_r (const time_t *timer, struct tm *result);
#endif

#include <stdint.h>
#include <SoapySDR/Device.h>


/* a collection of user friendly tools */

/*!
 * Convert standard suffixes (k, M, G) to double
 *
 * \param s a string to be parsed
 * \return double
 */

double atofs(char *s);

/*!
 * Convert time suffixes (s, m, h) to double
 *
 * \param s a string to be parsed
 * \return seconds as double
 */

double atoft(char *s);

/*!
 * Convert percent suffixe (%) to double
 *
 * \param s a string to be parsed
 * \return double
 */

double atofp(char *s);

/*!
 * Set device frequency and report status on stderr
 *
 * \param dev the device handle
 * \param frequency in Hz
 * \return 0 on success
 */

int verbose_set_frequency(SoapySDRDevice *dev, uint32_t frequency);

/*!
 * Set device sample rate and report status on stderr
 *
 * \param dev the device handle
 * \param samp_rate in samples/second
 * \return 0 on success
 */

int verbose_set_sample_rate(SoapySDRDevice *dev, uint32_t samp_rate);

/*!
 * Set device bandwidth and report status on stderr
 *
 * \param dev the device handle
 * \param frequency in Hz
 * \return 0 on success
 */

int verbose_set_bandwidth(SoapySDRDevice *dev, uint32_t bandwidth);


/*!
 * Enable or disable the direct sampling mode and report status on stderr
 *
 * \param dev the device handle
 * \param on 0 means disabled, 1 I-ADC input enabled, 2 Q-ADC input enabled
 * \return 0 on success
 */

int verbose_direct_sampling(SoapySDRDevice *dev, int on);

/*!
 * Enable offset tuning and report status on stderr
 *
 * \param dev the device handle
 * \return 0 on success
 */

int verbose_offset_tuning(SoapySDRDevice *dev);

/*!
 * Enable auto gain and report status on stderr
 *
 * \param dev the device handle
 * \return 0 on success
 */

int verbose_auto_gain(SoapySDRDevice *dev);

/*!
 * Set tuner gain and report status on stderr
 *
 * \param dev the device handle
 * \param gain in tenths of a dB
 * \return 0 on success
 */

int verbose_gain_set(SoapySDRDevice *dev, int gain);

/*!
 * Set tuner gain elements by a key/value string
 *
 * \param dev the device handle
 * \param gain_str string of gain element pairs (example LNA=40,VGA=20,AMP=0), or string of overall gain, in dB
 * \return 0 on success
 */
int verbose_gain_str_set(SoapySDRDevice *dev, char *gain_str);

/*!
 * Set the frequency correction value for the device and report status on stderr.
 *
 * \param dev the device handle
 * \param ppm_error correction value in parts per million (ppm)
 * \return 0 on success
 */

int verbose_ppm_set(SoapySDRDevice *dev, int ppm_error);

/*!
 * Reset buffer
 *
 * \param dev the device handle
 * \return 0 on success
 */

int verbose_reset_buffer(SoapySDRDevice *dev);

/*!
 * Find the closest matching device.
 *
 * \param s a string to be parsed
 * \param devOut device output returned
 * \param streamOut stream output returned
 * \param format stream format (such as SOAPY_SDR_CS16)
 * \return dev 0 if successful
 */

int verbose_device_search(char *s, SoapySDRDevice **devOut, SoapySDRStream **streamOut, const char *format);

/*!
 * Start redirecting stdout to stderr to avoid unwanted stdout emissions.
 * Applications should call this if they want to use stdout for their own output,
 * before verbose_device_start(), and optionally stop after configuring all settings.
 *
 * \return Saved file descriptor to pass to suppress_stdout_stop()
 */
int suppress_stdout_start(void);

/*!
 * Stop redirecting stdout to stderr.
 *
 * \param tmp_stdout File descriptor from suppress_stdout_start()
 */
void suppress_stdout_stop(int tmp_stdout);

/*!
 * Parse a comma-separated list of key/value pairs into SoapySDRKwargs
 *
 * \param s String of key=value pairs, separated by commas
 * \param args Parsed keyword arguments
 */
void parse_kwargs(char *s, SoapySDRKwargs *args);

#endif /*__CONVENIENCE_H*/
