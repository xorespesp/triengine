#include "file_bvh_loader.hh"

#include <triengine/common.h>
#include <triengine/math/constants.hh>
#include <triengine/math/math3d.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/string_format.hh>
#include <triengine/utility/string_tokenizer.hh>

#include <charconv>
#include <fstream>
#include <sstream>
#include <memory>
#include <unordered_map>
#include <optional>
#include <variant>
#include <list>

namespace triengine::io
{
    namespace
    {
        class bvh_file_parser
        {
		private:
			enum class bvh_channel_type {
				x_position, y_position, z_position,
				x_rotation, y_rotation, z_rotation,
			};

			enum class bvh_hierarchy_node_type {
				root, child,
			};

			using bvh_hierarchy_node_order_t = size_t;

			class bvh_hierarchy_node;
			using bvh_hierarchy_node_ptr = std::shared_ptr<bvh_hierarchy_node>;

			class bvh_hierarchy_node
				: public std::enable_shared_from_this<bvh_hierarchy_node>
			{
			private:
				const bvh_hierarchy_node_type _node_type; // BVH node type
				const std::string _joint_name; // BVH node name (joint name)
				const Eigen::Vector3d _t_pose_offset; // BVH node T-pose offset (relative to parent's offset)
				const std::vector<bvh_channel_type> _channel_types; // BVH node channel types

				std::weak_ptr<bvh_hierarchy_node> _parent_node; // Note: root node's parent is nullptr
				std::variant<
					std::list<bvh_hierarchy_node_ptr>/* BVH child joint node(s) */,
					Eigen::Vector3d/* BVH child end-site t-pose offset (relative to parent's offset) */
				> _child_info;

				using child_nodes_vindex = std::integral_constant<size_t, 0>;
				using child_endsite_vindex = std::integral_constant<size_t, 1>;

				const bvh_hierarchy_node_order_t _node_decl_order; // BVH node declaration order(index), in BVH hierarchy section
				const std::pair<size_t/* begin col idx */, size_t/* end col idx */> _frame_col_range;

			public:
				bvh_hierarchy_node(
					const bvh_hierarchy_node_type node_type,
					const std::string_view joint_name,
					const Eigen::Vector3d& t_pose_offset,
					const std::vector<bvh_channel_type>& node_channel_types,
					const bvh_hierarchy_node_order_t node_decl_order,
					const size_t frame_col_start_offset)
					: _node_type{ node_type }
					, _node_decl_order{ node_decl_order }
					, _joint_name{ joint_name }
					, _t_pose_offset{ t_pose_offset }
					, _channel_types{ node_channel_types }
					, _frame_col_range{ frame_col_start_offset, frame_col_start_offset + _channel_types.size() }
				{
					TRIENGINE_TRACE("CREATE bvh_hierarchy_node@%lu (%p)", _node_decl_order, this);

					// find duplicate
					if (_channel_types.end() != std::adjacent_find(_channel_types.begin(), _channel_types.end())) {
						throw std::runtime_error{ "Invalid BVH node channel type" };
					}
				}

				virtual ~bvh_hierarchy_node() {
					TRIENGINE_TRACE("DESTROY bvh_hierarchy_node@%lu (%p)", _node_decl_order, this);
				}

				bvh_hierarchy_node_type get_node_type() const noexcept {
					return _node_type;
				}

				bvh_hierarchy_node_order_t get_node_order() const noexcept {
					return _node_decl_order;
				}

				const std::string& get_joint_name() const noexcept {
					return _joint_name; 
				}

				const Eigen::Vector3d& get_t_pose_offset() const noexcept {
					return _t_pose_offset;
				}

				const std::vector<bvh_channel_type>& get_channel_types() const noexcept {
					return _channel_types;
				}

				std::pair<size_t/* begin col idx */, size_t/* end col idx */> get_frame_col_range() const noexcept {
					return _frame_col_range; 
				}

				bvh_hierarchy_node_ptr get_parent_node() {
					return
						(_node_type != bvh_hierarchy_node_type::root)
						? _parent_node.lock()
						: this->shared_from_this();
				}

				bool child_is_endsite() const noexcept {
					return _child_info.index() == child_endsite_vindex::value;
				}

				const std::list<bvh_hierarchy_node_ptr>& get_child_nodes() const {
					TRIENGINE_ASSERT(!this->child_is_endsite());
					return std::get<child_nodes_vindex::value>(_child_info);
				}

				const Eigen::Vector3d& get_child_endsite_t_pose_offset() const {
					TRIENGINE_ASSERT(this->child_is_endsite());
					return std::get<child_endsite_vindex::value>(_child_info);
				}

				void add_child_node(bvh_hierarchy_node_ptr new_child_node)
				{
					if (!new_child_node ||
						new_child_node.get() == this ||
						new_child_node->get_node_type() != bvh_hierarchy_node_type::child) {
						throw std::invalid_argument{ "Invalid new child node" };
					}

					if (this->child_is_endsite()) {
						throw std::runtime_error{ "End-site node cannot have child node(s)" };
					}

					new_child_node->_parent_node = this->shared_from_this(); // Set child node's parent
					std::get<child_nodes_vindex::value>(_child_info).push_back(new_child_node);
				}

				void mark_child_as_endsite(const Eigen::Vector3d& endsite_t_pose_offset)
				{
					if (!this->child_is_endsite() && !this->get_child_nodes().empty()) {
						throw std::runtime_error{ "End-site node cannot have child node(s)" };
					}

					_child_info = endsite_t_pose_offset;
				}

				auto get_childs_parent_map()
					-> std::unordered_map<
						bvh_hierarchy_node_ptr/* child */, 
						bvh_hierarchy_node_ptr/* parent */
					> const
				{
					std::unordered_map<bvh_hierarchy_node_ptr/* child */, bvh_hierarchy_node_ptr/* parent */> parent_map;
					this->_get_childs_parent_map_impl(parent_map);
					return parent_map;
				}

				// List of all joint nodes sorted in the order defined in the BVH HIERARCHY section 
				// (sorted in parent joint -> child joint order)
				// This order is usually so that the parent comes before the child, 
				// so the child can be processed while the parent's world transformation has been computed.
				auto get_ordered_childs_list()
					-> std::vector<bvh_hierarchy_node_ptr> const
				{
					std::vector<bvh_hierarchy_node_ptr> ordered_list;
					this->_get_ordered_childs_list_impl(ordered_list);
					return ordered_list;
				}

				// for debug
				std::string dump_childs_hierarchy() const
				{
					std::stringstream out;
					out << std::fixed << std::setprecision(6);
					this->_dump_childs_hierarchy_impl(out);
					return out.str();
				}

			private:
				void _get_childs_parent_map_impl(
					std::unordered_map<bvh_hierarchy_node_ptr/* child */, bvh_hierarchy_node_ptr/* parent */>& parent_map/* inout */)
				{
					const auto [_, success] = parent_map.insert({ this->shared_from_this(), this->get_parent_node() });
					TRIENGINE_ASSERT(success);
					if (!this->child_is_endsite()) {
						for (const auto& child_node : this->get_child_nodes()) {
							child_node->_get_childs_parent_map_impl(parent_map);
						}
					}
				}

				void _get_ordered_childs_list_impl(
					std::vector<bvh_hierarchy_node_ptr>& ordered_list/* inout */)
				{
					ordered_list.push_back(this->shared_from_this());
					if (!this->child_is_endsite()) {
						for (const auto& child_node : this->get_child_nodes()) {
							child_node->_get_ordered_childs_list_impl(ordered_list);
						}
					}
				}

				void _dump_childs_hierarchy_impl(
					std::stringstream& out/* inout */,
					const std::string_view prev_prefixes = "",
					const uint32_t indent_level = 0,
					const bool is_last_branch = true) const
				{
					static constexpr std::string_view
						kVoidSpacePrefixToken  = "    ",
						kTrunkPrefixToken      = "|   ",
						kMidBranchPrefixToken  = "|-- ",
						kLastBranchPrefixToken = "+-- ";

					static constexpr int
						kMaxLinePaddingSize = 80;

					if (!indent_level) {
						// ...
					}

					if (indent_level > 0) {
						out << prev_prefixes << kTrunkPrefixToken << "\n";
					}

					std::string_view next_prefix_token;
					if (is_last_branch) {
						out << prev_prefixes << kLastBranchPrefixToken;
						next_prefix_token = kVoidSpacePrefixToken;
					}
					else {
						out << prev_prefixes << kMidBranchPrefixToken;
						next_prefix_token = kTrunkPrefixToken;
					}

					out << (_node_type == bvh_hierarchy_node_type::root ? "Root" : "Child")
						<< "@" << _node_decl_order
						<< "(" << _joint_name << ")"
						<< " ";

					// Add padding.
					{
						const int
							curr_x_pos = static_cast<int>(out.str().size())/* end pos */ - static_cast<int>(out.str().find_last_of('\n'))/* begin pos */,
							needed_paddings = kMaxLinePaddingSize - curr_x_pos;

						for (int n = 0; n < needed_paddings; ++n) {
							out << ((curr_x_pos + n) % 2 ? "." : " ");
						}
					}

					out << " "
						<< "T-Offset=["
						<< _t_pose_offset.x() << ","
						<< _t_pose_offset.y() << ","
						<< _t_pose_offset.z()
						<< "], "
						<< "FrameCol=[" << _frame_col_range.first << "," << _frame_col_range.second << "]"
						<< "\n";

					std::string next_prefixes{ prev_prefixes };
					next_prefixes.append(next_prefix_token);

					if (!this->child_is_endsite())
					{
						const auto& child_nodes = this->get_child_nodes();
						//assert(!child_nodes.empty());

						for (
							auto child_it = child_nodes.cbegin();
							child_it != child_nodes.cend();
							++child_it
							)
						{
							(*child_it)->_dump_childs_hierarchy_impl(
								out,
								next_prefixes,
								indent_level + 1,
								child_it == std::next(child_nodes.end(), -1)
							);
						}
					}
					else
					{
						const auto& child_endsite_t_offset = this->get_child_endsite_t_pose_offset();

						out << next_prefixes << kTrunkPrefixToken << "\n";
						out << next_prefixes << kLastBranchPrefixToken;
						out << "EndSite" << " ";

						// Add padding.
						{
							const int
								curr_x_pos = static_cast<int>(out.str().size())/* end pos */ - static_cast<int>(out.str().find_last_of('\n'))/* begin pos */,
								needed_paddings = kMaxLinePaddingSize - curr_x_pos;

							for (int n = 0; n < needed_paddings; ++n) {
								out << ((curr_x_pos + n) % 2 ? "." : " ");
							}
						}

						out << " "
							<< "T-Offset=["
							<< child_endsite_t_offset.x() << ","
							<< child_endsite_t_offset.y() << ","
							<< child_endsite_t_offset.z()
							<< "]\n";
					}
				}

			}; // class

