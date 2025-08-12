#include "shader_preprocessor.hh"
#include <triengine/utility/debug_utils.hh>

#include <optional>
#include <fstream>

namespace triengine::core
{
    ////////////////////////////////////////////////////////////////////////////////
    // basic_shader_preprocessor implementation
    ////////////////////////////////////////////////////////////////////////////////

    basic_shader_preprocessor::basic_shader_preprocessor(
        include_resolver_t include_resolver)
        : _include_resolver(std::move(include_resolver))
    {
        if (!_include_resolver) {
            TRIENGINE_PANIC("Resolver cannot be null.");
        }
    }

    void basic_shader_preprocessor::set_debug_comments(bool enable) {
        _opts.generate_debug_comments = enable;
    }

    void basic_shader_preprocessor::set_max_include_depth(uint32_t max_depth) {
        _opts.max_include_depth = max_depth;
    }

    void basic_shader_preprocessor::set_allow_empty_includes(bool allow) {
        _opts.allow_empty_includes = allow;
    }

    void basic_shader_preprocessor::set_allow_multiple_inclusion(bool allow) {
        _opts.allow_multiple_inclusion = allow;
    }

    std::string basic_shader_preprocessor::process(
        const std::string_view shader_file_path,
        const bool throw_on_error) const
    {
        try
        {
            // Use the include resolver to get the root shader file's content and canonical path.
            // The current_file_path is the same as the requested path for the root file.
            const auto root_file = _include_resolver(shader_file_path, include_directive_type::quotes, shader_file_path); // TODO: improve this
            if (!root_file.has_value()) {
                TRIENGINE_PANIC("Failed to resolve root shader file: %.*s"
                    , static_cast<int>(shader_file_path.size())
                    , shader_file_path.data()
                );
            }

            std::unordered_set<std::string> included_files;
            std::stack<include_context_t> include_stack;
            return this->_preprocess_include_directives(
                root_file->content,
                root_file->canonical_path,
                included_files,
                include_stack,
                0
            );
        }
        catch (const std::exception& e)
        {
            if (throw_on_error) {
                throw;
            }

            TRIENGINE_ERROR("%s(): Error processing shader file \"%.*s\": %s"
                , __func__
                , static_cast<int>(shader_file_path.size())
                , shader_file_path.data()
                , e.what()
            );
        }

        return "";
    }

    std::string basic_shader_preprocessor::process_from_memory(
        std::string_view shader_source,
        std::string_view shader_name,
        bool throw_on_error) const
    {
        try
        {
            std::unordered_set<std::string> included_files;
            std::stack<include_context_t> include_stack;
            return this->_preprocess_include_directives(
                std::string{ shader_source },
                std::string{ shader_name },
                included_files,
                include_stack,
                0
            );
        }
        catch (const std::exception& e)
        {
            if (throw_on_error) {
                throw;
            }

            TRIENGINE_ERROR("%s(): Error processing memory shader \"%.*s\": %s"
                , __func__
                , static_cast<int>(shader_name.size())
                , shader_name.data()
                , e.what()
            );
        }

        return "";
    }

