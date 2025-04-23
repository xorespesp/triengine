#include "shader_preprocessor.hh"
#include <triengine/misc/debug_utils.hh>

#include <optional>
#include <fstream>

namespace triengine
{
    namespace {
        namespace detail
        {
            enum class include_directive_type {
                quotes,        ///< The include uses quotes, e.g., #include "file.h"
                angle_brackets ///< The include uses angle brackets, e.g., #include <file.h>
            };

            /**
             * Checks if the given line is a #include directive and extracts the included file path.
             *
             * @param line The string line to check.
             * @return Returns the extracted file path from the #include directive (including quotes or angle brackets).
             */
            auto try_parse_include_directive(
                std::string_view line
            ) -> std::optional<std::pair<include_directive_type, std::filesystem::path>>
            {
                // Trim leading and trailing spaces from the line.
                const auto trim_left = std::find_if_not(line.begin(), line.end(), 
                    [](char c) { return std::isspace(static_cast<uint8_t>(c)); });

                if (trim_left == line.end()) { return std::nullopt; }

                const auto trim_right = std::find_if_not(line.rbegin(), line.rend(), 
                    [](char c) { return std::isspace(static_cast<uint8_t>(c)); }).base();

                line = line.substr(size_t(trim_left - line.begin()), size_t(trim_right - trim_left));

                // Check if the line contains the "#include" token.
                constexpr std::string_view kIncludeToken = "#include";
                const size_t token_pos = line.find(kIncludeToken);
                if (token_pos == std::string_view::npos) {
                    return std::nullopt;
                }

                // Check if there is nothing but spaces before the "#include" token.
                for (size_t i = 0; i < token_pos; ++i) {
                    if (!std::isspace(static_cast<uint8_t>(line[i]))) {
                        return std::nullopt;
                    }
                }

                // Check if there is a file path after the "#include" directive.
                size_t search_pos = token_pos + kIncludeToken.size();
                if (search_pos >= line.size()) {
                    return std::nullopt;
                }

                // Skip any trailing spaces after "#include".
                while (search_pos < line.size() && std::isspace(static_cast<uint8_t>(line[search_pos]))) ++search_pos;

                // Now, check if a quote ('"') or angle bracket ('<') follows, indicating a valid file path.
                if (search_pos >= line.size()) {
                    return std::nullopt;
                }

                const char first_char = line[search_pos];

                // If the character after "#include" is not a quote or angle bracket, it's invalid.
                if (first_char != '"' && first_char != '<') {
                    return std::nullopt;
                }

                // Extract the file path enclosed by quotes or angle brackets.
                ++search_pos; // Move past the starting quote or angle bracket.
                const size_t start_pos = search_pos;

                const char closing_char = (first_char == '"') ? '"' : '>';
                while (search_pos < line.size() && line[search_pos] != closing_char) ++search_pos;

                // If no closing quote or angle bracket is found, the line is invalid.
                if (search_pos >= line.size()) {
                    return std::nullopt;
                }

                // Check if there's anything but comments after the closing quote/bracket
                size_t comment_check_pos = search_pos + 1;
                bool found_non_whitespace = false;
                bool found_comment = false;

                while (comment_check_pos < line.size())
                {
                    if (std::isspace(static_cast<uint8_t>(line[comment_check_pos]))) {
                        // Skip whitespace
                        ++comment_check_pos;
                        continue;
                    }

                    if (line.substr(comment_check_pos, 2) == "//" ||
                        line.substr(comment_check_pos, 2) == "/*") {
                        // Found a comment, everything is fine
                        found_comment = true;
                        break;
                    }

                    // Found non-whitespace, non-comment content
                    found_non_whitespace = true;
                    break;
                }

                // If there's non-whitespace content after the include that isn't a comment, the line is invalid
                if (found_non_whitespace && !found_comment) {
                    return std::nullopt;
                }

                return std::make_pair(
                    (first_char == '"') ? include_directive_type::quotes : include_directive_type::angle_brackets,
                    std::filesystem::path{ line.substr(start_pos, search_pos - start_pos) }
                );
            }