            struct bvh_parser_context {
            private:
				// Parser token strings
				static constexpr std::string_view
					kHierarchy = "HIERARCHY",
					kMotion = "MOTION",
					kRoot = "ROOT",
					kJoint = "JOINT",
					kChannels = "CHANNELS",
					kOffset = "OFFSET",
					kEnd = "End",
					kSite = "Site",
					kFrames = "Frames",
					kFrame = "Frame",
					kTime = "Time",
					kXposition = "Xposition",
					kYposition = "Yposition",
					kZposition = "Zposition",
					kXrotation = "Xrotation",
					kYrotation = "Yrotation",
					kZrotation = "Zrotation";

				// Parser token chars
				static constexpr char
					kScopeBegin = '{',
					kScopeEnd = '}',
					kColon = ':',
					kLineSeperator = '\n';

			private:

				enum class bvh_tokenizer_policy_type {
					handle_line_seperators,
					ignore_line_seperators,
				};

				class bvh_tokenizer_method final
					: public string::tokenizer_methods::char_separator
				{
				private:
					using super = typename string::tokenizer_methods::char_separator;

				public:
					using typename super::char_type;
					using typename super::const_char_pointer;
					using typename super::string_type;
					using typename super::string_view_type;

				private:
					bvh_tokenizer_policy_type _policy{};

				public:
					bvh_tokenizer_method() noexcept
						: super(
							/* empty token policy */string::empty_token_policy_type::drop_empty_tokens,
							/* drop delims */{ ' ', '\t', '\r' },
							/* keep delims */{ kColon, kLineSeperator }
						)
					{
						this->set_policy(bvh_tokenizer_policy_type::handle_line_seperators);
					}

					void set_policy(bvh_tokenizer_policy_type new_policy)
					{
						_policy = new_policy;
						switch (_policy) {
						case bvh_tokenizer_policy_type::handle_line_seperators:
							this->drop_delims().erase(kLineSeperator);
							this->keep_delims().insert(kLineSeperator);
							break;
						case bvh_tokenizer_policy_type::ignore_line_seperators:
							this->drop_delims().insert(kLineSeperator);
							this->keep_delims().erase(kLineSeperator);
							break;
						default:
							// TODO: throw error
							break;
						}
					}

				}; // class

