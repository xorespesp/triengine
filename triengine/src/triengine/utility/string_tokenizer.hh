#pragma once
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/noncopyable.hh>

#include <string>
#include <string_view>
#include <set>

namespace triengine::string
{
    enum class empty_token_policy_type { 
        drop_empty_tokens, 
        keep_empty_tokens, 
    };

    namespace tokenizer_detail
    {
        // Ref: https://stackoverflow.com/questions/72663370/allow-only-explicit-specialization-of-template-class
        template<class _StringContainer>
        struct token_string_assigner_trait {
            static inline void clear(_StringContainer& obj) = delete;
            static inline void assign(_StringContainer& obj, const typename _StringContainer::value_type* str, size_t len) = delete;
        };

        template<>
        struct token_string_assigner_trait<std::string> {
            static inline void clear(std::string& obj) {
                obj.clear();
            }

            static inline void assign(std::string& obj, const std::string_view::value_type* str, size_t len) {
                obj.assign(str, len);
            }
        };

        template<>
        struct token_string_assigner_trait<std::string_view> {
            static inline void clear(std::string_view& obj) {
                obj = std::string_view{};
            }

            static inline void assign(std::string_view& obj, const std::string_view::value_type* str, size_t len) {
                obj = std::string_view{ str, len };
            }
        };

        template <
            class _DeriveImpl,
            class _CharT
        >
        struct tokenizer_method_trait
        {
            using char_type = _CharT;
            using const_char_pointer = _CharT const*;
            using string_type = std::basic_string<_CharT>;
            using string_view_type = std::basic_string_view<_CharT>;

            // CRTP method
            void reset()
            {
                static_assert(std::is_base_of<tokenizer_method_trait<_DeriveImpl, _CharT>, _DeriveImpl>::value, "!!");
                return static_cast<_DeriveImpl*>(this)->reset_impl();
            }

            // CRTP method
            auto process_token(
                const_char_pointer& curr_ptr/* inout */,
                const_char_pointer const end_ptr,
                string_view_type& result_token/* input */
            ) -> bool/* is valid token? */
            {
                static_assert(std::is_base_of<tokenizer_method_trait<_DeriveImpl, _CharT>, _DeriveImpl>::value, "!!");
                return static_cast<_DeriveImpl*>(this)->process_token_impl(curr_ptr, end_ptr, result_token);
            }

            // helper
            bool operator()(
                const_char_pointer& curr_ptr/* inout */,
                const_char_pointer const end_ptr,
                string_view_type& result_token/* input */)
            {
                return this->process_token(curr_ptr, end_ptr, result_token);
            }
        };

    } // namespace