            /**
             * Creates a virtual path for system includes
             *
             * @param p The path to convert to a virtual path
             * @return A virtual path with "virtual:" prefix
             */
            std::filesystem::path make_virtual_path(
                const std::string& p)
            {
                if (p.find("virtual:") == 0) {
                    return std::filesystem::path{ p };
                } else {
                    return std::filesystem::path{ "virtual:" } / p;
                }
            }

            /**
             * Checks if a path is a virtual path
             *
             * @param p The path to check
             * @return True if the path is a virtual path
             */
            bool is_virtual_path(
                const std::filesystem::path& p)
            {
                return p.string().find("virtual:") == 0;
            }

            /**
             * Converts a path to a canonical absolute path and checks if it exists.
             *
             * @param p The path to check
             * @return canonical absolute path
             * @throws std::runtime_error there is a resolve error
             */
            std::filesystem::path resolve_path(
                const std::filesystem::path& p)
            {
                try
                {
                    if (!std::filesystem::exists(p)) {
                        throw std::runtime_error{ "not found" };
                    }

                    // Make sure the path is properly normalized
                    return std::filesystem::canonical(p);
                }
                catch (const std::filesystem::filesystem_error& e)
                {
                    TRIENGINE_PANIC("Failed to resolve path \"%s\" (%s)"
                        , p.generic_string().c_str(), e.what()
                    );
                }
            }

            /**
             * Helper function to read a file into a string
             *
             * @param file_path Path to the file to read
             * @return Content of the file as a string
             * @throws std::runtime_error if the file cannot be opened or read
             */
            std::string read_file(
                const std::filesystem::path& file_path)
            {
                try
                {
                    std::ifstream f{ file_path, std::ios::binary };
                    if (!f.is_open()) {
                        throw std::runtime_error{ "unable to open file" };
                    }

                    // Get file size for better memory allocation
                    f.seekg(0, std::ios::end);
                    const auto size = static_cast<size_t>(f.tellg());
                    f.seekg(0, std::ios::beg);

                    std::string content;
                    content.reserve(size);
                    content.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

                    return content;
                }
                catch (const std::exception& e)
                {
                    TRIENGINE_PANIC("error reading file \"%s\": %s"
                        , file_path.generic_string().c_str(), e.what()
                    );
                }
            }

            // Helper function
            std::string build_include_stack_trace(
                const std::stack<shader_preprocessor::include_context_t>& include_stack)
            {
                std::string error_msg = "Include stack trace:\n";
                auto temp_stack = include_stack;
                while (!temp_stack.empty()) {
                    error_msg += "  - " + temp_stack.top().to_string() + "\n";
                    temp_stack.pop();
                }
                return error_msg;
            }

        } // namespace
    } // namespace

    void shader_preprocessor::set_debug_comments(bool enable) {
        _opts.generate_debug_comments = enable;
    }

    void shader_preprocessor::set_max_include_depth(uint32_t max_depth) {
        _opts.max_include_depth = max_depth;
    }

    void shader_preprocessor::set_allow_empty_includes(bool allow) {
        _opts.allow_empty_includes = allow;
    }

    void shader_preprocessor::set_allow_multiple_inclusion(bool allow) {
        _opts.allow_multiple_inclusion = allow;
    }

    void shader_preprocessor::set_default_search_directory(const std::filesystem::path& dir_path) {
        _opts.default_search_dir = detail::resolve_path(dir_path);
        TRIENGINE_TRACE("Default search directory set to: \"%s\""
            , _opts.default_search_dir.generic_string().c_str()
        );
    }

    void shader_preprocessor::register_system_include(
        std::string_view include_name,
        const std::filesystem::path& file_path)
    {
        if (include_name.empty()) {
            throw std::runtime_error{ "invalid include_name argument" };
        }

        const std::filesystem::path resolved_file_path = detail::resolve_path(file_path);

        std::string file_content = detail::read_file(resolved_file_path);
        if (file_content.empty()) {
            if (!_opts.allow_empty_includes) {
                TRIENGINE_PANIC("Empty system include file: \"%s\"", resolved_file_path.generic_string().c_str());
            }
            TRIENGINE_TRACE("Empty system include file: \"%s\"", resolved_file_path.generic_string().c_str());
        }

        _registered_system_includes[detail::make_virtual_path(std::string{ include_name })] = std::move(file_content);

        TRIENGINE_TRACE("Registered system include file: \"%s\" -> \"%.*s\""
            , resolved_file_path.generic_string().c_str()
            , static_cast<int>(include_name.size())
            , include_name.data()
        );
    }