            private:
                const std::string _raw_file_content;
				string::tokenizer<bvh_tokenizer_method> _tokenizer;
				string::tokenizer<bvh_tokenizer_method>::iterator _curr_token_it;

				size_t _num_joints{ 0 };
				size_t _num_frames{ 0 };
				size_t _num_frame_cols{ 0 };
				double _frame_time{ 0.0 };

				bvh_hierarchy_node_ptr _root_joint_node;
				std::vector<bvh_hierarchy_node_ptr> _ordered_joint_nodes;

				std::unordered_map<
					bvh_hierarchy_node_ptr/* child */, 
					bvh_hierarchy_node_ptr/* parent */
				> _joint_nodes_parent_map;

				std::unordered_map<
					std::string/* bvh node name */,
					bvh_hierarchy_node_ptr
				> _joint_nodes_name_map;

				Eigen::MatrixXd _raw_frames_data; // Shape: (_num_frame, _num_frame_cols)

			private:
				static inline std::optional<bvh_channel_type> try_parse_bvh_channel_type(std::string_view type_sv) noexcept {
					if (type_sv == kXposition) { return bvh_channel_type::x_position; }
					if (type_sv == kYposition) { return bvh_channel_type::y_position; }
					if (type_sv == kZposition) { return bvh_channel_type::z_position; }
					if (type_sv == kXrotation) { return bvh_channel_type::x_rotation; }
					if (type_sv == kYrotation) { return bvh_channel_type::y_rotation; }
					if (type_sv == kZrotation) { return bvh_channel_type::z_rotation; }
					return std::nullopt;
				}

