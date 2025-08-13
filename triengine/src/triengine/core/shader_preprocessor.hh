#pragma once
#include <filesystem>
#include <unordered_set>
#include <functional>
#include <string_view>
#include <optional>
#include <string>
#include <stack>

#include <triengine/utility/string_format.hh>

namespace triengine::core
{
    /**
     * A basic GLSL #include directive preprocessor.
     * Handles two types of #include directives:
     *   1. `#include "path/to/file.glsl"`
     *   2. `#include <path/to/file.glsl>`
     * 
     * <Role of shader preprocessor>
     *   The shader preprocessor is responsible for:
     *     - Parsing `#include` directives.
     *     - Passing include requests to a user-provided callback.
     *     - Inserting the returned source code into the current file.
     *     - Checking for and preventing duplicate includes.
     *
     *   The shader preprocessor does NOT:
     *     - Resolve file paths.
     *     - Read files from disk.
     *   (These tasks are handled by the "include resolver".)
     *
     * <Role of include resolver callback>
     *   Behavior:
     *     - Receives the path (or virtual path) of the file currently being parsed (e.g., "shaders/lighting.glsl")
     *       and the include path (or virtual path) requested in that file (e.g., "../utils.glsl").
     *     - Combines these paths to produce a normalized canonical path (e.g., "shaders/utils.glsl").
     *     - Loads the shader code for the canonical path into memory.
     *     - Returns both:
     *         1. The canonical path. (or virtual path)
     *         2. The loaded source code.
     *
     * <Note>
     *   - The preprocessor uses the canonical virtual path returned by the callback
     *     as a unique key to check if a file has already been included.
     *   - This prevents duplicate inclusion of the same file.
     */
    class basic_shader_preprocessor
    {
    public:
        enum class include_directive_type {
            quotes,        ///< The include uses quotes, e.g., #include "file.h"
            angle_brackets ///< The include uses angle brackets, e.g., #include <file.h>
        };

        /**
         * @struct resolved_include_info_t
         * @brief  A structure returned by the include resolver, containing the file's content
         * and its unique, canonical path.
         */
        struct resolved_include_info_t {
            std::string canonical_path; // A unique, normalized file path. (or virtual path)
            std::string content;        // The content of the file.
        };

        /**
         * @using include_resolver_t
         * @brief A function type for a callback that resolves an #include directive.
         * @param current_file_canonical_path The current canonical file path(or virtual path) of the file containing the #include directive.
         * @param parsed_include_type The type of the #include directive (quotes or angle_brackets).
         * @param parsed_include_path The path(or virtual path) string from the #include directive (e.g., "../common.glsl").
         * @return A resolved_include_t struct if successful, otherwise std::nullopt.
         */
        using include_resolver_t = std::function<
            std::optional<resolved_include_info_t>(
                std::string_view current_file_canonical_path, // #include가 위치한 파일의 경로 (혹은 가상 경로)
                include_directive_type parsed_include_type,   // #include "..." 인지 #include <...> 인지 구분
                std::string_view parsed_include_path          // #include "..." 에 있던 경로 (혹은 가상 경로)
            )
        >;

    private:

        // Configuration options
        struct process_options_t {
            bool generate_debug_comments{ true }; // Include debug comments in output
            bool allow_empty_includes{ false }; // Allow empty include files
            bool allow_multiple_inclusion{ false }; // Allow including the same file multiple times
            uint32_t max_include_depth{ 32 }; // Maximum include depth to prevent stack overflow

            // https://www.khronos.org/opengl/wiki/Core_Language_(GLSL)#Version
            //uint16_t glsl_version{ 430 };
            //std::string glsl_version_profile_name{ "core" }; // "core" / "compatibility"
        };

        // Struct to track include context for better error reporting
        struct include_context_t {
            std::string file_path;
            size_t line_number;

            std::string to_string() const {
                return utility::string::c_format("\"%s\" line %zu", file_path.c_str(), line_number);
            }
        };

    public:
        /**
         * @brief Constructs the preprocessor with a specific include resolver.
         * @param resolver The callback function to use for resolving includes.
         */
        explicit basic_shader_preprocessor(include_resolver_t include_resolver);

