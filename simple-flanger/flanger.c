/*
  Copyright 2017 Amaury ABRIAL <yruama_lairba@hotmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** Include standard C headers */
#include <math.h>
#include <stdlib.h>

/**
   LV2 headers are based on the URI of the specification they come from, so a
   consistent convention can be used even for unofficial extensions.  The URI
   of the core LV2 specification is <http://lv2plug.in/ns/lv2core>, by
   replacing `http:/` with `lv2` any header in the specification bundle can be
   included, in this case `lv2.h`.
*/
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

/**
   The URI is the identifier for a plugin, and how the host associates this
   implementation in code with its description in data.  In this plugin it is
   only used once in the code, but defining the plugin URI at the top of the
   file is a good convention to follow.  If this URI does not match that used
   in the data files, the host will fail to load the plugin.
*/
#define FLANGER_URI "https://github.com/YruamaLairba/yru-simple-LV2-C#simple-flanger"

/**
   In code, ports are referred to by index.  An enumeration of port indices
   should be defined for readability.
*/
typedef enum {
	FLANGER_RATE = 0,
	FLANGER_DEPTH = 1,
	FLANGER_MIX = 2,
	FLANGER_INPUT  = 3,
	FLANGER_OUTPUT = 4
} PortIndex;

/**
   Every plugin defines a private structure for the plugin instance.  All data
   associated with a plugin instance is stored here, and is available to
   every instance method.  In this simple plugin, only port buffers need to be
   stored, since there is no additional instance data.
*/
#define MAX_FLANGER_AMPLITUDE_MS 30
#define ADDITIONAL_DELAY_MS 10


typedef struct {
	// Port buffers
	const float* rate;
	const float* depth;
	const float* mix;
	const float* input;
	float*       output;
	// Internal data
	float* delay_buffer;
	unsigned int delay_buffer_size;
	unsigned int write_head;
	float progression;
	double sampling_rate;
} Flanger;

/**
   The `instantiate()` function is called by the host to create a new plugin
   instance.  The host passes the plugin descriptor, sample rate, and bundle
   path for plugins that need to load additional resources (e.g. waveforms).
   The features parameter contains host-provided features defined in LV2
   extensions, but this simple plugin does not use any.

   This function is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    sampling_rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	Flanger* flanger = (Flanger*)calloc(1, sizeof(Flanger));
	flanger->sampling_rate = sampling_rate;
	flanger->delay_buffer_size = 1 + (sampling_rate * MAX_FLANGER_AMPLITUDE_MS
		* ADDITIONAL_DELAY_MS)/1000.0f;
	flanger->delay_buffer = (float*)calloc(flanger->delay_buffer_size,
	                                    sizeof(float));

	return (LV2_Handle)flanger;
}

/**
   The `connect_port()` method is called by the host to connect a particular
   port to a buffer.  The plugin must store the data location, but data may not
   be accessed except in run().

   This method is in the ``audio'' threading class, and is called in the same
   context as run().
*/
static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	Flanger* flanger = (Flanger*)instance;

	switch ((PortIndex)port) {
	case FLANGER_RATE:
		flanger->rate = (const float*)data;
		break;
	case FLANGER_DEPTH:
		flanger->depth = (const float*)data;
	case FLANGER_MIX:
		flanger->mix = (const float*)data;
	case FLANGER_INPUT:
		flanger->input = (const float*)data;
		break;
	case FLANGER_OUTPUT:
		flanger->output = (float*)data;
		break;
	}
}

/**
   The `activate()` method is called by the host to initialise and prepare the
   plugin instance for running.  The plugin must reset all internal state
   except for buffer locations set by `connect_port()`.  Since this plugin has
   no other internal state, this method does nothing.

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void
activate(LV2_Handle instance)
{
}

/** Define a macro for converting a gain in dB to a coefficient. */
#define DB_CO(g) ((g) > -90.0f ? powf(10.0f, (g) * 0.05f) : 0.0f)

/** PI constant */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif//M_PI