				template <typename _Ty>
				static inline std::optional<size_t> index_of(const std::vector<_Ty>& v, const _Ty& target) {
					const auto it = std::find(v.begin(), v.end(), target);
					return (it != v.end()) ? static_cast<int32_t>(it - v.begin()) : std::nullopt;
				}

            public:
				bvh_parser_context(
                    std::string raw_file_content,
					[[maybe_unused]] double scale_factor = 1.0)
                    : _raw_file_content{ std::move(raw_file_content) }
                    , _tokenizer{ bvh_tokenizer_method{}, _raw_file_content }
                    , _curr_token_it{ _tokenizer.begin() }
                {
                    this->_parse_hierarchy_section();
                    this->_parse_motion_section();
                }

				size_t num_joints() const noexcept {
					return _ordered_joint_nodes.size();
				}

				size_t num_frames() const noexcept {
					return _raw_frames_data.rows();
				}

				size_t num_frame_cols() const noexcept {
					return _raw_frames_data.cols();
				}

				double frame_time() const noexcept {
					return _frame_time;
				}

				bvh_hierarchy_node_ptr root_joint_node() const noexcept {
					return _root_joint_node;
				}

				const auto& ordered_joint_nodes() const noexcept {
					return _ordered_joint_nodes;
				}

				const auto& joint_nodes_name_map() const noexcept {
					return _joint_nodes_name_map;
				}

				const auto& joint_nodes_parent_map() const noexcept {
					return _joint_nodes_parent_map;
				}

				const auto& raw_frames_data() const noexcept {
					return _raw_frames_data;
				}

				std::vector<Eigen::Vector3d> extract_root_joint_positions() const
				{
					const auto channel_types = _root_joint_node->get_channel_types();
					const auto [start_col, end_col] = _root_joint_node->get_frame_col_range();

					size_t
						x_pos_col = std::string::npos,
						y_pos_col = std::string::npos,
						z_pos_col = std::string::npos;

					for (size_t i = 0; i < channel_types.size(); ++i) {
						switch (channel_types[i]) {
						case bvh_channel_type::x_position: x_pos_col = start_col + i; break;
						case bvh_channel_type::y_position: y_pos_col = start_col + i; break;
						case bvh_channel_type::z_position: z_pos_col = start_col + i; break;
						default: break;
						}
					}

					TRIENGINE_ASSERT(x_pos_col != std::string::npos);
					TRIENGINE_ASSERT(y_pos_col != std::string::npos);
					TRIENGINE_ASSERT(z_pos_col != std::string::npos);

					std::vector<Eigen::Vector3d> positions;
					positions.reserve(this->num_frames());
					for (size_t row = 0; row < this->num_frames(); ++row) {
						positions.emplace_back(
							_raw_frames_data(row, x_pos_col),
							_raw_frames_data(row, y_pos_col),
							_raw_frames_data(row, z_pos_col)
						);
					}

					return positions;
				}

