#pragma once

namespace triengine
{
	struct text_render_options
	{
		enum class dist_scale_mode_type {
			fade_out,   // Distance-based fade: full scale within ref_distance, gradual fade beyond
			perspective // Perspective scaling: scale inversely proportional to distance (natural 3D effect)
		};

		struct dist_scale_options {
			bool enabled{ true };
			dist_scale_mode_type scale_mode{ dist_scale_mode_type::perspective };
			float ref_distance{ 4.0f };  // Reference distance (scale = 1.0 at this distance)
			float max_distance{ 50.0f }; // Completely invisible at this distance (scale = 0.0)
		};

		struct depth_test_options {
			bool enabled{ true };
			float depth_bias{ 0.005f };    // Reduced base bias (adaptive bias will handle distance scaling)
			float occlusion_alpha{ 0.0f }; // Alpha multiplier when text is occluded
		};

		bool enabled{ true };
		dist_scale_options dist_scale_opts;
		depth_test_options depth_test_opts;
	};

} // namespace