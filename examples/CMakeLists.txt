# Fetch xutl (header-only utility library) used exclusively by the examples.
# Including it here (under the TRIENGINE_BUILD_EXAMPLES guard) ensures the xutl
# target is never referenced when examples are disabled.
include(FetchXutl)

add_subdirectory(ex01-basic)
add_subdirectory(ex02-screen-quad)
add_subdirectory(ex03-offscreen)
if (WIN32)
	add_subdirectory(ex04-interprocess)
endif()
add_subdirectory(ex05-bvh-inspector)