				std::pair<std::vector<Eigen::Vector3d>, std::string> extract_joint_euler_angles(
					bvh_hierarchy_node_ptr joint_node) const
				{
					const auto channel_types = joint_node->get_channel_types();
					const auto [start_col, end_col] = joint_node->get_frame_col_range();

					std::array<size_t, 3> euler_angle_cols{ std::string::npos, };
					std::string euler_axis_order;
					for (size_t i = 0; i < channel_types.size(); ++i) {
						switch (channel_types[i]) {
						case bvh_channel_type::x_rotation:
							euler_angle_cols.at(euler_axis_order.size()) = start_col + i; // x_rot_col
							euler_axis_order.push_back('X');
							break;
						case bvh_channel_type::y_rotation:
							euler_angle_cols.at(euler_axis_order.size()) = start_col + i; // y_rot_col
							euler_axis_order.push_back('Y');
							break;
						case bvh_channel_type::z_rotation:
							euler_angle_cols.at(euler_axis_order.size()) = start_col + i; // z_rot_col
							euler_axis_order.push_back('Z'); 
							break;
						default:
							break;
						}
					}

					TRIENGINE_ASSERT(euler_axis_order.size() == 3);

					std::vector<Eigen::Vector3d> euler_angles;
					euler_angles.reserve(this->num_frames());
					for (size_t row = 0; row < this->num_frames(); ++row) {
						euler_angles.emplace_back(
							_raw_frames_data(row, euler_angle_cols[0]),
							_raw_frames_data(row, euler_angle_cols[1]),
							_raw_frames_data(row, euler_angle_cols[2])
						);
					}

					return { euler_angles, euler_axis_order };
				}

				std::string dump() const
				{
					std::stringstream out;
					out << std::fixed << std::setprecision(6);
					out << "Num Frames: " << _num_frames << "\n";
					out << "Frame Time: " << _frame_time << "\n";
					out << "Joints Parent Map:\n";
					for (const auto& [child, parent] : _joint_nodes_parent_map) {
						out << "  " << "\"" << child->get_joint_name() << "\" -> \"" << parent->get_joint_name() << "\"\n";
					}
					out << "Joints Hierarchy:\n";
					out << _root_joint_node->dump_childs_hierarchy();
					return out.str();
				}

            private:
				void _parse_hierarchy_section()
				{
					_tokenizer.method().set_policy(bvh_tokenizer_policy_type::ignore_line_seperators);

					this->_expect_current_token(kHierarchy, true);

					const auto parse_hierarchy_section_node_impl =
						[this](auto&& fn_self, bvh_hierarchy_node_ptr parent_node) -> bvh_hierarchy_node_ptr
						{
							const bool is_root_node = parent_node == nullptr;
							this->_expect_current_token(is_root_node ? kRoot : kJoint, true);

							const std::string_view node_name = this->_get_current_token(true);

							this->_expect_current_token(kScopeBegin, true);
							this->_expect_current_token(kOffset, true);

							const Eigen::Vector3d node_offset{
								this->_get_current_token_scalar<double>(true),
								this->_get_current_token_scalar<double>(true),
								this->_get_current_token_scalar<double>(true)
							};

							this->_expect_current_token(kChannels, true);

							std::vector<bvh_channel_type> node_channel_types;
							node_channel_types.resize(this->_get_current_token_scalar<size_t>(true));
							for (size_t i = 0; i < node_channel_types.size(); ++i) {
								const auto channel_type = try_parse_bvh_channel_type(this->_get_current_token(true));
								if (!channel_type) { throw std::runtime_error{ "Failed to parse BVH channel type" }; }
								node_channel_types[i] = channel_type.value();
							}

							if (is_root_node) {
								// TODO: validate node channel types...
							} else {
								// TODO: validate node channel types...
							}

							auto curr_node = this->_allocate_bvh_hierarchy_node(
								is_root_node ? bvh_hierarchy_node_type::root : bvh_hierarchy_node_type::child,
								node_name,
								node_offset,
								node_channel_types
							);

							if (parent_node) {
								parent_node->add_child_node(curr_node);
							}

							if (this->_get_current_token() == kJoint)
							{
								// current node has child node(s)
								do {
									fn_self(fn_self, curr_node); // parse nodes recursive
								} while (this->_get_current_token() != std::string_view{ &kScopeEnd, 1 });
							}
							else
							{
								// current node has end site as child
								this->_expect_current_token(kEnd, true);
								this->_expect_current_token(kSite, true);
								this->_expect_current_token(kScopeBegin, true);
								this->_expect_current_token(kOffset, true);

								const Eigen::Vector3d endsite_offset{
									this->_get_current_token_scalar<double>(true),
									this->_get_current_token_scalar<double>(true),
									this->_get_current_token_scalar<double>(true)
								};
								curr_node->mark_child_as_endsite(endsite_offset);

								this->_expect_current_token(kScopeEnd, true);
							}

							this->_expect_current_token(kScopeEnd, true);
							return curr_node;
						};

					const auto parse_hierarchy_secion_node =
						[&](bvh_hierarchy_node_ptr parent_node = nullptr) -> bvh_hierarchy_node_ptr {
							return parse_hierarchy_section_node_impl(
								parse_hierarchy_section_node_impl, 
								parent_node
							);
						};

					_root_joint_node = parse_hierarchy_secion_node();
					_ordered_joint_nodes = _root_joint_node->get_ordered_childs_list();
					_joint_nodes_parent_map = _root_joint_node->get_childs_parent_map();
				}