/**
   The `run()` method is the main process function of the plugin.  It processes
   a block of audio in the audio context.  Since this plugin is
   `lv2:hardRTCapable`, `run()` must be real-time safe, so blocking (e.g. with
   a mutex) or memory allocation are not allowed.
*/
static void
run(LV2_Handle instance, uint32_t n_samples)
{
	Flanger* flanger = (Flanger*)instance;

	// Port
	const float rate = *(flanger->rate);
	const float depth = *(flanger->depth);
	const float mix = *(flanger->mix);
	const float* const input  = flanger->input;
	float* const       output = flanger->output;
	// Internal data
	float * const delay_buffer = flanger->delay_buffer;
	unsigned int delay_buffer_size = flanger->delay_buffer_size;
	double sampling_rate = flanger->sampling_rate;
	float progression = flanger->progression;

	float delta = rate / (float)sampling_rate;

	for (uint32_t pos = 0; pos < n_samples; pos++) {
		float input_sample = input[pos];
		delay_buffer[flanger->write_head] = input_sample;

		float modulant =  0.5f * ( 1.0f +
			(sinf(2.0f * (float)M_PI * progression)));
		progression += delta;
		if (progression > 1.0f) {
			progression += -1.0f;
		}

		float delay_in_sample = ((depth * modulant  * 
			(float)MAX_FLANGER_AMPLITUDE_MS) + (float)ADDITIONAL_DELAY_MS) *
			(float)sampling_rate / 1000.0f;

		float delay_in_sample_i; //integral part
		float delay_in_sample_d; //decimal part
		delay_in_sample_d = modff(delay_in_sample, &delay_in_sample_i);

		int read_head_a = flanger->write_head - (int)delay_in_sample_i;
		//int read_head_b = read_head_a - 1;
		if (read_head_a < 0) read_head_a += delay_buffer_size;
		int read_head_b = read_head_a - 1;
		if (read_head_b < 0) read_head_b += delay_buffer_size;

		float sample_a = delay_buffer[read_head_a];
		float sample_b = delay_buffer[read_head_b];
		float interpolated_sample = (1.0f - delay_in_sample_d) * sample_a + 
			delay_in_sample_d * sample_b;

		float output_sample = 0.5f * 
			((1.0f - mix)* input_sample + mix * interpolated_sample);

		flanger->write_head++;
		if (flanger->write_head >= delay_buffer_size) {
			flanger->write_head -= delay_buffer_size;
		}
		output[pos] = output_sample;
		flanger->progression = progression;


	}
}

/**
   The `deactivate()` method is the counterpart to `activate()`, and is called by
   the host after running the plugin.  It indicates that the host will not call
   `run()` again until another call to `activate()` and is mainly useful for more
   advanced plugins with ``live'' characteristics such as those with auxiliary
   processing threads.  As with `activate()`, this plugin has no use for this
   information so this method does nothing.

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void
deactivate(LV2_Handle instance)
{
}

/**
   Destroy a plugin instance (counterpart to `instantiate()`).

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void
cleanup(LV2_Handle instance)
{
	Flanger* flanger = (Flanger*)instance;
	flanger->delay_buffer_size = 0;
	free(flanger->delay_buffer);
	free(instance);
}

/**
   The `extension_data()` function returns any extension data supported by the
   plugin.  Note that this is not an instance method, but a function on the
   plugin descriptor.  It is usually used by plugins to implement additional
   interfaces.  This plugin does not have any extension data, so this function
   returns NULL.

   This method is in the ``discovery'' threading class, so no other functions
   or methods in this plugin library will be called concurrently with it.
*/
static const void*
extension_data(const char* uri)
{
	return NULL;
}

/**
   Every plugin must define an `LV2_Descriptor`.  It is best to define
   descriptors statically to avoid leaking memory and non-portable shared
   library constructors and destructors to clean up properly.
*/
static const LV2_Descriptor descriptor = {
	FLANGER_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

/**
   The `lv2_descriptor()` function is the entry point to the plugin library.  The
   host will load the library and call this function repeatedly with increasing
   indices to find all the plugins defined in the library.  The index is not an
   indentifier, the URI of the returned descriptor is used to determine the
   identify of the plugin.

   This method is in the ``discovery'' threading class, so no other functions
   or methods in this plugin library will be called concurrently with it.
*/
LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:  return &descriptor;
	default: return NULL;
	}
}
