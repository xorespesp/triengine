#pragma once
#include <triengine/common.h>

#include <string>
#include <string_view>

namespace triengine
{
	enum class text_alignment_type {
		top_left,
		center,
	};

	enum class text_object_type {
		label_2d,
		label_3d,
	};

	using text_object_id_type = size_t;

	class text_object {
	protected:
		text_object(text_object_type type)
			: _id{ _generate_unique_id() }
			, _type{ type }
		{}

	public:
		text_object(const text_object&) = delete;
		text_object& operator=(const text_object&) = delete;
		text_object(text_object&&) = delete;
		text_object& operator=(text_object&&) = delete;

		text_object_id_type get_id() const noexcept { return _id; }
		text_object_type get_type() const noexcept { return _type; }

		bool is_visible() const noexcept { return _visible; }
		void set_visible(bool visibility) {
			_visible = visibility;
		}

		bool is_empty() const noexcept {
			return _text.empty();
		}

		const std::string& get_text() const noexcept { return _text; }
		void set_text(std::string_view text) {
			_text = std::string(text);
		}

		text_alignment_type get_text_alignment() const noexcept { return _text_align; }
		void set_text_alignment(text_alignment_type text_align) {
			_text_align = text_align;
		}

		float get_scale() const noexcept { return _scale; }
		void set_scale(float scale) {
			_scale = scale;
		}

		const color3_f32& get_color() const noexcept { return _color; }
		void set_color(const color3_f32& color) {
			_color = color;
		}

	private:
		static text_object_id_type _generate_unique_id();

	private:
		text_object_id_type _id;
		text_object_type _type;
		bool _visible{ true };
		std::string _text;
		text_alignment_type _text_align{};
		float _scale{ 1.0f };
		color3_f32 _color{};
	};

	class text_2d_object : public text_object {
	public:
		text_2d_object() : text_object(text_object_type::label_2d) {}

		vec2_f32 get_position() const noexcept { return _position; }
		void set_position(const vec2_f32 position) {
			_position = position;
		}

	private:
		vec2_f32 _position{}; // label position in screen space (viewport coordinates)
	};

	class text_3d_object : public text_object {
	public:
		text_3d_object() : text_object(text_object_type::label_3d) {}

		const vec3_f32& get_position() const noexcept { return _position; }
		void set_position(const vec3_f32& position) {
			_position = position;
		}

	private:
		vec3_f32 _position{}; // label position in world space
	};

} // namespace