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
#define TREMOLO_URI "https://github.com/YruamaLairba/yru-simple-LV2-C#simple-tremolo"

/**
   In code, ports are referred to by index.  An enumeration of port indices
   should be defined for readability.
*/
typedef enum {
	TREMOLO_DELAY   = 0,
	TREMOLO_DEPTH = 1,
	TREMOLO_INPUT  = 2,
	TREMOLO_OUTPUT = 3
} PortIndex;

/**
   Every plugin defines a private structure for the plugin instance.  All data
   associated with a plugin instance is stored here, and is available to
   every instance method.  In this simple plugin, only port buffers need to be
   stored, since there is no additional instance data.
*/
//const unsigned int max_rate_in_sample = 44100;
//const unsigned int rate_buffer_size = max_rate_in_sample + 1;
#define MAX_DELAY_IN_SEC 1
#define MAX_DELAY_IN_SAMPLE 44100  // 1 sec at 44100hz
#define DELAY_BUFFER_SIZE (MAX_DELAY_IN_SAMPLE + 1)


typedef struct {
	// Port buffers
	const float* rate;
	const float* depth;
	const float* input;
	float*       output;
	// Internal values
	float progression;
	double sample_rate;
} Tremolo;

/**
   The `instantiate()` function is called by the host to create a new plugin
   instance.  The host passes the plugin descriptor, sample_rate, and bundle
   path for plugins that need to load additional resources (e.g. waveforms).
   The features parameter contains host-provided features defined in LV2
   extensions, but this simple plugin does not use any.

   This function is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    sample_rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	Tremolo* tremolo = (Tremolo*)calloc(1, sizeof(Tremolo));
	tremolo->progression=0;
	tremolo->sample_rate = sample_rate;


	return (LV2_Handle)tremolo;
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
	Tremolo* tremolo = (Tremolo*)instance;

	switch ((PortIndex)port) {
	case TREMOLO_DELAY:
		tremolo->rate = (const float*)data;
		break;
	case TREMOLO_DEPTH:
		tremolo->depth = (const float*)data;
	case TREMOLO_INPUT:
		tremolo->input = (const float*)data;
		break;
	case TREMOLO_OUTPUT:
		tremolo->output = (float*)data;
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
	//const Tremolo* tremolo = (const Tremolo*)instance;
	Tremolo* tremolo = (Tremolo*)instance;

	//Port
	const float        rate   = *(tremolo->rate);
	const float        depth = *(tremolo->depth);
	const float* const input  = tremolo->input;
	float* const       output = tremolo->output;
	//internal value
	float progression = tremolo->progression;
	double sample_rate = tremolo->sample_rate;

	float delta = rate / (float)sample_rate;

	for (uint32_t pos = 0; pos < n_samples; pos++) {
		float input_sample = input[pos];
		float modulant = (1.0f-depth*0.5f) + depth*0.5f * (
			sinf(2.0f * (float)M_PI * progression));
		output[pos] = input_sample * modulant;
		progression += delta;
		if (progression > 1.0f) {
			progression += -1.0f;
		}
	}
	tremolo->progression = progression;
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
	Tremolo* tremolo = (Tremolo*)instance;
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
	TREMOLO_URI,
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