    namespace tokenizer_methods
    {
        // Ref: https://www.boost.org/doc/libs/1_58_0/libs/tokenizer/char_separator.htm
        template <
            class _CharT
        >
        class basic_char_separator
            : public tokenizer_detail::tokenizer_method_trait<basic_char_separator<_CharT>, _CharT>
        {
            using super = typename tokenizer_detail::tokenizer_method_trait<basic_char_separator<_CharT>, _CharT>;

        public:
            using typename super::char_type;
            using typename super::const_char_pointer;
            using typename super::string_type;
            using typename super::string_view_type;

        private:
            const empty_token_policy_type _empty_token_policy;
            std::set<char_type> _drop_delims; 
            std::set<char_type> _keep_delims;

        public:
            basic_char_separator(
                empty_token_policy_type empty_token_policy,
                std::set<char_type> drop_delims,
                std::set<char_type> keep_delims = "") noexcept
                : _empty_token_policy{ empty_token_policy }
                , _drop_delims{ std::move(drop_delims) }
                , _keep_delims{ std::move(keep_delims) }
            { }

            basic_char_separator(
                empty_token_policy_type empty_token_policy,
                string_view_type drop_delims,
                string_view_type keep_delims = "") noexcept
                : _empty_token_policy{ empty_token_policy }
                , _drop_delims{ drop_delims.begin(), drop_delims.end() }
                , _keep_delims{ keep_delims.begin(), keep_delims.end() }
            { }

            const std::set<char_type>& drop_delims() const noexcept { return _drop_delims; }
            std::set<char_type>& drop_delims() noexcept { return _drop_delims; }

            const std::set<char_type>& keep_delims() const noexcept { return _keep_delims; }
            std::set<char_type>& keep_delims() noexcept { return _keep_delims; }

            /*! CRTP override !*/
            void reset_impl() { }

            /*! CRTP override !*/
            bool process_token_impl(
                const_char_pointer& curr_ptr/* inout */,
                const_char_pointer const end_ptr,
                string_view_type& result_token/* inout */)
            {
                TRIENGINE_ASSERT(!_drop_delims.empty() || !_keep_delims.empty());

                using token_string_assigner = tokenizer_detail::token_string_assigner_trait<string_view_type>;

                // clear current token (make it empty)
                token_string_assigner::clear(result_token);

                if (_empty_token_policy == empty_token_policy_type::drop_empty_tokens)
                {
                    // skip past all drop-delimiters.
                    while (
                        curr_ptr != end_ptr && 
                        this->_is_drop_delim(*curr_ptr)
                        )
                    { ++curr_ptr; }

                    // if we reach the end, then stop.
                    if (curr_ptr == end_ptr) {
                        //TRIENGINE_DEBUG(L"END!!\n");
                        return false;
                    }

                    // if we meet a keep-delimiter, make it single token & move past it, and stop.
                    if (this->_is_keep_delim(*curr_ptr)) {
                        token_string_assigner::assign(result_token, curr_ptr/* tok_begin */, 1); // make single token (keep-delimiter only)
                        ++curr_ptr; // move past it
                        return true; // and stop
                    }

                    // or else, append all the non-delim characters, and stop.
                    {
                        auto tok_begin = curr_ptr;
                        while (
                            curr_ptr != end_ptr &&
                            !this->_is_drop_delim(*curr_ptr) &&
                            !this->_is_keep_delim(*curr_ptr)
                            )
                        { ++curr_ptr; /* move past it */ }
                        token_string_assigner::assign(result_token, tok_begin, (curr_ptr/* tok_end */ - tok_begin));
                        return true;
                    }
                }
                else if (_empty_token_policy == empty_token_policy_type::keep_empty_tokens)
                {
                    // if we reach the end, then stop.
                    if (curr_ptr == end_ptr) {
                        //TRIENGINE_DEBUG(L"END!!\n");
                        return false;
                    }

                    // if we meet a keep-delimiter, make it single token & move past it, and stop.
                    if (this->_is_keep_delim(*curr_ptr)) {
                        token_string_assigner::assign(result_token, curr_ptr/* tok_begin */, 1); // make single token (keep-delimiter only)
                        ++curr_ptr; // move past it
                        return true; // and stop
                    }

                    // if we meet a drop-delimiter, make it empty token & move past it, and stop.
                    if (this->_is_drop_delim(*curr_ptr)) {
                        ++curr_ptr; // move past it (& make empty token)
                        return true; // and stop
                    }

                    // or else, append all the non-delim characters, and stop.
                    {
                        auto tok_begin = curr_ptr;
                        while (
                            curr_ptr != end_ptr &&
                            !this->_is_drop_delim(*curr_ptr) &&
                            !this->_is_keep_delim(*curr_ptr)
                            )
                        { ++curr_ptr; /* move past it */ }
                        token_string_assigner::assign(result_token, tok_begin, (curr_ptr/* tok_end */ - tok_begin));
                        return true;
                    }
                }

                //TRIENGINE_DEBUG(L"UNKNOWN TOKEN POLICY\n");
                return false;
            }

        private:
            inline bool _is_drop_delim(char_type ch) const noexcept {
                if (_drop_delims.empty()) { return false; }
                return _drop_delims.count(ch) > 0;
            }

            inline bool _is_keep_delim(char_type ch) const noexcept {
                if (_keep_delims.empty()) { return false; }
                return _keep_delims.count(ch) > 0;
            }

        }; // class

        using char_separator = basic_char_separator<char>;

    } // namespace