    void shader_preprocessor::register_system_include_from_memory(
        std::string_view include_name,
        std::string file_content)
    {
        if (include_name.empty()) {
            throw std::runtime_error{ "invalid include_name argument" };
        }

        if (file_content.empty())
        {
            if (!_opts.allow_empty_includes) {
                TRIENGINE_PANIC("Empty system include content for: \"%.*s\""
                    , static_cast<int>(include_name.size())
                    , include_name.data()
                );
            }

            TRIENGINE_TRACE("Empty system include content for: \"%.*s\""
                , static_cast<int>(include_name.size())
                , include_name.data()
            );
        }

        _registered_system_includes[detail::make_virtual_path(std::string{ include_name })] = std::move(file_content);
        TRIENGINE_TRACE("Registered system include \"%.*s\" from memory"
            , static_cast<int>(include_name.size())
            , include_name.data()
        );
    }

    std::string shader_preprocessor::process(
        const std::filesystem::path& shader_file_path,
        const bool throw_on_error) const
    {
        try
        {
            const std::filesystem::path resolved_shader_file_path = detail::resolve_path(shader_file_path);
            std::unordered_set<std::filesystem::path, path_hasher_t> included_files;
            std::stack<include_context_t> include_stack;
            return _preprocess_include_directives(
                detail::read_file(resolved_shader_file_path),
                resolved_shader_file_path,
                included_files,
                include_stack
            );
        }
        catch (const std::exception& e)
        {
            if (throw_on_error) {
                throw;
            }

            TRIENGINE_TRACE("%s(): Error processing shader file \"%s\": %s"
                , __func__
                , shader_file_path.generic_string().c_str()
                , e.what()
            );
        }

        return "";
    }

    std::string shader_preprocessor::process_from_memory(
        std::string_view shader_source,
        std::string_view shader_name,
        bool throw_on_error) const
    {
        try
        {
            std::unordered_set<std::filesystem::path, path_hasher_t> included_files;
            std::stack<include_context_t> include_stack;
            return _preprocess_include_directives(
                std::string(shader_source),
                detail::make_virtual_path(std::string(shader_name)),
                included_files,
                include_stack
            );
        }
        catch (const std::exception& e)
        {
            if (throw_on_error) {
                throw;
            }

            TRIENGINE_TRACE("%s(): Error processing memory shader \"%.*s\": %s"
                , __func__
                , static_cast<int>(shader_name.size())
                , shader_name.data()
                , e.what()
            );
        }

        return "";
    }

