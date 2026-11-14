#pragma once
// Implementation of shader_block_layout.hh; include that header instead.

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace triengine::core::shader_block
{
    namespace detail
    {
        template <class _Type>
        inline constexpr bool always_false_v = false;

        template <class _Scalar, int _Rows, int _Columns>
        struct eigen_glsl_type {
            static_assert(always_false_v<_Scalar>, "no GLSL type for this Eigen type");
        };
        template <> struct eigen_glsl_type<float, 2, 1> { using type = glsl::vec2; };
        template <> struct eigen_glsl_type<float, 3, 1> { using type = glsl::vec3; };
        template <> struct eigen_glsl_type<float, 4, 1> { using type = glsl::vec4; };
        template <> struct eigen_glsl_type<int32_t, 2, 1> { using type = glsl::ivec2; };
        template <> struct eigen_glsl_type<int32_t, 3, 1> { using type = glsl::ivec3; };
        template <> struct eigen_glsl_type<int32_t, 4, 1> { using type = glsl::ivec4; };
        template <> struct eigen_glsl_type<uint32_t, 2, 1> { using type = glsl::uvec2; };
        template <> struct eigen_glsl_type<uint32_t, 3, 1> { using type = glsl::uvec3; };
        template <> struct eigen_glsl_type<uint32_t, 4, 1> { using type = glsl::uvec4; };
        template <> struct eigen_glsl_type<float, 3, 3> { using type = glsl::mat3; };
        template <> struct eigen_glsl_type<float, 4, 4> { using type = glsl::mat4; };
    } // namespace

    // GLSL type of a non-struct, non-array field.
    template <class _Type>
    struct glsl_type_of {
        static_assert(detail::always_false_v<_Type>, "no GLSL type for this C++ type");
    };

    template <> struct glsl_type_of<float> { using type = glsl::float_; };
    template <> struct glsl_type_of<int32_t> { using type = glsl::int_; };
    template <> struct glsl_type_of<uint32_t> { using type = glsl::uint_; };
    template <> struct glsl_type_of<color3_f32> { using type = glsl::vec3; };
    template <> struct glsl_type_of<color4_f32> { using type = glsl::vec4; };

    template <class _Scalar, int _Rows, int _Columns, int _Options, int _MaxRows, int _MaxColumns>
    struct glsl_type_of<Eigen::Matrix<_Scalar, _Rows, _Columns, _Options, _MaxRows, _MaxColumns>> {
        static_assert(_Columns == 1 || (_Options & Eigen::RowMajor) == 0, "GLSL matrices are column-major");
        using type = typename detail::eigen_glsl_type<_Scalar, _Rows, _Columns>::type;
    };

    template <class _Type, class = void>
    struct is_shader_block_struct : std::false_type {};

    template <class _Type>
    struct is_shader_block_struct<_Type, std::void_t<decltype(_Type::shader_block_fields())>> : std::true_type {};

    template <class _Type>
    inline constexpr bool is_shader_block_struct_v = is_shader_block_struct<_Type>::value;

    namespace detail
    {
        // ---------------------------------------------------------------------------------
        // Field list of a shader block struct (TRIENGINE_SHADER_BLOCK_FIELDS)
        // ---------------------------------------------------------------------------------

        // One entry of a TRIENGINE_SHADER_BLOCK_FIELDS list.
        template <class _Struct, class _Field>
        struct field_descriptor {
            using struct_type = _Struct;
            using field_type = _Field;
            std::string_view name;
            _Field _Struct::* memptr;
        };

        template <class _Struct, class _Field>
        constexpr field_descriptor<_Struct, _Field> make_field(const std::string_view name, _Field _Struct::* const memptr) noexcept {
            return { name, memptr };
        }

        template <class _Struct>
        using fields_of = decltype(_Struct::shader_block_fields());

        template <class _Struct, size_t _Index>
        using field_type_of = typename std::tuple_element_t<_Index, fields_of<_Struct>>::field_type;

        template <class _Struct>
        inline constexpr size_t field_count_v = std::tuple_size_v<fields_of<_Struct>>;

        template <class _MemberPointer>
        struct member_pointer_traits;

        template <class _Struct, class _Field>
        struct member_pointer_traits<_Field _Struct::*> {
            using struct_type = _Struct;
            using field_type = _Field;
        };

        // Whether two field descriptors name the same member. Member pointers of different types cannot even be
        // compared, and never name the same member.
        template <class _Lhs, class _Rhs>
        constexpr bool same_field(const _Lhs& lhs, const _Rhs& rhs) noexcept {
            if constexpr (std::is_same_v<decltype(lhs.memptr), decltype(rhs.memptr)>) {
                return lhs.memptr == rhs.memptr;
            } else {
                return false;
            }
        }

        inline constexpr size_t kNoField = static_cast<size_t>(-1);

        // Position of a field in the TRIENGINE_SHADER_BLOCK_FIELDS list.(0 for the first)
        // `kNoField` if it is not listed. (NOTE: NOT a byte offset)
        template <auto _FieldPointer, size_t... _Indices>
        constexpr size_t get_field_index(std::index_sequence<_Indices...>) noexcept
        {
            constexpr auto fields = member_pointer_traits<decltype(_FieldPointer)>::struct_type::shader_block_fields();
            constexpr auto target = make_field({}, _FieldPointer);

            size_t index = kNoField;
            ((index = same_field(target, std::get<_Indices>(fields)) ? _Indices : index), ...);
            return index;
        }

        template <class _Type>
        struct is_std_array : std::false_type {};

        template <class _Type, size_t _Count>
        struct is_std_array<std::array<_Type, _Count>> : std::true_type {};

        template <class _Type>
        struct is_std_vector : std::false_type {};

        template <class _Type, class _Alloc>
        struct is_std_vector<std::vector<_Type, _Alloc>> : std::true_type {};

        // ---------------------------------------------------------------------------------
        // Checks of a TRIENGINE_SHADER_BLOCK_FIELDS list (compile time)
        // ---------------------------------------------------------------------------------

        template <class _Struct, size_t... _Indices>
        constexpr void check_field_list(std::index_sequence<_Indices...>) noexcept
        {
            constexpr size_t field_count = sizeof...(_Indices);
            static_assert(field_count > 0, "a shader block struct needs a field");

            static_assert((std::is_same_v<typename std::tuple_element_t<_Indices, fields_of<_Struct>>::struct_type, _Struct> && ...),
                "TRIENGINE_SHADER_BLOCK_FIELDS of another struct: a derived struct, or a wrong struct name in the macro");

            static_assert([] {
                // Every field of the list matches exactly one field, itself.
                constexpr auto fields = _Struct::shader_block_fields();
                const auto times_listed = [&](const auto& field) {
                    size_t count = 0;
                    ((count += same_field(field, std::get<_Indices>(fields)) ? 1 : 0), ...);
                    return count;
                };
                return ((times_listed(std::get<_Indices>(fields)) == 1) && ...);
            }(), "a field is listed more than once in TRIENGINE_SHADER_BLOCK_FIELDS");

            static_assert(((!is_std_vector<field_type_of<_Struct, _Indices>>::value || _Indices + 1 == field_count) && ...),
                "a std::vector field (runtime-sized array) must be the last field");
        }

        // ---------------------------------------------------------------------------------
        // Layout computation (compile time)
        //   get_type_layout<rule, T>()     alignment, size and array stride of one field type
        //   get_array_layout<rule, T>()    alignment and stride of an array of T
        //   get_struct_layout<rule, S>()   offset of every field of S, and the alignment and size of S
        // A nested struct makes get_type_layout call get_struct_layout, which calls get_type_layout for its fields.
        // ---------------------------------------------------------------------------------

        // Layout of one field type: a GLSL type, a std::array, or a nested shader block struct.
        struct type_layout_info {
            size_t alignment;    // the field's offset is a multiple of this
            size_t size;         // bytes the field takes; the next field may start right after
            size_t array_stride; // distance between elements when this type is an array element; 0 if it cannot be one
        };

        // Layout of a shader block struct.
        template <size_t _FieldCount>
        struct struct_layout_info {
            std::array<size_t, _FieldCount> field_offsets{}; // offset of each field from the struct start, in listing order
            size_t alignment = 1;                            // the struct's offset is a multiple of this
            size_t size = 0;                                 // bytes the struct takes; with a runtime-sized array, the bytes before it
            size_t runtime_array_stride = 0;                 // distance between runtime-sized array elements; 0 without one
            bool has_runtime_array = false;                  // the last field is a std::vector
        };

        // Layout of an array (a std::array field, or the runtime-sized array).
        struct array_layout_info {
            size_t alignment; // the array's offset is a multiple of this
            size_t stride;    // distance between consecutive elements
        };

        template <layout_rule _Rule, class _Type>
        constexpr type_layout_info get_type_layout() noexcept;

        // An array aligns like its element, rounded up to a vec4 under std140.
        template <layout_rule _Rule, class _Element>
        constexpr array_layout_info get_array_layout() noexcept
        {
            static_assert(!is_std_array<_Element>::value && !is_std_vector<_Element>::value, "arrays of arrays are not supported");
            constexpr type_layout_info element = get_type_layout<_Rule, _Element>();
            const size_t alignment = _Rule == layout_rule::std140 ? round_up(element.alignment, kVec4Alignment) : element.alignment;
            return { alignment, element.array_stride };
        }

        // Places the fields one after another, each at the next multiple of its alignment, then pads the struct
        // to a multiple of its own alignment. A runtime-sized array goes last; its elements are not in `size`.
        template <layout_rule _Rule, class _Struct>
        constexpr struct_layout_info<field_count_v<_Struct>> get_struct_layout() noexcept
        {
            static_assert(is_shader_block_struct_v<_Struct>, "list the fields with TRIENGINE_SHADER_BLOCK_FIELDS");
            check_field_list<_Struct>(std::make_index_sequence<field_count_v<_Struct>>{});

            struct_layout_info<field_count_v<_Struct>> info{};
            size_t index = 0;
            size_t end = 0;           // where the fields placed so far end
            size_t max_alignment = 1; // largest field alignment so far; the struct aligns to it

            const auto place_field = [&](const auto& field) {
                using field_type = typename std::decay_t<decltype(field)>::field_type;
                if constexpr (is_std_vector<field_type>::value) {
                    // Runtime-sized array: only its start is placed; its elements come after `size`.
                    constexpr array_layout_info array = get_array_layout<_Rule, typename field_type::value_type>();
                    info.field_offsets[index] = round_up(end, array.alignment);
                    info.runtime_array_stride = array.stride;
                    info.has_runtime_array = true;
                    end = info.field_offsets[index];
                    max_alignment = std::max(max_alignment, array.alignment);
                } else {
                    // Any other field: at the next multiple of its alignment.
                    constexpr type_layout_info type = get_type_layout<_Rule, field_type>();
                    info.field_offsets[index] = round_up(end, type.alignment);
                    end = info.field_offsets[index] + type.size;
                    max_alignment = std::max(max_alignment, type.alignment);
                }
                ++index;
            };
            std::apply(
                [&](const auto&... fields) { (place_field(fields), ...); }, 
                _Struct::shader_block_fields()
            );

            info.alignment = struct_alignment(max_alignment, _Rule);
            info.size = info.has_runtime_array ? end : struct_size(end, info.alignment);
            return info;
        }

        template <layout_rule _Rule, class _Type>
        constexpr type_layout_info get_type_layout() noexcept
        {
            if constexpr (is_shader_block_struct_v<_Type>) {
                // A nested struct; in an array, its elements are one struct size apart.
                constexpr auto info = get_struct_layout<_Rule, _Type>();
                static_assert(!info.has_runtime_array, "only the outermost shader block struct may end with a std::vector field");
                return { info.alignment, info.size, info.size };
            } else if constexpr (is_std_array<_Type>::value) {
                // A fixed-size array; it cannot itself be an array element.
                static_assert(std::tuple_size_v<_Type> > 0, "GLSL arrays have at least one element");
                constexpr array_layout_info array = get_array_layout<_Rule, typename _Type::value_type>();
                return { array.alignment, array.stride * std::tuple_size_v<_Type>, 0 };
            } else if constexpr (is_std_vector<_Type>::value) {
                // A runtime-sized array anywhere but the last field of the outermost struct (`get_struct_layout` places that one).
                static_assert(always_false_v<_Type>, "a std::vector field must be the last field of the outermost shader block struct");
                return {};
            } else {
                // A GLSL scalar, vector or matrix: from the table.
                using glsl_type = typename glsl_type_of<_Type>::type;
                return { glsl_type::alignment, glsl_type::size, glsl_type::array_stride(_Rule) };
            }
        }

        // ---------------------------------------------------------------------------------
        // Serialization (runtime)
        // ---------------------------------------------------------------------------------

        // Writes `value` at `dst`, where its bytes start in the block.
        template <layout_rule _Rule, class _Type>
        void write_value(std::byte* const dst, const _Type& value)
        {
            if constexpr (is_shader_block_struct_v<_Type>)
            {
                // Shader block struct: each field at its offset.
                constexpr auto info = get_struct_layout<_Rule, _Type>();
                size_t index = 0;
                const auto write_field = [&](const auto& field) {
                    std::byte* const field_dst = dst + info.field_offsets[index++];
                    using field_type = std::remove_cv_t<std::remove_reference_t<decltype(field)>>;
                    if constexpr (is_std_vector<field_type>::value) {
                        // Runtime-sized array: its elements one stride apart.
                        for (size_t i = 0; i < field.size(); ++i) {
                            write_value<_Rule>(field_dst + i * info.runtime_array_stride, field[i]);
                        }
                    } else {
                        // Any other field: written by type.
                        write_value<_Rule>(field_dst, field);
                    }
                };
                // A comma fold runs left to right, so `index` follows the field order.
                std::apply(
                    [&](const auto&... fields) { (write_field(value.*(fields.memptr)), ...); },
                    _Type::shader_block_fields()
                );
            }
            else if constexpr (is_std_array<_Type>::value)
            {
                // Fixed-size array: its elements one stride apart.
                constexpr size_t stride = get_array_layout<_Rule, typename _Type::value_type>().stride;
                for (size_t i = 0; i < value.size(); ++i) {
                    write_value<_Rule>(dst + i * stride, value[i]);
                }
            }
            else
            {
                // GLSL scalar, vector or matrix: copies the components.
                using glsl_type = typename glsl_type_of<_Type>::type;
                constexpr size_t column_bytes = sizeof(typename glsl_type::component_type) * glsl_type::rows;
                const auto* const src = [&] {
                    if constexpr (std::is_arithmetic_v<_Type>) {
                        return &value;       // float, int32_t, uint32_t
                    } else {
                        return value.data(); // Eigen types and colors keep their components contiguous
                    }
                }();
                if constexpr (glsl_type::columns == 1) {
                    // Scalar or vector: one run of components.
                    std::memcpy(dst, src, column_bytes);
                } else {
                    // Matrix: one run per column, `column_stride` apart (the gaps are padding).
                    for (uint32_t column = 0; column < glsl_type::columns; ++column) {
                        std::memcpy(dst + column * glsl_type::column_stride, src + column * glsl_type::rows, column_bytes);
                    }
                }
            }
        }

        // Reads `value` from `src`; the inverse of `write_value`. The runtime-sized array gets `runtime_element_count` elements.
        template <layout_rule _Rule, class _Type>
        void read_value(const std::byte* const src, const size_t runtime_element_count, _Type& value)
        {
            if constexpr (is_shader_block_struct_v<_Type>)
            {
                // Shader block struct: each field from its offset.
                constexpr auto info = get_struct_layout<_Rule, _Type>();
                size_t index = 0;
                const auto read_field = [&](auto& field) {
                    const std::byte* const field_src = src + info.field_offsets[index++];
                    using field_type = std::remove_reference_t<decltype(field)>;
                    if constexpr (is_std_vector<field_type>::value) {
                        // Runtime-sized array: resized to `runtime_element_count`, then its elements one stride apart.
                        field.resize(runtime_element_count);
                        for (size_t i = 0; i < field.size(); ++i) {
                            read_value<_Rule>(field_src + i * info.runtime_array_stride, 0, field[i]);
                        }
                    } else {
                        // Any other field: read by type.
                        read_value<_Rule>(field_src, 0, field);
                    }
                };
                // A comma fold runs left to right, so `index` follows the field order.
                std::apply(
                    [&](const auto&... fields) { (read_field(value.*(fields.memptr)), ...); },
                    _Type::shader_block_fields()
                );
            }
            else if constexpr (is_std_array<_Type>::value)
            {
                // Fixed-size array: its elements one stride apart.
                constexpr size_t stride = get_array_layout<_Rule, typename _Type::value_type>().stride;
                for (size_t i = 0; i < value.size(); ++i) {
                    read_value<_Rule>(src + i * stride, 0, value[i]);
                }
            }
            else
            {
                // GLSL scalar, vector or matrix: copies the components back, skipping the padding.
                using glsl_type = typename glsl_type_of<_Type>::type;
                constexpr size_t column_bytes = sizeof(typename glsl_type::component_type) * glsl_type::rows;
                auto* const dst = [&] {
                    if constexpr (std::is_arithmetic_v<_Type>) {
                        return &value;       // float, int32_t, uint32_t
                    } else {
                        return value.data(); // Eigen types and colors keep their components contiguous
                    }
                }();
                if constexpr (glsl_type::columns == 1) {
                    // Scalar or vector: one run of components.
                    std::memcpy(dst, src, column_bytes);
                } else {
                    // Matrix: one run per column, `column_stride` apart.
                    for (uint32_t column = 0; column < glsl_type::columns; ++column) {
                        std::memcpy(dst + column * glsl_type::rows, src + column * glsl_type::column_stride, column_bytes);
                    }
                }
            }
        }

        // Element count of the runtime-sized array (the last std::vector field).
        template <class _Block>
        size_t get_runtime_array_size(const _Block& block) noexcept
        {
            constexpr size_t last = field_count_v<_Block> - 1;
            static_assert(is_std_vector<field_type_of<_Block, last>>::value, "the block has no runtime-sized array");
            return (block.*(std::get<last>(_Block::shader_block_fields()).memptr)).size();
        }

        // ---------------------------------------------------------------------------------
        // Driver check (runtime)
        //   get_expected_variables<rule, B>()    every variable the driver should report for B, under the name it uses
        //   compare_with_driver()                queries the driver and compares (shader_block_layout.cc)
        // Naming follows "Naming Active Resources" (OpenGL 4.6 core profile spec, 7.3.1.1).
        // ---------------------------------------------------------------------------------

        // Component type of a GLSL type: float, int or uint.
        enum class scalar_type { f32, i32, u32 };

        // A member of a shader storage block that is an array of structs, around the variables of its element 0.
        // The driver lists only that element, and reports the array's size and stride on each of its variables.
        struct top_level_array_info {
            bool present = false; // the variable is inside such an array
            size_t size = 0;      // elements; 0 if runtime-sized
            size_t stride = 0;    // distance between elements
        };

        // One variable as the driver should report it.
        struct expected_variable {
            std::string name;               // e.g. `intensity`, `lights[1].color`; an array of basic types as one `splits[0]`
            size_t offset;                  // from the block start
            scalar_type component;          // GLSL type: component, columns and rows
            uint32_t columns;
            uint32_t rows;
            size_t array_size;              // elements of an array of basic types; 0 if runtime-sized; 1 for anything else
            size_t array_stride;            // distance between those elements; 0 for anything else
            size_t matrix_stride;           // distance between matrix columns; 0 for a non-matrix
            top_level_array_info top_level;
        };

        // What `compare_with_driver` compares the driver's report with.
        struct expected_block {
            layout_rule rule;
            bool has_runtime_array;
            size_t data_size;                         // block bytes, counting one runtime-sized array element as the driver does,
                                                      // padded to the block alignment (some drivers pad the end, some do not)
            std::vector<expected_variable> variables; // in GLSL declaration order
        };

        void compare_with_driver(
            uint32_t program, 
            block_type type, 
            std::string_view block_name, 
            uint32_t binding, 
            const expected_block& expected
        );

        template <class _Type>
        struct is_array_of_structs : std::false_type {};

        template <class _Element, size_t _Count>
        struct is_array_of_structs<std::array<_Element, _Count>> : std::bool_constant<is_shader_block_struct_v<_Element>> {};

        template <class _Element, class _Alloc>
        struct is_array_of_structs<std::vector<_Element, _Alloc>> : std::bool_constant<is_shader_block_struct_v<_Element>> {};

        // Declared element count of an array field; 0 for the runtime-sized array.
        template <class _Array>
        constexpr size_t get_array_count() noexcept
        {
            if constexpr (is_std_vector<_Array>::value) {
                return 0;
            } else {
                return std::tuple_size_v<_Array>;
            }
        }

        // An entry for a variable of GLSL scalar, vector or matrix type `_Type`, or an array of them.
        template <class _Type>
        expected_variable make_expected_variable(
            std::string name,
            const size_t offset,
            const size_t array_size,
            const size_t array_stride,
            const top_level_array_info& top_level)
        {
            using glsl_type = typename glsl_type_of<_Type>::type;
            using component_type = typename glsl_type::component_type;
            const scalar_type component = std::is_same_v<component_type, float> ? scalar_type::f32
                : std::is_same_v<component_type, int32_t> ? scalar_type::i32 : scalar_type::u32;
            return {
                std::move(name), offset,
                component, glsl_type::columns, glsl_type::rows,
                array_size, array_stride, glsl_type::column_stride,
                top_level,
            };
        }

        // Appends the variables of a field of type `_Type` named `name` at `offset`, below the members of the block itself.
        template <layout_rule _Rule, class _Type>
        void collect_variables(
            std::vector<expected_variable>& variables,
            const std::string& name,
            const size_t offset,
            const top_level_array_info& top_level)
        {
            if constexpr (is_shader_block_struct_v<_Type>)
            {
                // Shader block struct: each field as `name.field`.
                constexpr auto info = get_struct_layout<_Rule, _Type>();
                size_t index = 0;
                const auto collect_field = [&](const auto& field) {
                    using field_type = typename std::decay_t<decltype(field)>::field_type;
                    const size_t field_offset = offset + info.field_offsets[index++];
                    collect_variables<_Rule, field_type>(variables, name + "." + std::string(field.name), field_offset, top_level);
                };
                std::apply(
                    [&](const auto&... fields) { (collect_field(fields), ...); },
                    _Type::shader_block_fields()
                );
            }
            else if constexpr (is_array_of_structs<_Type>::value)
            {
                // Array of structs: each element as `name[i]`.
                using element_type = typename _Type::value_type;
                constexpr size_t stride = get_array_layout<_Rule, element_type>().stride;
                for (size_t i = 0; i < get_array_count<_Type>(); ++i) {
                    collect_variables<_Rule, element_type>(variables, utility::string::c_format("%s[%zu]", name.c_str(), i), offset + i * stride, top_level);
                }
            }
            else if constexpr (is_std_array<_Type>::value || is_std_vector<_Type>::value)
            {
                // Array of basic types: one entry `name[0]` for the whole array.
                using element_type = typename _Type::value_type;
                constexpr size_t stride = get_array_layout<_Rule, element_type>().stride;
                variables.push_back(make_expected_variable<element_type>(name + "[0]", offset, get_array_count<_Type>(), stride, top_level));
            }
            else
            {
                // GLSL scalar, vector or matrix.
                variables.push_back(make_expected_variable<_Type>(name, offset, 1, 0, top_level));
            }
        }

        // Every variable the driver should report for `_Block`. A member of the block itself is named by its field name,
        // except that a shader storage block member that is an array of structs is listed only for element 0: a top-level
        // array (OpenGL 4.6 spec 7.3.1.1). Uniform blocks list every element to keep the indices of the older
        // `glGetActiveUniform`; see ARB_program_interface_query, issues (4) to (6).
        template <layout_rule _Rule, class _Block>
        std::vector<expected_variable> get_expected_variables(const block_type type)
        {
            constexpr auto info = get_struct_layout<_Rule, _Block>();
            std::vector<expected_variable> variables;
            size_t index = 0;
            const auto collect_member = [&](const auto& field) {
                using field_type = typename std::decay_t<decltype(field)>::field_type;
                const std::string name{ field.name };
                const size_t offset = info.field_offsets[index++];
                if constexpr (is_array_of_structs<field_type>::value) {
                    if (type == block_type::shader_storage) {
                        // Array of structs in a shader storage block: element 0 only, with the array's size and stride.
                        using element_type = typename field_type::value_type;
                        constexpr size_t stride = get_array_layout<_Rule, element_type>().stride;
                        const top_level_array_info top_level{ true, get_array_count<field_type>(), stride };
                        collect_variables<_Rule, element_type>(variables, name + "[0]", offset, top_level);
                        return;
                    }
                }
                // Any other member: as below the block's members.
                collect_variables<_Rule, field_type>(variables, name, offset, top_level_array_info{});
            };
            std::apply(
                [&](const auto&... fields) { (collect_member(fields), ...); },
                _Block::shader_block_fields()
            );
            return variables;
        }

    } // namespace

    template <layout_rule _Rule, class _Block>
    constexpr size_t block_layout<_Rule, _Block>::size() noexcept {
        constexpr auto info = detail::get_struct_layout<_Rule, _Block>();
        return info.size;
    }

    template <layout_rule _Rule, class _Block>
    constexpr size_t block_layout<_Rule, _Block>::runtime_array_stride() noexcept {
        constexpr auto info = detail::get_struct_layout<_Rule, _Block>();
        return info.runtime_array_stride;
    }

    template <layout_rule _Rule, class _Block>
    constexpr size_t block_layout<_Rule, _Block>::size_for(const size_t element_count) noexcept {
        return size() + element_count * runtime_array_stride();
    }

    template <layout_rule _Rule, class _Block>
    template <auto _FieldPointer>
    constexpr size_t block_layout<_Rule, _Block>::offset_of() noexcept
    {
        using struct_type = typename detail::member_pointer_traits<decltype(_FieldPointer)>::struct_type;
        static_assert(std::is_same_v<struct_type, _Block>, "not a field of this block; use the layout of the struct that has it");

        constexpr size_t index = detail::get_field_index<_FieldPointer>(std::make_index_sequence<detail::field_count_v<_Block>>{});
        static_assert(index != detail::kNoField, "not a field listed with TRIENGINE_SHADER_BLOCK_FIELDS");

        return detail::get_struct_layout<_Rule, _Block>().field_offsets[index];
    }

    template <layout_rule _Rule, class _Block>
    void block_layout<_Rule, _Block>::serialize(
        const _Block& block, 
        std::vector<std::byte>& bytes)
    {
        size_t element_count = 0;
        if constexpr (runtime_array_stride() > 0) {
            element_count = detail::get_runtime_array_size(block);
            if (element_count > (std::numeric_limits<size_t>::max() - size()) / runtime_array_stride()) {
                TRIENGINE_PANIC("%zu runtime-sized array elements do not fit in a byte count", element_count);
            }
        }
        bytes.assign(size_for(element_count), std::byte{ 0 });
        detail::write_value<_Rule>(bytes.data(), block);
    }

    template <layout_rule _Rule, class _Block>
    template <class _Element>
    void block_layout<_Rule, _Block>::serialize_element(
        const size_t index, 
        const _Element& element, 
        std::byte* const bytes, 
        const size_t byte_count)
    {
        constexpr auto info = detail::get_struct_layout<_Rule, _Block>();
        static_assert(info.has_runtime_array, "the block has no runtime-sized array");
        static_assert(std::is_same_v<_Element, typename detail::field_type_of<_Block, detail::field_count_v<_Block> - 1>::value_type>,
            "not the runtime-sized array's element type");

        const size_t capacity = byte_count < info.size ? 0 : (byte_count - info.size) / info.runtime_array_stride;
        if (index >= capacity) {
            TRIENGINE_PANIC("Runtime-sized array element %zu is outside the %zu elements of the %zu-byte block", index, capacity, byte_count);
        }
        const size_t offset = info.size + index * info.runtime_array_stride;
        std::memset(bytes + offset, 0, info.runtime_array_stride);
        detail::write_value<_Rule>(bytes + offset, element);
    }

    template <layout_rule _Rule, class _Block>
    void block_layout<_Rule, _Block>::deserialize(
        const std::byte* const bytes, 
        const size_t byte_count, 
        _Block& block)
    {
        constexpr auto info = detail::get_struct_layout<_Rule, _Block>();
        if (byte_count < info.size) {
            TRIENGINE_PANIC("%zu bytes are fewer than the %zu-byte block", byte_count, info.size);
        }
        const size_t element_count = info.has_runtime_array ? (byte_count - info.size) / info.runtime_array_stride : 0;
        detail::read_value<_Rule>(bytes, element_count, block);
    }

    template <layout_rule _Rule, class _Block>
    void block_layout<_Rule, _Block>::verify(
        const uint32_t program,
        const block_type type,
        const std::string_view block_name,
        const uint32_t binding)
    {
        constexpr auto info = detail::get_struct_layout<_Rule, _Block>();
        const detail::expected_block expected{
            _Rule,
            info.has_runtime_array,
            round_up(size_for(info.has_runtime_array ? 1 : 0), info.alignment),
            detail::get_expected_variables<_Rule, _Block>(type),
        };
        detail::compare_with_driver(program, type, block_name, binding, expected);
    }

} // namespace

