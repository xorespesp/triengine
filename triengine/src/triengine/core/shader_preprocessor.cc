#include "shader_preprocessor.hh"
#include <triengine/utility/debug_utils.hh>

#include <optional>
#include <fstream>
#include <algorithm>

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

    std::optional<std::string> basic_shader_preprocessor::process_from_memory(
        const std::string_view root_shader_source,
        const std::string_view root_shader_canonical_path,
        const bool throw_on_error) const
    {
        try
        {
            std::unordered_set<std::string> included_canonical_paths;
            std::stack<include_context_t> include_stack;
            return this->_preprocess_include_directives(
                std::string{ root_shader_source },
                std::string{ root_shader_canonical_path },
                included_canonical_paths,
                include_stack,
                0
            );
        }
        catch (const std::exception& e)
        {
            if (throw_on_error) {
                throw;
            }

            TRIENGINE_ERROR("%s(): Error processing root shader \"%.*s\": %s"
                , __func__
                , static_cast<int>(root_shader_canonical_path.size())
                , root_shader_canonical_path.data()
                , e.what()
            );
        }

        return std::nullopt;
    }

    std::string basic_shader_preprocessor::_preprocess_include_directives(
        const std::string& curr_shader_file_content,
        const std::string& curr_shader_canonical_path,
        std::unordered_set<std::string>& curr_included_canonical_paths/* in-out */,
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
            0 != curr_included_canonical_paths.count(curr_shader_canonical_path))
        {
            TRIENGINE_WARN("%s(): Already included file, skipping: \"%s\"\n%s"
                , __func__
                , curr_shader_canonical_path.c_str()
                , this->_build_include_stack_trace(curr_include_stack).c_str()
            );

            return "";
        }

        // Mark this shader as included
        curr_included_canonical_paths.insert(curr_shader_canonical_path);

        std::string prep_result;
        prep_result.reserve(curr_shader_file_content.size() * 2); // To avoid reallocation as much as possible

        if (_opts.generate_debug_comments) {
            prep_result += utility::string::c_format("/***** [%u] BEGIN FILE: \"%s\" *****/\n"
                , curr_include_depth
                , curr_shader_canonical_path.c_str()
            );
        }

        for (size_t curr_line_begin_pos{ 0 }, curr_line_end_pos{ 0 }, curr_line_number{ 1 };
             curr_line_begin_pos < curr_shader_file_content.size();
             ++curr_line_number)
        {
            curr_line_end_pos = curr_shader_file_content.find('\n', curr_line_begin_pos);
            const std::string_view curr_shader_line = curr_shader_file_content.substr(
                curr_line_begin_pos, 
                (curr_line_end_pos != std::string_view::npos)
                ? curr_line_end_pos - curr_line_begin_pos
                : curr_line_end_pos
            );

            const auto parse_result = this->_try_parse_include_directive(curr_shader_line);
            if (parse_result.has_value())
            {
                const auto& [parsed_include_type, parsed_include_path] = *parse_result;

                include_context_t curr_incl_ctx{ curr_shader_canonical_path, curr_line_number };
                curr_include_stack.push(curr_incl_ctx);

                const auto resolved_include = _include_resolver(
                    curr_shader_canonical_path,
                    parsed_include_type,
                    parsed_include_path
                );

                if (!resolved_include.has_value()) {
                    // Error handling
                    TRIENGINE_PANIC("%s -> In \"%.*s\": Failed to resolve include: \"%s\""
                        , curr_incl_ctx.to_string().c_str()
                        , static_cast<int>(curr_shader_canonical_path.size()), curr_shader_canonical_path.data()
                        , parsed_include_path.c_str()
                    );
                }

                // Handle empty file includes
                if (!_opts.allow_empty_includes && resolved_include->content.empty()) {
                    TRIENGINE_WARN("%s -> In \"%.*s\": Included file is empty: \"%s\""
                        , curr_incl_ctx.to_string().c_str()
                        , static_cast<int>(curr_shader_canonical_path.size()), curr_shader_canonical_path.data()
                        , resolved_include->canonical_path.c_str()
                    );
                    curr_include_stack.pop();
                    continue;
                }

                // Recursively process with the resolved include result(content and canonical path), provided by the resolver
                std::string included_content = this->_preprocess_include_directives(
                    resolved_include->content,
                    resolved_include->canonical_path,
                    curr_included_canonical_paths,
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
                prep_result += curr_shader_line;
                prep_result += '\n';
            }

            // move up to next line
            curr_line_begin_pos = (curr_line_end_pos != std::string_view::npos)
                ? curr_line_end_pos + 1
                : curr_shader_file_content.size();
        } // for

        // Note: We don't remove curr_shader_canonical_path from curr_included_canonical_paths here
        // This is to maintain proper include guard behavior and prevent multiple inclusion
        // of the same file in different branches unless specifically allowed

        if (_opts.generate_debug_comments) {
            prep_result += utility::string::c_format("/***** [%u] END FILE: \"%s\" *****/\n"
                , curr_include_depth
                , curr_shader_canonical_path.c_str()
            );
        }

        //if (curr_include_depth == 0) {
        //    TRIENGINE_TRACE("prep_result:\n%s", prep_result.c_str());
        //}

        return prep_result;
    }

    // Test Cases:
    //   [#include "file.glsl"] => quotes , path='file.glsl'
    //   [#include <file.glsl>] => angle , path='file.glsl'
    //   [#include"file.glsl"] => quotes , path='file.glsl'
    //   [#include<file.glsl>] => angle , path='file.glsl'
    //   [#  include    "path/with/spaces.glsl"] => quotes , path='path/with/spaces.glsl'
    //   [#include "file.glsl"   // comment] => quotes , path='file.glsl'
    //   [#include "file.glsl"   /* block comment */] = > quotes, path = 'file.glsl'
    //   [#include "file.glsl"   /* block comment */] = > quotes, path = 'file.glsl'
    //   [#include "file.glsl"   /* unterminated comment] => NO MATCH
    //   [   #   include   <angle/path.glsl>   ] => angle , path='angle/path.glsl'
    //   [#include "file.glsl" garbage] => NO MATCH
    //   [not an include] => NO MATCH
    //   [#include file.glsl] => NO MATCH
    //   [#include "file with spaces.glsl"] => quotes , path='file with spaces.glsl'
    auto basic_shader_preprocessor::_try_parse_include_directive(
        std::string_view line
    ) const -> std::optional<std::pair<include_directive_type, std::string>>
    {
        static const auto is_space = [](char c) {
            // Cast to unsigned char to avoid UB
            return std::isspace(static_cast<unsigned char>(c));
        };

        // 1. Trim leading/trailing spaces
        auto ltrim_it = std::find_if_not(line.begin(), line.end(),
            [](char c) { return is_space(c); }
        );
        if (ltrim_it == line.end()) { return std::nullopt; }

        auto rtrim_it = std::find_if_not(line.rbegin(), line.rend(),
            [](char c) { return is_space(c); }
        ).base();
        line = line.substr(
            static_cast<size_t>(ltrim_it - line.begin()),
            static_cast<size_t>(rtrim_it - ltrim_it)
        );

        // 2. Must start with '#' followed by optional spaces and 'include'
        if (line.empty() || line.front() != '#') { return std::nullopt; }

        size_t curr_pos = 1; // skip '#'
        while (curr_pos < line.size() && is_space(line[curr_pos])) { ++curr_pos; }

        constexpr std::string_view kIncludeToken = "include";
        if (curr_pos + kIncludeToken.size() > line.size() ||
            line.compare(curr_pos, kIncludeToken.size(), kIncludeToken) != 0)
        {
            return std::nullopt;
        }
        curr_pos += kIncludeToken.size();

        // Skip spaces after 'include' token (if exists)
        while (curr_pos < line.size() && is_space(line[curr_pos])) { ++curr_pos; }
        if (curr_pos >= line.size()) {
            return std::nullopt;
        }

        // 3. Detect opening delimiter
        char first_char = line[curr_pos];
        if (first_char != '"' && first_char != '<') { return std::nullopt; }
        char closing_char = (first_char == '"') ? '"' : '>';

        ++curr_pos; // skip opening delimiter
        size_t start_path = curr_pos;

        // Find closing delimiter
        while (curr_pos < line.size() && line[curr_pos] != closing_char) { ++curr_pos; }
        if (curr_pos >= line.size()) { return std::nullopt; }

        std::string include_path{ line.substr(start_path, curr_pos - start_path) };

        ++curr_pos; // move past closing delimiter

        // 4. Skip spaces
        while (curr_pos < line.size() && is_space(line[curr_pos])) { ++curr_pos; }

        // 5. Handle optional comments
        if (curr_pos < line.size()) {
            if (line.compare(curr_pos, 2, "//") == 0) {
                // rest of line is comment -> ok
            } else if (line.compare(curr_pos, 2, "/*") == 0) {
                curr_pos += 2;
                // find closing */
                size_t close_pos = line.find("*/", curr_pos);
                if (close_pos == std::string_view::npos) {
                    return std::nullopt; // unterminated comment
                }
                curr_pos = close_pos + 2;
                // skip spaces after comment
                while (curr_pos < line.size() && is_space(line[curr_pos])) { ++curr_pos; }
                if (curr_pos != line.size()) {
                    return std::nullopt; // garbage after comment
                }
            } else {
                // found non-comment garbage
                return std::nullopt;
            }
        }

        return std::make_pair(
            (first_char == '"')
            ? include_directive_type::quotes
            : include_directive_type::angle_brackets,
            std::move(include_path)
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
    // shader_preprocessor_fs implementation
    ////////////////////////////////////////////////////////////////////////////////

    shader_preprocessor_fs::shader_preprocessor_fs()
        : basic_shader_preprocessor(std::bind(
            &shader_preprocessor_fs::_include_resolver, this, 
            std::placeholders::_1, 
            std::placeholders::_2, 
            std::placeholders::_3
        ))
    { }

    std::optional<std::string> shader_preprocessor_fs::process(
        const std::filesystem::path& root_shader_file_path,
        const bool throw_on_error) const
    {
        try
        {
            std::error_code ec;
            std::filesystem::path root_shader_canonical_path = std::filesystem::canonical(
                root_shader_file_path, 
                ec
            );

            if (ec) {
                TRIENGINE_PANIC("Failed to resolve root shader path \"%.*s\" (%s)"
                    , static_cast<int>(root_shader_file_path.string().size())
                    , root_shader_file_path.string().data()
                    , ec.message().c_str()
                );
            }

            std::string root_shader_file_content;
            if (!this->_read_shader_file_content(root_shader_canonical_path, root_shader_file_content)) {
                TRIENGINE_PANIC("Failed to read root shader file content from \"%s\""
                    , root_shader_canonical_path.generic_string().c_str()
                );
            }

            return this->process_from_memory(
                root_shader_file_content,
                root_shader_canonical_path.generic_string(),
                throw_on_error
            );
        }
        catch (const std::exception& e)
        {
            if (throw_on_error) {
                throw;
            }

            TRIENGINE_ERROR("%s(): Error processing shader file \"%.*s\": %s"
                , __func__
                , static_cast<int>(root_shader_file_path.generic_string().size())
                , root_shader_file_path.generic_string().data()
                , e.what()
            );
        }

        return std::nullopt;
    }

    std::optional<shader_preprocessor_fs::resolved_include_info_t> shader_preprocessor_fs::_include_resolver(
        const std::string_view current_file_canonical_path,
        const include_directive_type parsed_include_type,
        const std::string_view parsed_include_path) const
    {
        if (parsed_include_type == include_directive_type::quotes) // Handle: `#include "path/to/file.glsl"`
        {
            // Normalize the include path.
            // Note: use `weakly_canonical()` as it doesn't require the file to exist yet, which is more robust.
            // `canonical()` can be used as well if you are sure the path is valid.
            std::error_code ec;
            std::filesystem::path parsed_include_canonical_path = std::filesystem::weakly_canonical(
                std::filesystem::path{ current_file_canonical_path }.parent_path() / parsed_include_path,
                ec
            );

            if (ec) {
                TRIENGINE_ERROR("Failed to resolve path for \"%.*s\" relative to \"%.*s\". Reason: %s"
                    , static_cast<int>(parsed_include_path.size()), parsed_include_path.data()
                    , static_cast<int>(current_file_canonical_path.size()), current_file_canonical_path.data()
                    , ec.message().c_str()
                );
                return std::nullopt;
            }

            // Avoid self-including
            if (std::filesystem::path{ current_file_canonical_path } == parsed_include_canonical_path) {
                TRIENGINE_ERROR("Self-include detected: \"%s\""
                    , parsed_include_canonical_path.generic_string().c_str()
                );
                return std::nullopt;
            }

            std::string file_content;
            if (!this->_read_shader_file_content(parsed_include_canonical_path, file_content)) {
                TRIENGINE_ERROR("Failed to read file content from \"%s\""
                    , parsed_include_canonical_path.generic_string().c_str()
                );
                return std::nullopt;
            }

            return resolved_include_info_t{
                parsed_include_canonical_path.generic_string(),
                std::move(file_content)
            };
        }
        else
        {
            TRIENGINE_ERROR("Unsupported include directive type: %d"
                , static_cast<int>(parsed_include_type)
            );
        }

        return std::nullopt;
    }

    bool shader_preprocessor_fs::_read_shader_file_content(
        const std::filesystem::path& file_path,
        std::string& file_content/* out */) const
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

            file_content.clear();
            file_content.reserve(size);
            file_content.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

            // Strip UTF-8 BOM signature (optional)
            if (file_content.size() >= 3 &&
                static_cast<uint8_t>(file_content[0]) == 0xEF &&
                static_cast<uint8_t>(file_content[1]) == 0xBB &&
                static_cast<uint8_t>(file_content[2]) == 0xBF) {
                file_content.erase(0, 3);
            }

            return true;
        }
        catch (const std::exception& e)
        {
            TRIENGINE_ERROR("Failed to read file \"%s\": %s"
                , file_path.generic_string().c_str()
                , e.what()
            );
        }

        return false;
    }

} // namespace