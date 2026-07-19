#include "text_object.hh"
#include <atomic>

namespace triengine
{
	text_object_id_type text_object::_generate_unique_id()
	{
		static std::atomic<text_object_id_type> _counter{ 0 };
		return ++_counter;
	}

} // namespace