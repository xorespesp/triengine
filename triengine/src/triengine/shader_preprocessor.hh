#pragma once
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <string>
#include <stack>

#include <triengine/misc/string_utils.hh>

namespace triengine
{
    /**
     * A simple GLSL #include directive preprocessor.
     * Handles two types of #include directives:
     *   1. local file include: `#include "/path/to/file.glsl"`
     *   2. pre-registered system include: `#include <preregistered.glsl>`
     */
    class shader_preprocessor
    {
    public:
        // Hash function for std::filesystem::path
        struct path_hasher_t {
            std::size_t operator()(const std::filesystem::path& path) const {
                return std::hash<std::string>{}(path.generic_string());
            }
        };

        // Struct to track include context for better error reporting
        struct include_context_t {
            std::filesystem::path file_path;
            uint32_t line_number;

            std::string to_string() const {
                return misc::string::c_format("\"%s\" line %lu", file_path.generic_string().c_str(), line_number);
            }
        };

        // Configuration options
        struct process_options_t {
            bool generate_debug_comments{ true }; // Include debug comments in output
            bool allow_empty_includes{ false }; // Allow empty include files
            bool allow_multiple_inclusion{ false }; // Allow including the same file multiple times
            uint32_t max_include_depth{ 32 }; // Maximum include depth to prevent stack overflow

            // https://www.khronos.org/opengl/wiki/Core_Language_(GLSL)#Version
            //uint16_t glsl_version{ 430 };
            //std::string glsl_version_profile_name{ "core" }; // "core" / "compatibility"

            std::filesystem::path default_search_dir{ std::filesystem::current_path() }; // Default search directory for includes
        };

    public:
        shader_preprocessor() = default;

        /**
         * Enable or disable debug comments in the processed output
         *
         * @param enable Whether to enable debug comments
         */
        void set_debug_comments(bool enable);

        /**
         * Set the maximum include depth to prevent stack overflow
         *
         * @param max_depth Maximum include depth allowed
         */
        void set_max_include_depth(uint32_t max_depth);

        /**
         * Set whether to allow empty include files
         *
         * @param allow Whether to allow empty include files
         */
        void set_allow_empty_includes(bool allow);

        /**
         * Set whether to allow including the same file multiple times
         *
         * @param allow Whether to allow multiple inclusion of the same file
         */
        void set_allow_multiple_inclusion(bool allow);

        /**
         * Sets the default search directory for includes
         *
         * @param dir_path The directory path to use for resolving includes from memory shaders
         * @throws std::runtime_error if the path is ill-formed
         */
        void set_default_search_directory(const std::filesystem::path& dir_path);

        /**
         * Registers a system include from disk.
         *
         * @param include_name The name of the pre-registered system include (e.g., preregistered.glsl).
         * @param file_path The path of the pre-registered system include file.
         * @throws std::runtime_error if the file cannot be opened
         */
        void register_system_include(
            std::string_view include_name,
            const std::filesystem::path& file_path
        );

        /**
         * Registers a system include from memory.
         *
         * @param include_name The name of the pre-registered system include (e.g., preregistered.glsl).
         * @param file_content The content of the pre-registered system include file.
         * @throws std::runtime_error if include_name is empty
         */
        void register_system_include_from_memory(
            std::string_view include_name,
            std::string file_content
        );

        /**
         * Preprocesses shader code from disk and handles #include preprocessor directives.
         *
         * @param shader_file_path The path to the shader file to be loaded.
         * @param throw_on_error Whether to throw exceptions on error (default: true)
         * @return A preprocessed shader file content.
         * @throws std::runtime_error if throw_on_error is true and the file cannot be opened
         */
        std::string process(
            const std::filesystem::path& shader_file_path,
            bool throw_on_error = true
        ) const;

        /**
         * Preprocesses shader code directly from memory and handles #include preprocessor directives.
         *
         * @param shader_source The shader source code in memory
         * @param shader_name A virtual shader name to identify this shader (for include tracking)
         * @param throw_on_error Whether to throw exceptions on error (default: false)
         * @return A preprocessed shader content
         */
        std::string process_from_memory(
            std::string_view shader_source,
            std::string_view shader_name = "memory_shader",
            bool throw_on_error = true
        ) const;

    private:

        /**
         * Core include processing logic (handles includes for both file and memory versions)
         *
         * @param curr_shader_file_content The current shader source code to preprocess
         * @param curr_shader_file_path The (real or virtual) file path of the current shader (for include cycle detection)
         * @param curr_included_files Set of already included files to avoid cycles
         * @param include_stack Stack of include contexts for error reporting
         * @param curr_depth Current include depth to prevent stack overflow
         * @return Processed shader content with all includes resolved
         * @throws std::runtime_error there is a processing error
         */
        std::string _preprocess_include_directives(
            const std::string& curr_shader_file_content,
            const std::filesystem::path& curr_shader_file_path,
            std::unordered_set<std::filesystem::path, path_hasher_t>& curr_included_files/* in-out */,
            std::stack<include_context_t>& include_stack/* in-out */,
            uint32_t curr_depth = 0
        ) const;

    private:
        process_options_t _opts;
        std::unordered_map<
            std::filesystem::path/* virtual include path */,
            std::string/* include file content */,
            path_hasher_t
        > _registered_system_includes; // Pre-registered system include files

    }; // class

} // namespace