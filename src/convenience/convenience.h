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
 * Find nearest supported gain
 *
 * \param dev the device handle
 * \param target_gain in tenths of a dB
 * \return 0 on success
 */

int nearest_gain(SoapySDRDevice *dev, int target_gain);

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
 * \return dev device, NULL on error
 */

SoapySDRDevice *verbose_device_search(char *s);

/*!
 * Read samples as Complex Signed 16-bit (CS16) pairs
 *
 * \param dev the device handle
 * \param stream the stream handle
 * \param buf buffer to read into
 * \param len maximum number of elements in buf
 * \return number of bytes read, or negative if an error
 */
int read_samples_cs16(SoapySDRDevice *dev, SoapySDRStream *stream, int16_t *buf, int len);

#endif /*__CONVENIENCE_H*/