				void _parse_motion_section()
				{
					_tokenizer.method().set_policy(bvh_tokenizer_policy_type::handle_line_seperators);

					this->_expect_current_token(kMotion, true);
					this->_expect_current_token(kLineSeperator, true);

					bool 
						parsed_num_frames{ false }, 
						parsed_frame_time{ false };

					while (!(
						parsed_num_frames &&
						parsed_frame_time
						))
					{
						const std::string_view first_key_part{ this->_get_current_token(true) };

						if (first_key_part == kFrames)
						{
							this->_expect_current_token(kColon, true);
							_num_frames = this->_get_current_token_scalar<size_t>(true);
							parsed_num_frames = true;
							TRIENGINE_TRACE("num_frames: %zu", _num_frames);
						}
						else if (first_key_part == kFrame)
						{
							this->_expect_current_token(kTime, true); // second key part
							this->_expect_current_token(kColon, true);
							_frame_time = this->_get_current_token_scalar<double>(true);
							parsed_frame_time = true;
							TRIENGINE_TRACE("frame_time: %f", _frame_time);
						}

						this->_expect_current_token(kLineSeperator, true);
					} // while

					_raw_frames_data = Eigen::MatrixXd::Zero(_num_frames, _num_frame_cols);
					for (Eigen::Index frame_row = 0; frame_row < _raw_frames_data.rows(); ++frame_row) {
						for (Eigen::Index frame_col = 0; frame_col < _raw_frames_data.cols(); ++frame_col) {
							_raw_frames_data(frame_row, frame_col) = this->_get_current_token_scalar<double>(true);
						}
						this->_expect_current_token(kLineSeperator, true);
					}
				}

			private:
                std::string_view _get_current_token(
					bool move_to_next = false)
				{
					if (_curr_token_it == _tokenizer.end()) {
						throw std::runtime_error{ "Unexpected eof" };
					}

                    const std::string_view token = *_curr_token_it;
					if (move_to_next) {
						//TRIENGINE_TRACE("read next token: \"%s\""
						//	, (token != std::string_view{ &kLineSeperator, 1 }) ? std::string{ token }.c_str() : "\\n"
						//);
						++_curr_token_it;

						// TODO: handle sequenced line seperators...
					}

                    return token;
                }

				template <typename _Ty>
				_Ty _get_current_token_scalar(
					bool move_to_next = false)
				{
					const std::string_view token = this->_get_current_token(move_to_next);
					_Ty token_scalar;

					const auto [_, ec] = std::from_chars(
						token.data(),
						token.data() + token.size(),
						token_scalar
					);

					if (ec != std::errc{}) {
						throw std::runtime_error{ utility::string::c_format(
							"Unexpected scalar token \"%.*s\" (ec: %d)"
							, static_cast<int>(token.size())
							, token.data()
							, ec
						) };
					}

					return token_scalar;
				}

				void _expect_current_token(
					const std::string_view expected_sv, 
					const bool move_to_next = false)
				{
					if (const std::string_view token = this->_get_current_token(move_to_next); 
						token != expected_sv) {
						throw std::runtime_error{ utility::string::c_format(
							"Unexpected token (expect \"%.*s\", got \"%.*s\")"
							, static_cast<int>(expected_sv.size())
							, expected_sv.data()
							, static_cast<int>(token.size())
							, token.data()
						) };
					}
				}

				void _expect_current_token(
					const char expected_ch,
					const bool move_to_next = false)
				{
					this->_expect_current_token(
						std::string_view{ &expected_ch, 1 },
						move_to_next
					);
				}

				bvh_hierarchy_node_ptr _allocate_bvh_hierarchy_node(
					const bvh_hierarchy_node_type node_type,
					const std::string_view node_name,
					const Eigen::Vector3d& t_pose_offset,
					const std::vector<bvh_channel_type>& node_channel_types)
				{
					auto new_bvh_node = std::make_shared<bvh_hierarchy_node>(
						node_type,
						node_name,
						t_pose_offset,
						node_channel_types,
						_num_frames/* bvh node declaration order */,
						_num_frame_cols/* bvh node frame col start offset */
					);

					if (const auto [_, success] = _joint_nodes_name_map.insert({ std::string{ node_name }, new_bvh_node });
						!success)
					{
						throw std::runtime_error{ "Duplicate BVH node found" };
					}

					// update for next allocation
					++_num_frames; // next declaration order
					_num_frame_cols = new_bvh_node->get_frame_col_range().second; // next start offset

					return new_bvh_node;
				}

            }; // class

