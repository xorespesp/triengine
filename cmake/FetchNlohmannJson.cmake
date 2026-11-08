if (NOT TARGET nlohmann_json::nlohmann_json)
    message(STATUS "Fetching nlohmann_json...")

    # The release archive holds only the headers and CMake files, far smaller than the repository.
    # See: https://json.nlohmann.me/integration/cmake/#fetchcontent
    FetchContent_Declare(nlohmann_json
        URL      https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(nlohmann_json)
endif()
