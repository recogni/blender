/*
 * Copyright 2011-2018 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "device/device.h"
#include "device/device_denoising.h"

#include "render/buffers.h"

#include "util/util_string.h"
#include "util/util_vector.h"

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING

#ifndef __DENOISING_H__
#define __DENOISING_H__

CCL_NAMESPACE_BEGIN

class StandaloneDenoiser {
public:
	StandaloneDenoiser(Device *device)
	 : device(device)
	{
		views = false;
		samples = 0;
		tile_size = make_int2(64, 64);

		radius = 8;
		strength = 0.5f;
		feature_strength = 0.5f;
		relative_pca = false;

		error = "";

		center_frame = "";
		frame_radius = 2;

		ldr_out = false;

		clamp_input = true;

		DeviceRequestedFeatures req;
		device->load_kernels(req);
	}

	bool run_filter();

	void output_profiling();

	string in_path;
	string out_path;
	bool ldr_out;

	string error;

	/* Do the frames contain multiple views? */
	bool views;
	/* Sample number override, takes precedence over values from input frames. */
	int samples;
	int2 tile_size;

	/* Frame or OIIO frame range of frames that are filtered. */
	string center_frame;
	/* How many frames before and after the current center frame are included? */
	int frame_radius;

	/* Highest frame that will be processed (for progress display). */
	int max_frame;

	/* Equivalent to the settings in the regular denoiser. */
	int radius;
	float strength;
	float feature_strength;
	bool relative_pca;

	/* Controls whether render layers that miss all or some of the required channels are passed through. */
	bool passthrough_incomplete;
	/* Controls whether channels that could not be parsed are passed through. */
	bool passthrough_unknown;
	/* Controls whether channels of a render layer that aren't used for denoising are passed through. */
	bool passthrough_additional;

	/* Clamp the input to the range of +-1e8. Should be enough for any legitimate data. */
	bool clamp_input;

protected:
	friend class FilterTask;

	Device *device;
};


class FilterTask {
public:
	FilterTask(Device *device, StandaloneDenoiser *sd, string in_file, string out_file, int frame)
	 : sd(sd), in_file(in_file), out_file(out_file),
	   device(device), buffer(device, "filter buffer", MEM_READ_ONLY), center_frame(frame)
	{
		in = NULL;
		out = NULL;

		samples = sd->samples;
	}

	~FilterTask()
	{
		free();
	}

	bool run_filter(vector<int> frames);

	bool filter_load(vector<int> frames);
	void filter_exec();
	void filter_save();

	/* Equivalent to the options in the StandaloneDenoiser. */
	StandaloneDenoiser *sd;
	int samples;

	string error;

protected:
	string in_file;
	string out_file;

	struct RenderLayer {
		string name;
		/* All channels belonging to this RenderLayer. */
		vector<string> channels;
		/* Channel offset in the input file. */
		vector<int> file_offsets;
		/* Channel offset of the corresponding buffer channel if the channel is used for processing, -1 otherwise. */
		vector<int> buffer_offsets;

		/* Sample amount that was used for rendering this layer. */
		int samples;

		/* Buffer channel i will be set to file channel buffer_to_file_map[i].
		 * Generated from file_offsets and buffer_offsets in parse_channels(). */
		vector<int> buffer_to_file_map;

		/* buffer_to_file_map of the secondary frames, if any are used. */
		map<int, vector<int> > frame_buffer_to_file_map;

		/* Write i-th channel of the processing output to out_results[i]-th channel of the file. */
		vector<int> out_results;

		/* Detect whether this layer contains a full set of channels and set up the offsets accordingly. */
		bool detect_sets();
		/* Generate the buffer_to_file_map for image reading. */
		void generate_map(int buffer_nchannels);
		/* Map the channels of a secondary frame to the channels that are required for processing,
		 * fill frame_buffer_to_file_map if all are present or return false if a channel are missing. */
		bool match_channels(int frame,
		                    const vector<string> &main_channelnames,
		                    const std::vector<string> &frame_channelnames);
	};

	Device *device;

	ImageInput *in;
	ImageOutput *out;

	array<float> out_buffer;
	device_vector<float> buffer;
	int buffer_pass_stride;
	int target_pass_stride;
	int out_pass_stride;
	int num_frames;
	int buffer_frame_stride;

	vector<string> in_channels;
	/* Contains all passthrough channels followed by the generated channels. */
	vector<string> out_channels;
	/* Set i-th channel of the output to the out_passthrough[i]-th channel of the input. */
	vector<int> out_passthrough;

	bool only_rgb;

	/* Layers to be processed. */
	vector<RenderLayer> layers;
	int current_layer;

	int center_frame;
	vector<int> frames;
	vector<ImageInput*> in_frames;

	list<RenderTile> tiles;
	int num_tiles;
	map<int, device_vector<float>*> target_mems;
	thread_mutex tiles_mutex, targets_mutex;

	int width, height, nchannels;

	/* Parse input file channels, separate them into RenderLayers, detect RenderLayers with full channel sets,
	 * fill layers and set up the output channels and passthrough map. */
	void parse_channels(const ImageSpec &in_spec);

	/* Load a slice of the given image and fill the buffer pointed to by mem according to the given channel map.
	 * If write_out is true, also passthrough unused input channels directly to the output image. */
	bool load_file(ImageInput *in_image,
	               const vector<int> &buffer_to_file_map,
	               float *mem,
	               bool write_out);

	void write_output();

	/* Open the input image, parse its channels, open the output image and allocate the output buffer. */
	bool open_frames(string in_filename, string out_filename);

	void free();

	DeviceTask create_task();

	bool acquire_tile(Device *device, Device *tile_device, RenderTile &tile);
	void map_neighboring_tiles(RenderTile *tiles, Device *tile_device);
	void unmap_neighboring_tiles(RenderTile *tiles);
	void release_tile();
	bool get_cancel();
};

CCL_NAMESPACE_END

#endif /* __DENOISING_H__ */