        private:
			std::unique_ptr<bvh_parser_context> _parser_ctx;
			double _scale_factor{ 1.0 }; // NOTE: BVH use centimeter by default

			using joints_euler_angles_map_t = std::unordered_map<bvh_hierarchy_node_ptr, std::vector<Eigen::Vector3d>>;
			using joints_euler_axis_order_map_t = std::unordered_map<bvh_hierarchy_node_ptr, std::string>;
			using joints_world_rotation_map_t = std::unordered_map<bvh_hierarchy_node_ptr, std::vector<Eigen::Quaterniond>>;
			using joints_world_position_map_t = std::unordered_map<bvh_hierarchy_node_ptr, std::vector<Eigen::Vector3d>>;

        public:
            bvh_file_parser() = default;

            bvh_file_t load(const std::filesystem::path& bvh_file_path)
            {
                const std::filesystem::path bvh_file_abs_path{ std::filesystem::canonical(bvh_file_path) };
                TRIENGINE_DEBUG("Load bvh file: %s", bvh_file_abs_path.string().c_str());

                std::string bvh_file_content; {
                    std::ifstream f{ bvh_file_abs_path, std::ios::in };
                    bvh_file_content = std::string{ // https://en.cppreference.com/w/cpp/iterator/istreambuf_iterator
                        std::istreambuf_iterator<char>{ f },
                        std::istreambuf_iterator<char>{}/* end iterator */
                    };
                }

                _parser_ctx = std::make_unique<bvh_parser_context>(bvh_file_content);
				TRIENGINE_TRACE("Parser dump info:\n%s", _parser_ctx->dump().c_str());

				// Eigen issue: https://gitlab.com/libeigen/eigen/-/issues/1806

				joints_euler_angles_map_t joints_euler_angles_map;
				joints_euler_axis_order_map_t joints_euler_axis_order_map;
				for (const auto& node_ptr : _parser_ctx->ordered_joint_nodes()) {
					auto [
						euler_angles, 
						euler_axis_order
					] = _parser_ctx->extract_joint_euler_angles(node_ptr);
					joints_euler_angles_map[node_ptr] = std::move(euler_angles);
					joints_euler_axis_order_map[node_ptr] = std::move(euler_axis_order);
				}

				const auto [
					joints_world_rotations_map, 
					joints_world_positions_map
				] = this->_calculate_joints_world_transforms(
					joints_euler_angles_map, 
					joints_euler_axis_order_map
				);

				bvh_file_t result;

				// fill joints info
				std::unordered_map<bvh_hierarchy_node_ptr, bvh_joint_id_t> bvh_jid_map;
				result.joints.reserve(_parser_ctx->num_joints());
				for (const auto& joint_node : _parser_ctx->ordered_joint_nodes()) {
					bvh_joint_info_t new_bvh_jinfo{};
					new_bvh_jinfo.name = joint_node->get_joint_name();
					new_bvh_jinfo.length = math::vec3_distance(Eigen::Vector3d::Zero().eval(), joint_node->get_t_pose_offset());
					new_bvh_jinfo.bvh_euler_axis_order = joints_euler_axis_order_map.at(joint_node);
					bvh_jid_map[joint_node] = static_cast<bvh_joint_id_t>(result.joints.size());
					result.joints.emplace_back(new_bvh_jinfo);
				}

				// fill joints parent map
				for (const auto& [
					child, 
					parent
					] : _parser_ctx->joint_nodes_parent_map())
				{
					result.joints_parent_map[bvh_jid_map.at(child)] = bvh_jid_map.at(parent);
				}

				// fill frames
				result.frames.reserve(_parser_ctx->num_frames());
				for (size_t frame_idx = 0; frame_idx < _parser_ctx->num_frames(); ++frame_idx) {
					bvh_motion_frame_t new_bvh_frame;
					for (const auto& [joint_node, bvh_jid] : bvh_jid_map) {
						bvh_joint_data_t& new_bvh_jdata = new_bvh_frame.skeleton[bvh_jid];
						new_bvh_jdata.joint_id = bvh_jid;
						new_bvh_jdata.bvh_euler_angels = joints_euler_angles_map.at(joint_node)[frame_idx];
						new_bvh_jdata.world_rotation = joints_world_rotations_map.at(joint_node)[frame_idx];
						new_bvh_jdata.world_position = joints_world_positions_map.at(joint_node)[frame_idx];
					}
					result.frames.emplace_back(new_bvh_frame);
				}

				// TODO: fill T-pose frame
				// ...
				
				// fill reset informations
				result.root_joint_id = bvh_jid_map.at(_parser_ctx->root_joint_node());
				result.frame_time = _parser_ctx->frame_time();

				{
					std::stringstream out;
					out << std::fixed << std::setprecision(6);

					out << "----- Joints -----\n";
					for (size_t jid = 0; jid < result.joints.size(); ++jid) {
						const auto& jinfo = result.joints[jid];
						out << "[#" << jid << "] name: " << jinfo.name << ", length: " << jinfo.length << "\n";
					}

					out << "\n";
					out << "----- Parent Map -----\n";
					for (const auto [child_jid, parent_jid] : result.joints_parent_map) {
						out << result.joints[child_jid].name << " -> " << result.joints[parent_jid].name << "\n";
					}

					out << "\n";
					out << "Root Joint ID: " << result.root_joint_id << "\n";
					out << "Frame Time: " << result.frame_time << "\n";
					out << "Num Frames: " << result.frames.size() << "\n";

					TRIENGINE_TRACE("dump:\n%s", out.str().c_str());
				}

				return result;
            }