    template <
        class _TokenizerMethod = tokenizer_methods::char_separator
    >
    class tokenizer
        : utility::noncopyable
    {
        using string_type = typename _TokenizerMethod::string_type;
        using string_view_type = typename _TokenizerMethod::string_view_type;
        using const_char_pointer = typename _TokenizerMethod::const_char_pointer;
        using method_type = _TokenizerMethod;

    public:
        //------------------------------------------------------------------------
        // Iterator
        //------------------------------------------------------------------------
        class iterator {
        private:
            tokenizer* const _tokenizer;
            const_char_pointer _str_begin;
            const_char_pointer const _str_end;
            string_view_type _curr_token;
            bool _is_curr_token_valid;

        public:
            iterator(
                tokenizer* tokenizer,
                const_char_pointer str_begin, 
                const_char_pointer str_end)
                : _tokenizer{ tokenizer }
                , _str_begin{ str_begin }
                , _str_end{ str_end }
                , _curr_token{}
                , _is_curr_token_valid{ false }
            { this->_initialize(); }

            bool operator==(const iterator& rhs) const noexcept {
                return this->_is_equal(rhs);
            }

            bool operator!=(const iterator& rhs) const noexcept {
                return !(rhs == *this);
            }

            const auto& operator*() const {
                return this->_dereference();
            }

            iterator operator++() {
                this->_increment();
                return *this;
            }

        private:
            // init & read first token
            void _initialize() {
                if (!_is_curr_token_valid) { // if not initialized
                    _tokenizer->method().reset();
                    if (_str_begin < _str_end && _str_begin != nullptr && _str_end != nullptr) {
                        _is_curr_token_valid = _tokenizer->method().process_token(
                            _str_begin,
                            _str_end,
                            _curr_token
                        );
                    } else {
                        _is_curr_token_valid = false;
                    }
                }
            }

            // read next token
            void _increment() {
                TRIENGINE_ASSERT(_is_curr_token_valid);
                _is_curr_token_valid = _tokenizer->method().process_token(
                    _str_begin/* inout */,
                    _str_end,
                    _curr_token/* inout */
                );
            }

            // get current token
            const string_view_type& _dereference() const {
                TRIENGINE_ASSERT(_is_curr_token_valid);
                return _curr_token;
            }

            bool _is_equal(const iterator& rhs) const noexcept {
                if (rhs._is_curr_token_valid && _is_curr_token_valid) {
                    return (rhs._str_begin == _str_begin) && (rhs._str_end == _str_end);
                } else {
                    return (rhs._is_curr_token_valid == _is_curr_token_valid);
                }
            }

        }; // class iterator

        //------------------------------------------------------------------------

    private:
        method_type _method;
        const string_view_type _target_str;
        const iterator _it_begin, _it_end;
        
    public:
        tokenizer(
            method_type tokenizer_method,
            string_view_type target_str)
            : _method{ tokenizer_method }
            , _target_str{ target_str }
            , _it_begin{ this, (_target_str.data())/* begin */, (_target_str.data() + _target_str.size())/* end */ }
            , _it_end{ this, (_target_str.data() + _target_str.size())/* end */, (_target_str.data() + _target_str.size())/* end */ }
        { }

        iterator begin() const noexcept { return _it_begin; }
        iterator end() const noexcept { return _it_end; }

        const method_type& method() const noexcept { return _method; }
        method_type& method() noexcept { return _method; }

    }; // class

    template <empty_token_policy_type _EmptyTokenPolicy>
    std::vector<std::string> tokenize_by_char_seperator(
        const std::string_view target_sv,
        const std::string_view drop_charset_sv)
    {
        std::vector<std::string> tokens;

        std::string_view::size_type
            scan_pos = 0,
            curr_token_pos = 0,
            curr_token_size = 0;

        while (scan_pos != std::string_view::npos)
        {
            scan_pos = target_sv.find_first_of(drop_charset_sv, scan_pos);
            if (scan_pos != std::string_view::npos) {
                curr_token_size = scan_pos - curr_token_pos;
                scan_pos += 1; // next scan pos
            } else {
                curr_token_size = target_sv.size() - curr_token_pos;
            }

            if (curr_token_size > 0 || _EmptyTokenPolicy == empty_token_policy_type::keep_empty_tokens) {
                const auto token = target_sv.substr(curr_token_pos, curr_token_size);
                tokens.emplace_back(token);
            }

            curr_token_pos = scan_pos; // next token pos == next scan pos
        }

        return tokens;
    }

    template <empty_token_policy_type _EmptyTokenPolicy>
    std::vector<std::string> tokenize_by_delimeter(
        const std::string_view target_sv,
        const std::string_view drop_delim_sv)
    {
        std::vector<std::string> tokens;

        std::string_view::size_type
            scan_pos = 0,
            curr_token_pos = 0,
            curr_token_size = 0;

        while (scan_pos != std::string_view::npos)
        {
            scan_pos = target_sv.find(drop_delim_sv, scan_pos);
            if (scan_pos != std::string_view::npos) {
                curr_token_size = scan_pos - curr_token_pos;
                scan_pos += drop_delim_sv.size(); // next scan pos
            } else {
                curr_token_pos = target_sv.size() - curr_token_pos;
            }

            if (curr_token_size > 0 || _EmptyTokenPolicy == empty_token_policy_type::keep_empty_tokens) {
                const auto token = target_sv.substr(curr_token_pos, curr_token_size);
                tokens.emplace_back(token);
            }

            curr_token_pos = scan_pos; // next token pos == next scan pos
        }

        return tokens;
    }

} // namespace