#define _TRIENGINE_SHADER_BLOCK_FIELD(STRUCT, FIELD) ::triengine::core::shader_block::detail::make_field(#FIELD, &STRUCT::FIELD)

// `M(S, x)` for each `x`, comma-separated. `_TRIENGINE_SHADER_BLOCK_EXPAND` makes MSVC's traditional preprocessor
// split `__VA_ARGS__` into separate arguments.
#define _TRIENGINE_SHADER_BLOCK_EXPAND(X) X
#define _TRIENGINE_SHADER_BLOCK_PICK( \
    _1, _2, _3, _4, _5, _6, _7, _8, \
    _9, _10, _11, _12, _13, _14, _15, _16, \
    _17, _18, _19, _20, _21, _22, _23, _24, \
    _25, _26, _27, _28, _29, _30, _31, _32, \
    NAME, ...) NAME

#define _TRIENGINE_SHADER_BLOCK_FOR_EACH(M, S, ...) \
    _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_PICK(__VA_ARGS__, \
        _TRIENGINE_SHADER_BLOCK_FOR_EACH_32, _TRIENGINE_SHADER_BLOCK_FOR_EACH_31, _TRIENGINE_SHADER_BLOCK_FOR_EACH_30, _TRIENGINE_SHADER_BLOCK_FOR_EACH_29, \
        _TRIENGINE_SHADER_BLOCK_FOR_EACH_28, _TRIENGINE_SHADER_BLOCK_FOR_EACH_27, _TRIENGINE_SHADER_BLOCK_FOR_EACH_26, _TRIENGINE_SHADER_BLOCK_FOR_EACH_25, \
        _TRIENGINE_SHADER_BLOCK_FOR_EACH_24, _TRIENGINE_SHADER_BLOCK_FOR_EACH_23, _TRIENGINE_SHADER_BLOCK_FOR_EACH_22, _TRIENGINE_SHADER_BLOCK_FOR_EACH_21, \
        _TRIENGINE_SHADER_BLOCK_FOR_EACH_20, _TRIENGINE_SHADER_BLOCK_FOR_EACH_19, _TRIENGINE_SHADER_BLOCK_FOR_EACH_18, _TRIENGINE_SHADER_BLOCK_FOR_EACH_17, \
        _TRIENGINE_SHADER_BLOCK_FOR_EACH_16, _TRIENGINE_SHADER_BLOCK_FOR_EACH_15, _TRIENGINE_SHADER_BLOCK_FOR_EACH_14, _TRIENGINE_SHADER_BLOCK_FOR_EACH_13, \
        _TRIENGINE_SHADER_BLOCK_FOR_EACH_12, _TRIENGINE_SHADER_BLOCK_FOR_EACH_11, _TRIENGINE_SHADER_BLOCK_FOR_EACH_10, _TRIENGINE_SHADER_BLOCK_FOR_EACH_9, \
        _TRIENGINE_SHADER_BLOCK_FOR_EACH_8, _TRIENGINE_SHADER_BLOCK_FOR_EACH_7, _TRIENGINE_SHADER_BLOCK_FOR_EACH_6, _TRIENGINE_SHADER_BLOCK_FOR_EACH_5, \
        _TRIENGINE_SHADER_BLOCK_FOR_EACH_4, _TRIENGINE_SHADER_BLOCK_FOR_EACH_3, _TRIENGINE_SHADER_BLOCK_FOR_EACH_2, _TRIENGINE_SHADER_BLOCK_FOR_EACH_1 \
    )(M, S, __VA_ARGS__))

#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_1(M, S, X) M(S, X)
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_2(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_1(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_3(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_2(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_4(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_3(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_5(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_4(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_6(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_5(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_7(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_6(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_8(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_7(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_9(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_8(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_10(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_9(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_11(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_10(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_12(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_11(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_13(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_12(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_14(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_13(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_15(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_14(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_16(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_15(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_17(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_16(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_18(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_17(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_19(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_18(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_20(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_19(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_21(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_20(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_22(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_21(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_23(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_22(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_24(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_23(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_25(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_24(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_26(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_25(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_27(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_26(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_28(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_27(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_29(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_28(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_30(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_29(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_31(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_30(M, S, __VA_ARGS__))
#define _TRIENGINE_SHADER_BLOCK_FOR_EACH_32(M, S, X, ...) M(S, X), _TRIENGINE_SHADER_BLOCK_EXPAND(_TRIENGINE_SHADER_BLOCK_FOR_EACH_31(M, S, __VA_ARGS__))
