#pragma once
#include <type_traits>
#include <tuple>

namespace triengine::utility
{
    namespace {
        /// The `hash_combine` code is from boost
        /// Reciprocal of the golden ratio helps spread entropy and handles duplicates.
        /// ref: http://stackoverflow.com/questions/4948780
        /// (see Mike Seymour in magic-numbers-in-boosthash-combine)
        template <class _Ty>
        inline void _hash_combine(std::size_t& hash_seed, _Ty const& v) {
            hash_seed ^= std::hash<_Ty>()(v) 
                        + 0x9e3779b9 
                        + (hash_seed << 6) 
                        + (hash_seed >> 2);
        }

        template <class _Tuple, size_t _Index = std::tuple_size<_Tuple>::value - 1>
        struct _hash_value_impl {
            static void apply(size_t& hash_seed, _Tuple const& tuple) {
                _hash_value_impl<_Tuple, _Index - 1>::apply(hash_seed, tuple);
                _hash_combine(hash_seed, std::get<_Index>(tuple));
            }
        };

        template <class _Tuple>
        struct _hash_value_impl<_Tuple, 0> {
            static void apply(size_t& hash_seed, _Tuple const& tuple) {
                _hash_combine(hash_seed, std::get<0>(tuple));
            }
        };
    }  // namespace

    /// `hash_tuple` defines a general hash function for `std::tuple`
    /// ref: http://stackoverflow.com/questions/7110301
    template <typename _Tuple>
    struct hash_tuple {
        size_t operator()(_Tuple const& tt) const { 
            return std::hash<_Tuple>()(tt);
        }
    };

    template <typename... _Ty>
    struct hash_tuple<std::tuple<_Ty...>> {
        size_t operator()(std::tuple<_Ty...> const& tt) const {
            size_t hash_seed = 0;
            _hash_value_impl<std::tuple<_Ty...>>::apply(hash_seed, tt);
            return hash_seed;
        }
    };

    template <typename _EigenMat>
    struct hash_eigen {
        std::size_t operator()(_EigenMat const& eigen_matrix) const {
            size_t hash_seed = 0;
            for (size_t i = 0; i < eigen_matrix.size(); ++i) {
                auto elem = *(eigen_matrix.data() + i);
                hash_seed ^= std::hash<typename _EigenMat::Scalar>()(elem) 
                            + 0x9e3779b9 
                            + (hash_seed << 6) 
                            + (hash_seed >> 2);
            }
            return hash_seed;
        }
    };

    // Hash function for `enum class` for C++ standard less than C++14
    // https://stackoverflow.com/a/24847480/1255535
    struct hash_enum_class {
        template <typename _Ty>
        std::size_t operator()(_Ty t) const {
            return static_cast<std::size_t>(t);
        }
    };

} // namespace