    std::string basic_shader_preprocessor::_preprocess_include_directives(
        const std::string& curr_shader_file_content,
        const std::string& curr_shader_file_path,
        std::unordered_set<std::string>& curr_included_files/* in-out */,
        std::stack<include_context_t>& curr_include_stack/* in-out */,
        const uint32_t curr_include_depth) const
    {
        // Check for include depth overflow
        if (curr_include_depth > _opts.max_include_depth)
        {
            TRIENGINE_PANIC("Maximum include depth exceeded.\n%s"
                , this->_build_include_stack_trace(curr_include_stack).c_str()
            );
        }

        // If the current shader is already included, handle based on configuration
        if (!_opts.allow_multiple_inclusion &&
            curr_included_files.find(curr_shader_file_path) != curr_included_files.end())
        {
            TRIENGINE_WARN("%s(): Include cycle detected. skipping already included file: \"%s\"\n%s"
                , __func__
                , curr_shader_file_path.c_str()
                , this->_build_include_stack_trace(curr_include_stack).c_str()
            );

            return "";
        }

        // Mark this shader as included
        curr_included_files.insert(curr_shader_file_path);

        std::string prep_result;
        prep_result.reserve(curr_shader_file_content.size() * 2); // To avoid reallocation as much as possible

        if (_opts.generate_debug_comments) {
            prep_result += utility::string::c_format("/***** [%lu] BEGIN FILE: \"%s\" *****/\n"
                , curr_include_depth
                , curr_shader_file_path.c_str()
            );
        }

        std::istringstream stream{ curr_shader_file_content };
        std::string curr_line;
        uint32_t curr_line_number{ 0 };

        while (std::getline(stream, curr_line))
        {
            ++curr_line_number;

            const auto parse_result = this->_try_parse_include_directive(curr_line);
            if (parse_result.has_value())
            {
                const auto& [parsed_include_type, parsed_include_path] = *parse_result;

                include_context_t curr_incl_ctx{ curr_shader_file_path, curr_line_number };
                curr_include_stack.push(curr_incl_ctx);

                const auto resolved_include = _include_resolver(
                    curr_shader_file_path,
                    parsed_include_type,
                    parsed_include_path
                );

                if (!resolved_include.has_value()) {
                    // Error handling
                    TRIENGINE_WARN("%s -> In \"%.*s\": Failed to resolve include: \"%s\""
                        , curr_incl_ctx.to_string().c_str()
                        , static_cast<int>(curr_shader_file_path.size()), curr_shader_file_path.data()
                        , parsed_include_path.c_str()
                    );
                    //include_stack.pop();
                }

                // Recursively process with the resolved include result(content and canonical path), provided by the resolver
                std::string included_content = _preprocess_include_directives(
                    resolved_include->content,
                    resolved_include->canonical_path,
                    curr_included_files,
                    curr_include_stack,
                    curr_include_depth + 1
                );

                prep_result += included_content;
                if (!included_content.empty() && included_content.back() != '\n') {
                    prep_result += '\n';
                }

                // Pop context from stack
                curr_include_stack.pop();
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
            prep_result += utility::string::c_format("/***** [%lu] END FILE: \"%s\" *****/\n"
                , curr_include_depth
                , curr_shader_file_path.c_str()
            );
        }

        //if (curr_include_depth == 0) {
        //    TRIENGINE_TRACE("prep_result:\n%s", prep_result.c_str());
        //}

        return prep_result;
    }

    auto basic_shader_preprocessor::_try_parse_include_directive(
        std::string_view line
    ) const -> std::optional<std::pair<include_directive_type, std::string>>
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
            (first_char == '"') ? basic_shader_preprocessor::include_directive_type::quotes : basic_shader_preprocessor::include_directive_type::angle_brackets,
            std::string{ line.substr(start_pos, search_pos - start_pos) }
        );
    }

    std::string basic_shader_preprocessor::_build_include_stack_trace(
        const std::stack<include_context_t>& include_stack) const
    {
        std::string error_msg = "Include stack trace:\n";
        auto temp_stack = include_stack;
        while (!temp_stack.empty()) {
            error_msg += "  - " + temp_stack.top().to_string() + "\n";
            temp_stack.pop();
        }
        return error_msg;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // shader_preprocessor implementation
    ////////////////////////////////////////////////////////////////////////////////

    shader_preprocessor::shader_preprocessor()
        : basic_shader_preprocessor(std::bind(
            &shader_preprocessor::_include_resolver, this, 
            std::placeholders::_1, 
            std::placeholders::_2, 
            std::placeholders::_3
        ))
    { }

    void shader_preprocessor::set_default_search_directory(const std::filesystem::path& dir_path) {
        _default_search_dir = this->_resolve_path(dir_path);
        TRIENGINE_DEBUG("Default search directory set to: \"%s\""
            , _default_search_dir.generic_string().c_str()
        );
    }

    void shader_preprocessor::register_system_include(
        std::string_view include_name,
        const std::filesystem::path& file_path)
    {
        if (include_name.empty()) {
            throw std::runtime_error{ "invalid include_name argument" };
        }

        const std::filesystem::path resolved_file_path = this->_resolve_path(file_path);

        std::string file_content = this->_read_file(resolved_file_path);
        if (file_content.empty()) {
            if (!this->_options().allow_empty_includes) {
                TRIENGINE_PANIC("Empty system include file: \"%s\"", resolved_file_path.generic_string().c_str());
            }
            TRIENGINE_WARN("Empty system include file: \"%s\"", resolved_file_path.generic_string().c_str());
        }

        _registered_system_includes[this->_make_virtual_path(std::string{ include_name })] = std::move(file_content);

        TRIENGINE_DEBUG("Registered system include file: \"%s\" -> \"%.*s\""
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
            if (!this->_options().allow_empty_includes) {
                TRIENGINE_PANIC("Empty system include content for: \"%.*s\""
                    , static_cast<int>(include_name.size())
                    , include_name.data()
                );
            }

            TRIENGINE_WARN("Empty system include content for: \"%.*s\""
                , static_cast<int>(include_name.size())
                , include_name.data()
            );
        }

        _registered_system_includes[this->_make_virtual_path(std::string{ include_name })] = std::move(file_content);
        TRIENGINE_DEBUG("Registered system include \"%.*s\" from memory"
            , static_cast<int>(include_name.size())
            , include_name.data()
        );
    }

    std::optional<shader_preprocessor::resolved_include_info_t> shader_preprocessor::_include_resolver(
        const std::string_view current_file_path,
        const include_directive_type parsed_include_type,
        const std::string_view parsed_include_path) const
    {
        if (parsed_include_type == include_directive_type::quotes) // Handle: `#include "path/to/file.glsl"`
        {
            std::filesystem::path resolved_include_path;
            try {
                resolved_include_path = this->_resolve_path(this->_is_virtual_path(current_file_path)
                    ? _default_search_dir / parsed_include_path
                    : std::filesystem::path{ current_file_path }.parent_path() / parsed_include_path
                );
            } catch (...) {
                TRIENGINE_WARN("Faild to resolve include file path");
                return std::nullopt;
            }

            // Avoid including self
            if (current_file_path == resolved_include_path) {
                TRIENGINE_WARN("Tried to include itself");
                return std::nullopt;
            }

            std::string file_content = this->_read_file(resolved_include_path);

            return resolved_include_info_t{
                resolved_include_path.generic_string(),
                std::move(file_content)
            };
        }
        else if (parsed_include_type == include_directive_type::angle_brackets) // Handle: `#include <path/to/file.glsl>`
        {
            const std::filesystem::path virtual_include_path = this->_make_virtual_path(parsed_include_path);

            // Avoid including self
            if (current_file_path == virtual_include_path) {
                TRIENGINE_WARN("Tried to include itself");
                return std::nullopt;
            }

            if (const auto it = _registered_system_includes.find(virtual_include_path);
                it != _registered_system_includes.end())
            {
                const std::string& file_content = it->second;

                return resolved_include_info_t{
                    virtual_include_path.generic_string(),
                    file_content
                };
            }
            else
            {
                TRIENGINE_WARN("Invalid system include file");
            }
        }
        else
        {
            TRIENGINE_WARN("Unknown include directive type: %d"
                , static_cast<int>(parsed_include_type)
            );
        }

        return std::nullopt;
    }

    // --- Helper methods ---

    std::filesystem::path shader_preprocessor::_make_virtual_path(
        const std::string_view p) const
    {
        if (p.find("virtual:") == 0) {
            return std::filesystem::path{ p };
        }
        else {
            return std::filesystem::path{ "virtual:" } / p;
        }
    }

    bool shader_preprocessor::_is_virtual_path(
        const std::filesystem::path& p) const
    {
        return p.string().find("virtual:") == 0;
    }

    std::filesystem::path shader_preprocessor::_resolve_path(
        const std::filesystem::path& p) const
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

    std::string shader_preprocessor::_read_file(
        const std::filesystem::path& file_path) const
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

} // namespace