    std::string shader_preprocessor::_preprocess_include_directives(
        const std::string& curr_shader_file_content,
        const std::filesystem::path& curr_shader_file_path,
        std::unordered_set<std::filesystem::path, path_hasher_t>& curr_included_files/* in-out */,
        std::stack<include_context_t>& include_stack/* in-out */,
        const uint32_t curr_depth) const
    {
        // Check for include depth overflow
        if (curr_depth > _opts.max_include_depth)
        {
            TRIENGINE_PANIC("Maximum include depth exceeded.\n%s"
                , detail::build_include_stack_trace(include_stack).c_str()
            );
        }

        // If the current shader is already included, handle based on configuration
        if (!_opts.allow_multiple_inclusion &&
            curr_included_files.find(curr_shader_file_path) != curr_included_files.end())
        {
            TRIENGINE_TRACE("%s(): Include cycle detected. skipping already included file: \"%s\"\n%s"
                , __func__
                , curr_shader_file_path.generic_string().c_str()
                , detail::build_include_stack_trace(include_stack).c_str()
            );

            return "";
        }

        // Mark this shader as included
        curr_included_files.insert(curr_shader_file_path);

        std::string prep_result;
        prep_result.reserve(curr_shader_file_content.size() * 2); // To avoid reallocation as much as possible

        if (_opts.generate_debug_comments) {
            prep_result += misc::string::c_format("/***** [%lu] BEGIN FILE: \"%s\" *****/\n"
                , curr_depth
                , curr_shader_file_path.generic_string().c_str()
            );
        }

        std::istringstream stream{ curr_shader_file_content };
        std::string curr_line;
        uint32_t curr_line_number{ 0 };

        while (std::getline(stream, curr_line))
        {
            ++curr_line_number;

            const auto parse_result = detail::try_parse_include_directive(curr_line);
            if (parse_result.has_value())
            {
                const auto& [parsed_include_type, parsed_include_path] = *parse_result;

                include_context_t curr_ctx{ curr_shader_file_path, curr_line_number };
                include_stack.push(curr_ctx);

                std::string included_content;

                if (parsed_include_type == detail::include_directive_type::quotes) // Handle: #include "/path/to/file.glsl"
                {
                    std::filesystem::path resolved_include_path;
                    try {
                        resolved_include_path = detail::resolve_path(detail::is_virtual_path(curr_shader_file_path)
                            ? _opts.default_search_dir / parsed_include_path
                            : curr_shader_file_path.parent_path() / parsed_include_path
                        );
                    } catch (...) {
                        TRIENGINE_PANIC("%s -> invalid include file path \"%s\""
                            , curr_ctx.to_string().c_str()
                            , parsed_include_path.string().c_str()
                        );
                    }

                    // Avoid including self
                    if (curr_shader_file_path == resolved_include_path) {
                        TRIENGINE_TRACE("%s(): %s -> tried to include itself", __func__, curr_ctx.to_string().c_str());
                        include_stack.pop();
                        continue;
                    }

                    // Recursively process the included file
                    included_content = _preprocess_include_directives(
                        detail::read_file(resolved_include_path),
                        resolved_include_path,
                        curr_included_files,
                        include_stack,
                        curr_depth + 1
                    );
                }
                else if (parsed_include_type == detail::include_directive_type::angle_brackets) // Handle: #include <preregistered.glsl>
                {
                    const std::filesystem::path virtual_include_path = detail::make_virtual_path(parsed_include_path.string());

                    // Avoid including self
                    if (curr_shader_file_path == virtual_include_path) {
                        TRIENGINE_TRACE("%s(): %s -> tried to include itself", __func__, curr_ctx.to_string().c_str());
                        include_stack.pop();
                        continue;
                    }

                    if (const auto it = _registered_system_includes.find(virtual_include_path);
                        it != _registered_system_includes.end())
                    {
                        // Recursively process the included file
                        included_content = this->_preprocess_include_directives(
                            it->second,
                            virtual_include_path,
                            curr_included_files,
                            include_stack,
                            curr_depth + 1
                        );
                    }
                    else
                    {
                        include_stack.pop();
                        TRIENGINE_PANIC("%s -> system include \"%s\" not found"
                            , curr_ctx.to_string().c_str()
                            , parsed_include_path.string().c_str()
                        );
                    }
                }
                else
                {
                    include_stack.pop();
                    TRIENGINE_PANIC("%s -> Unknown include directive type: %d"
                        , curr_ctx.to_string().c_str()
                        , static_cast<int>(parsed_include_type)
                    );
                }

                prep_result += included_content;
                if (!included_content.empty() && included_content.back() != '\n') {
                    prep_result += '\n';
                }

                include_stack.pop();
            }
            else //if (!parse_result.has_value())
            {
                prep_result += curr_line + '\n';
            }
        } // while

        // Note: We don't remove curr_shader_file_path from curr_included_files here
        // This is to maintain proper include guard behavior and prevent multiple inclusion
        // of the same file in different branches unless specifically allowed

        if (_opts.generate_debug_comments) {
            prep_result += misc::string::c_format("/***** [%lu] END FILE: \"%s\" *****/\n"
                , curr_depth
                , curr_shader_file_path.generic_string().c_str()
            );
        }

        if (curr_depth == 0) {
            TRIENGINE_TRACE("prep_result:\n%s", prep_result.c_str());
        }

        return prep_result;
    }

} // namespace