        // --- Configuration Getters ---
        bool is_debug_comments_enabled() const noexcept { return _opts.generate_debug_comments; }
        uint32_t get_max_include_depth() const noexcept { return _opts.max_include_depth; }
        bool is_allow_empty_includes() const noexcept { return _opts.allow_empty_includes; }
        bool is_allow_multiple_inclusion() const noexcept { return _opts.allow_multiple_inclusion; }

        // --- Configuration Setters ---
        void set_debug_comments(bool enable);
        void set_max_include_depth(uint32_t max_depth);
        void set_allow_empty_includes(bool allow);
        void set_allow_multiple_inclusion(bool allow);

        /**
         * Preprocesses shader code directly from memory and handles #include preprocessor directives.
         *
         * @param root_shader_source The root shader source code in memory
         * @param root_shader_canonical_path A root shader's canonical file path (or virtual path) to identify this shader (for include tracking)
         *                                   Note: The path parameter MUST be the canonical (fully resolved) form to avoid include cycles.
         * @param throw_on_error Whether to throw exceptions on error (default: false)
         * @return A preprocessed shader content, or std::nullopt if an error occurred
         */
        std::optional<std::string> process_from_memory(
            std::string_view root_shader_source,
            std::string_view root_shader_canonical_path,
            bool throw_on_error = false
        ) const;

    private:

        /**
         * Core include processing logic (handles includes for both file and memory versions)
         *
         * @param curr_shader_file_content The current shader source code to preprocess
         * @param curr_shader_canonical_path The canonical file path (or virtual path) of the current shader (for include cycle detection)
         * @param curr_included_canonical_paths Set of already included file's canonical path to avoid cycles
         * @param include_stack Stack of include contexts for error reporting
         * @param curr_depth Current include depth to prevent stack overflow
         * @return Processed shader content with all includes resolved
         * @throws std::runtime_error there is a processing error
         */
        std::string _preprocess_include_directives(
            std::string_view curr_shader_file_content,
            const std::string& curr_shader_canonical_path,
            std::unordered_set<std::string>& curr_included_canonical_paths/* in-out */,
            std::stack<include_context_t>& curr_include_stack/* in-out */,
            uint32_t curr_include_depth = 0
        ) const;

        /**
         * Checks if the given line is a #include directive and extracts the included file path.
         *
         * @param line The string line to check.
         * @return Returns the extracted file path from the #include directive (including quotes or angle brackets).
         */
        auto _try_parse_include_directive(
            std::string_view line
        ) const -> std::optional<std::pair<include_directive_type, std::string>>;

        /**
         * Helper function for debugging purposes
         */
        std::string _build_include_stack_trace(
            const std::stack<include_context_t>& include_stack
        ) const;

    private:
        process_options_t _opts;
        include_resolver_t _include_resolver;

    }; // class

    /**
     * @brief A physical filesystem based GLSL `#include` preprocessor.
     *
     * @note Include resolution behavior:
     *   - `#include "..."`
     *     Resolves paths on the physical filesystem. 
     *     Paths are treated as relative to the file containing the directive.
     *     This is ideal for project-specific shaders.
     *
     *   - `#include <...>`
     *     Not supported in this implementation.
     */
    class shader_preprocessor_fs
        : public basic_shader_preprocessor
    {
    public:
        shader_preprocessor_fs();

        /**
         * Preprocesses shader code from disk and handles #include preprocessor directives.
         *
         * @param root_shader_file_path The path to the shader file to be loaded.
         * @param throw_on_error Whether to throw exceptions on error (default: false)
         * @return A preprocessed shader content, or std::nullopt if an error occurred
         * @throws std::runtime_error if throw_on_error is true and the file cannot be opened
         */
        std::optional<std::string> process(
            const std::filesystem::path& shader_file_path,
            bool throw_on_error = false
        ) const;

    private:
        /**
         * Include resolver callback, called by the basic_shader_preprocessor to resolve includes.
         */
        std::optional<resolved_include_info_t> _include_resolver(
            std::string_view current_file_path,
            include_directive_type parsed_include_type,
            std::string_view parsed_include_path
        ) const;

        /**
         * Helper function to read a file into a string
         *
         * @param file_path Path to the file to read
         * @param file_content Reference to a string where the file content will be stored
         * @return true if the file was read successfully, false otherwise
         */
        bool _read_shader_file_content(
            const std::filesystem::path& file_path,
            std::string& file_content/* out */
        ) const;
    };

} // namespace