        private:

			std::pair<joints_world_rotation_map_t, joints_world_position_map_t> _calculate_joints_world_transforms(
				const joints_euler_angles_map_t& joints_euler_angles_map,
				const joints_euler_axis_order_map_t& joints_euler_axis_order_map)
			{
				joints_world_rotation_map_t joints_world_rotation_map;
				joints_world_position_map_t joints_world_position_map;

				// pre-allocate
				constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
				for (const auto& joint_node : _parser_ctx->ordered_joint_nodes()) {
					joints_world_rotation_map[joint_node].resize(_parser_ctx->num_frames(), Eigen::Quaterniond{ NaN, NaN, NaN, NaN });
					joints_world_position_map[joint_node].resize(_parser_ctx->num_frames(), Eigen::Vector3d{ NaN, NaN, NaN });
				}

				const auto& joints_parent_map = _parser_ctx->joint_nodes_parent_map();
				const auto root_joint_positions = _parser_ctx->extract_root_joint_positions();

				for (size_t frame_idx = 0; frame_idx < _parser_ctx->num_frames(); ++frame_idx)
				{
					// Iterate joints in the order defined in the HIERARCHY section
					// This order is usually so that the parent comes before the child, 
					// so the child can be processed while the parent's world transformation has been computed.
					for (const auto& curr_joint : _parser_ctx->ordered_joint_nodes())
					{
						// Compute local rotation of current frame (Euler angles -> Quaternion)
						const Eigen::Quaterniond local_rotation = math::quat_from_euler(
							joints_euler_angles_map.at(curr_joint)[frame_idx].unaryExpr(&math::deg2rad<double>).eval(),
							joints_euler_axis_order_map.at(curr_joint)
						);

						Eigen::Quaterniond& world_rotation = joints_world_rotation_map[curr_joint][frame_idx];
						Eigen::Vector3d& world_position = joints_world_position_map[curr_joint][frame_idx];

						if (curr_joint->get_node_type() == bvh_hierarchy_node_type::root) // root joint?
						{
							world_rotation = local_rotation; // The world rotation of the root joint is equal to its local rotation.
							world_position = root_joint_positions[frame_idx];
						}
						else // child joint
						{
							const auto& parent_joint = joints_parent_map.at(curr_joint);
							const Eigen::Quaterniond& parent_world_rotation = joints_world_rotation_map.at(parent_joint)[frame_idx];
							const Eigen::Vector3d& parent_world_position = joints_world_position_map.at(parent_joint)[frame_idx];

							TRIENGINE_ASSERT(!parent_world_rotation.coeffs().array().isNaN().any());
							TRIENGINE_ASSERT(!parent_world_position.array().isNaN().any());

							// Calculate current world rotation (rotation apply order: parent -> local)
							world_rotation = math::quat_combine(parent_world_rotation, local_rotation).normalized();

							// Calculate world position: Parent world position + (Parent world rotation * T-pose offset of current joint)
							// Since T-pose offset is relative to the parent coordinate system, 
							// apply the parent's world rotation to convert it to a world coordinate system vector and then add it.
							world_position = parent_world_position + (parent_world_rotation * curr_joint->get_t_pose_offset());
						}

					} // for each joint nodes
				} // for each frames

				return { joints_world_rotation_map, joints_world_position_map };
			}

        }; // class

    } // namespace

    bool load_skeleton_from_bvh(
        const std::filesystem::path& file_path,
		bvh_file_t& bvh_file)
    {
        bvh_file_parser parser;
        bvh_file = parser.load(file_path);
        TRIENGINE_DEBUG("Load complete!");
        return true;
    }

} // namespace