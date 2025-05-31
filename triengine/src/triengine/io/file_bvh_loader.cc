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

/*
## BVH 모션 계산 과정

BVH 파일에서 각 관절의 최종 월드 변환은 부모 관절의 변환에 자신의 로컬 변환(오프셋 + 애니메이션된 회전/이동)을 순차적으로 곱해나가는 방식으로 계산된다.
관절 `J`의 월드 변환 행렬 $M_{world}^J$ 는 다음과 같이 표현될 수 있다:

1.  Root Joint:
	$M_{world}^{Root} = T_{anim}^{Root} \times M_{offset}^{Root} \times R_{anim}^{Root}$
	> $T_{anim}^{Root}$: 루트 관절의 애니메이션된 이동 변환 (주로 X, Y, Z Position 채널 데이터로부터 계산)
	> $M_{offset}^{Root}$: 루트 관절의 정적 오프셋 변환 (HIERARCHY의 OFFSET 값으로부터 계산, 일반적으로 루트는 (0,0,0)이지만 아닐 수도 있음)
	> $R_{anim}^{Root}$: 루트 관절의 애니메이션된 회전 변환 (X, Y, Z Rotation 채널 데이터로부터 계산, 채널 순서에 따름)

2.  Child Joint:
	$M_{world}^J = M_{world}^{Parent(J)} \times M_{offset}^J \times R_{anim}^J$
	> $M_{world}^{Parent(J)}$: 부모 관절의 월드 변환 행렬.
	> $M_{offset}^J$: 현재 관절 `J`의 부모 관절로부터의 정적 오프셋 변환.
	> $R_{anim}^J$: 현재 관절 `J`의 애니메이션된 회전 변환. (일반적으로 자식 관절은 위치 채널을 갖지 않으므로 $T_{anim}^J$는 단위 행렬로 간주된다. 
	>               만약 자식 관절도 위치 채널을 갖는다면 $M_{offset}^J \times T_{anim}^J \times R_{anim}^J$ 순서로 곱해질 수 있지만, 
	>               일반적으로는 자식 관절은 위치 채널을 갖지 않고 회전 채널만 갖는다.

> NOTE: 행렬 곱셈은 오른쪽에서 왼쪽 순서로 적용된다. (예를 들어 $A \times B \times C$ 라면, $C$ 변환 후 $B$ 변환 후 $A$ 변환이 적용됨)


## BVH에서 조인트의 HIREARCHY(루트/자식 관절)과 T-Pose(기본 자세 정의) 간의 상관관계

BVH 파일에서 루트 관절 또는 자식 관절의 회전값(MOTION 섹션)과 HIERARCHY 섹션에 정의된 T-Pose(또는 Bind Pose, Rest Pose)는 다음과 같은 상관관계를 갖는다:

### 1. HIERARCHY 섹션과 T-Pose
	- HIERARCHY 섹션의 OFFSET: 
	  이 값들은 각 관절의 기본 자세(T-Pose)에서의 상대적인 위치와 방향을 결정한다.
	- 루트(ROOT) 관절의 OFFSET: 
	  월드 좌표계 원점 (0,0,0)을 기준으로 루트 관절의 T-Pose 시 위치를 정의한다. 대부분 (0,0,0)이지만, 캐릭터 전체가 특정 위치에서 시작하도록 오프셋을 가질 수도 있다.
	- 자식(JOINT) 관절의 OFFSET: 
	  부모 관절의 피봇(pivot) 지점을 기준으로 해당 자식 관절의 T-Pose 시 위치를 정의한다.
	  이 오프셋들이 연결되어 팔다리 등의 골격 형태를 이룬다. 예를 들어, 어깨 관절의 오프셋은 가슴 관절로부터 팔이 시작되는 위치, 
	  팔꿈치 관절의 오프셋은 어깨 관절로부터 팔꿈치까지의 팔뚝 방향과 길이를 결정한다.
	- T-Pose에서의 회전: 
	  HIERARCHY 섹션만으로는 각 관절의 "초기 회전"을 직접적으로 명시하지 않는다.
	  OFFSET을 통해 부모로부터 자식으로 이어지는 뼈대(bone)의 방향이 결정될 뿐, 이 T-Pose 상태에서는 MOTION 섹션의 모든 회전 채널 값은 0이라고 가정한다.
	  즉, T-Pose는 애니메이션 회전이 전혀 적용되지 않은, 순수하게 OFFSET 정보만으로 골격이 배치된 자세를 의미한다.
	  관절의 로컬 좌표계는 이 T-Pose에서의 뼈대 방향을 기준으로 설정된다.

### 2. MOTION 섹션의 회전값 (T-Pose로부터의 변화량)
	- MOTION 섹션의 각 프레임별 회전 채널 값들(예: `Xrotation`, `Yrotation`, `Zrotation`)은 해당 관절을 "T-Pose 상태(OFFSET에 의해 결정된 기본 방향)로부터 얼마나 회전시킬 것인가"를 나타낸다.
	- 이 회전값들은 "로컬" 회전값이다. (즉, 해당 관절 자체의 로컬 좌표계를 기준으로 회전함)
	- 예를 들어, 팔꿈치 관절이 T-Pose에서 X축 방향으로 뻗어있다고 가정했을 때, `Zrotation` 채널에 90도 값이 들어오면, 
	  팔꿈치는 자신의 로컬 Z축을 기준으로 90도 회전하여 팔이 접히는 동작을 하게 된다. 이 회전은 T-Pose에서 X축으로 뻗어있던 그 상태를 기준으로 적용된다.

### 3. 상관관계 및 계산 과정에서의 역할

	$M_{world}^J = M_{world}^{Parent(J)} \times M_{offset}^J \times R_{anim}^J$

	- $M_{offset}^J$ (정적 오프셋 행렬, `offmat`에 해당):
	  HIERARCHY 섹션의 `OFFSET` 값으로 만들어지며, 
	  T-Pose에서 부모 관절에 대한 현재 관절의 상대적인 위치와 기본 방향을 설정한다. 이것이 "T-Pose의 기여분"이 된다.

	- $R_{anim}^J$ (애니메이션된 로컬 회전 행렬, `rmat`에 해당):
	  MOTION 섹션의 회전 채널 값으로 만들어지며, $M_{offset}^J$에 의해 설정된 "T-Pose의 기본 방향으로부터 추가적인 회전"을 가한다.

따라서, MOTION 섹션의 회전값은 항상 HIERARCHY 섹션에 정의된 T-Pose 골격 구조를 "기준" 또는 "출발점"으로 하여 적용되는 변화량이라고 이해할 수 있다.
T-Pose가 "0도 회전" 상태를 의미하며, 모션 데이터는 이 0도 상태에서 얼마나 더 회전할지를 지시하는 것아다.
결론적으로, HIERARCHY 섹션의 `OFFSET`은 캐릭터의 정적인 기본 뼈대 구조(T-Pose)를 정의하고,
MOTION 섹션의 회전값들은 이 "기본 뼈대 구조 위에서 각 관절이 동적으로 어떻게 움직이는지를 T-Pose 기준으로 정의"하는 동적인 2차원 배열 데이터이다.
그리고 이 두 정보를 적절히 조합해서 완전한 애니메이션 자세가 만들어진다.


## BVH 채널의 일반적인 구성

일반적으로 루트 조인트는 6개의 채널을, 자식 조인트들은 3개의 회전 채널만을 갖는 경우가 많다.
BVH 파일의 HIERARCHY 섹션에서 각 `ROOT` 또는 `JOINT`는 `CHANNELS` 키워드 뒤에 채널의 개수와 그 종류를 명시한다.

채널의 종류는 다음 6가지가 존재한다:
- `Xposition`, `Yposition`, `Zposition`: 해당 조인트의 로컬 X, Y, Z축 방향으로의 이동(translation)값.
- `Xrotation`, `Yrotation`, `Zrotation`: 해당 조인트의 로컬 X, Y, Z축을 기준으로 하는 회전(rotation)값. (단위는 degree)

회전의 경우, 적용 순서는 `CHANNELS`에 명시된 순서를 따른다.
예를 들어 `CHANNELS ... Zrotation Xrotation Yrotation`이라면 Z축, 그 다음 X축, 마지막으로 Y축 순서로 회전이 적용된다.

### 1. Root Joint

일반적인 채널 구성: 6 채널
	- `Xposition`, `Yposition`, `Zposition` (3개의 이동 채널)
	- `Zrotation`, `Xrotation`, `Yrotation` (3개의 회전 채널 - 순서는 파일마다 다를 수 있음)
이유:
	- 루트 조인트는 전체 캐릭터 또는 골격 계층의 "월드 공간에서의 절대적인 위치와 방향"을 결정한다.
	- 따라서 3개의 이동 채널은 루트 조인트의 월드 좌표 (x, y, z)를 나타내고, 3개의 회전 채널은 루트 조인트의 월드 공간에서의 전체적인 방향(orientation)을 나타낸다.
	- 캐릭터가 씬 안에서 움직이고 회전하는 모든 정보는 이 루트 조인트의 6개 채널을 통해 표현된다.

### 2. Child Joint

일반적인 채널 구성: 3 채널
	- `Zrotation`, `Xrotation`, `Yrotation` (3개의 회전 채널 - 순서는 파일마다 다를 수 있음)
이유:
	- 자식 조인트의 상대적인 위치는 HIERARCHY 섹션의 `OFFSET` 값에 의해 이미 "고정"되어 있다. 이 `OFFSET`은 부모 조인트로부터 해당 자식 조인트까지의 '뼈(bone)'의 길이와 기본 방향을 정의한다.
	- 일반적인 골격 애니메이션에서 팔다리의 길이나 관절의 위치는 프레임이 바뀌더라도 변하지 않는다(캐릭터가 스트레칭되거나 변형되지 않는 한). 
	  대신, 관절은 그 고정된 피봇(pivot) 지점을 중심으로 회전한다.
	- 따라서 자식 조인트는 주로 3개의 회전 채널만을 사용하여 부모에 대한 상대적인 회전(관절의 굽힘, 폄, 비틀림 등)을 표현한다.
	- 이 로컬 회전값과 `OFFSET` 값을 통해 부모로부터의 상대적인 변환이 결정되고, 이것이 루트까지 누적되어 최종 월드 좌표가 계산된다.

## 자식 조인트가 Position 채널을 갖는 경우?

BVH 포맷 자체는 자식 조인트가 Position 채널을 가지는 것을 금지하지는 않는다. 따라서, (매우 드물긴 하지만) 이는 기술적으로 가능한 케이스이다.
(`CHANNELS` 정의는 각 조인트마다 독립적으로 이루어지므로, 자식 조인트에도 `Xposition`, `Yposition`, `Zposition` 채널을 포함시킬 수 있기 때문)
그러나 일반적인 인체 골격 애니메이션에서는 자식 조인트가 위치 채널을 갖는 경우는 드문데, 그 이유는 다음과 같다:

1.  골격 길이의 일관성 유지:
    자식 조인트의 위치가 `OFFSET` 외에 추가적으로 애니메이션되면, '뼈'의 길이가 늘어나거나 줄어들거나, 관절이 부모로부터 분리되는 등 비현실적인 움직임이 발생할 수 있다.
	이는 일반적인 모션 캡처 데이터의 표현 방식이 아니다.
2.  데이터의 중복성 및 해석 복잡도 증가 방지:
    `OFFSET`으로 이미 상대 위치가 정의되는데, 여기에 추가적인 위치 애니메이션을 넣으면 제어가 더 복잡해지고 데이터 해석이 어려워질 수 있다.


자식 조인트가 Position 채널을 가질 수 있는 특수한 상황들을 몇 가지 추려보면 다음과 같다:

1. 특수 효과 (VFX): 
   캐릭터의 팔이 고무처럼 늘어난다거나, 관절이 빠지는 등의 특수한 애니메이션 효과를 표현할 때 의도적으로 사용될 수 있다.
2. 비표준적인 릭(Rig) 또는 데이터: 
   특정 3D 모델링/애니메이션 소프트웨어가 매우 커스텀된 릭 구조를 가지고 있고, 이를 BVH로 익스포트할 때 자식 조인트에 위치 정보를 포함시키는 경우가 있을 수 있다. 
   (예시로, IK 핸들이나 보조 컨트롤러의 정보를 담기 위해라던지)
3. 도구(Tool) 또는 소품(Prop) 표현: 
   캐릭터의 손에 들린 무기나 도구가 손목 관절(자식 조인트)에 부착되어 있지만, 손목 안에서 미세하게 움직이거나 특정 효과를 위해 위치가 변해야 하는 경우, 
   해당 무기/도구를 나타내는 조인트에 위치 채널을 추가할 수 있다. (하지만 보통은 별도의 계층으로 처리하거나 다른 방식을 사용함)
4. 데이터 혹은 포맷 변환 오류 문제: 
   다른 포맷에서 BVH로 변환하는 과정에서 데이터가 잘못 매핑되어 자식 조인트에 불필요한 위치 채널이 포함될 가능성도 있다.

결론적으로, 일반적인 캐릭터 애니메이션 BVH 파일에서는 루트 조인트가 6채널(3 이동 + 3 회전)을 가지고 월드 공간에서의 전체적인 움직임을 담당하고, 
자식 조인트들은 3채널(3 회전)을 가지고 부모에 대한 상대적인 회전을 통해 관절의 움직임을 표현하는 것이 표준적인 방식이다.
자식 조인트가 위치 채널을 가질 수는 있지만, 이는 일반적인 경우가 아니며 특별한 의도나 상황에서 사용될 가능성이 높다고 볼 수 있겠다.
*/

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
                    this->_do_parse();
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
				void _do_parse()
				{
					//
					// Parse Hierarchy Section
					//

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

					//
					// Parse Motion Section
					//

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
				}

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
						_num_frames/* bvh node declaration order in hierarchy section */,
						_num_frame_cols/* bvh node motion frame column start offset */
					);

					if (const auto [_, success] = _joint_nodes_name_map.insert({ std::string{ node_name }, new_bvh_node });
						!success)
					{
						throw std::runtime_error{ "Duplicate BVH node found" };
					}

					// update for next allocation
					++_num_frames; // next declaration order in hierarchy section
					_num_frame_cols = new_bvh_node->get_frame_col_range().second; // next motion frame column start offset

					return new_bvh_node;
				}

            }; // class

        private:
			std::unique_ptr<bvh_parser_context> _parser_ctx;
			double _scale_factor{ 1.0 }; // NOTE: BVH use centimeter by default

			using joints_euler_angles_map_t = std::unordered_map<bvh_hierarchy_node_ptr, std::vector<Eigen::Vector3d>>;
			using joints_euler_axis_order_map_t = std::unordered_map<bvh_hierarchy_node_ptr, std::string>;
			using joints_world_rotation_map_t = std::unordered_map<bvh_hierarchy_node_ptr, std::vector<Eigen::Matrix3d>>;
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
				
				// fill rest informations
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

			/** Enumeration class for axis */
			enum class Axis {
				X,
				Y,
				Z
			};

			/** 
			 * Create rotation matrix (right-handed)
			 * @param  angle  The rotation angle in radian
			 * @param  axis   The rotation axis
			 * @return  The rotation matrix
			 */
			template <typename _Scalar>
			Eigen::Matrix4<_Scalar> mat4_rotation(_Scalar angle_rad, Axis axis) const
			{
				Eigen::Matrix4<_Scalar> M = Eigen::Matrix4<_Scalar>::Identity();

				constexpr _Scalar kEPS = std::numeric_limits<_Scalar>::epsilon();

				// Additional check logic to prevent `-0.0f`
				_Scalar s = std::sin(angle_rad);
				if (std::fabs(s) < kEPS) { s = static_cast<_Scalar>(0); }
				_Scalar c = std::cos(angle_rad);
				if (std::fabs(c) < kEPS) { c = static_cast<_Scalar>(0); }

				// `0.0f` if `s` is nearly zero, otherwise `-s`
				_Scalar ms = (std::fabs(s) < kEPS)
					? static_cast<_Scalar>(0)
					: static_cast<_Scalar>(-1) * s;

				if (axis == Axis::X)
				{
					M(1, 1) = c;
					M(1, 2) = ms;
					M(2, 1) = s;
					M(2, 2) = c;
				}
				else if (axis == Axis::Y)
				{
					M(0, 0) = c;
					M(0, 2) = s;
					M(2, 0) = ms;
					M(2, 2) = c;
				}
				else // Axis::Z
				{
					M(0, 0) = c;
					M(0, 1) = ms;
					M(1, 0) = s;
					M(1, 1) = c;
				}

				return M;
			}

			/** Rotates matrix
			 *  @param  matrix  The matrix to be rotated
			 *  @param  angle   The rotation angle
			 *  @param  axis    The rotation axis
			 *  @return  The rotation matrix
			 */
			template <typename _Scalar>
			Eigen::Matrix4<_Scalar> rotate(Eigen::Matrix4<_Scalar> M, _Scalar angle_rad, Axis axis) const {
				return M * mat4_rotation(angle_rad, axis);
			}

			auto _calculate_joints_world_transforms(
				const joints_euler_angles_map_t& joints_euler_angles_map,
				const joints_euler_axis_order_map_t& joints_euler_axis_order_map
			) const -> std::pair<joints_world_rotation_map_t, joints_world_position_map_t>
			{
				/*
				// recalculate bvh joint's local transformation matrix
				void Bvh::recalculate_joints_ltm(std::shared_ptr<Joint> start_joint)
				{
					// 시작 관절 설정 (재귀의 시작점 또는 현재 처리 대상)
					if (start_joint == NULL) { // 함수가 처음 호출될 때 start_joint가 NULL이면
						if (root_joint_ == NULL) return; // 루트 관절이 없으면 계산 불가
						else start_joint = root_joint_; // 루트 관절부터 시작
					}

					// 정적 오프셋 행렬 준비 (모든 프레임에 동일하게 적용)
					// M_offset^J 에 해당. 부모 관절 좌표계에서 현재 관절의 피봇 위치로 이동하는 변환.
					glm::mat4 offmat_backup = glm::translate(
						glm::mat4(1.0), // 단위 행렬에서 시작
						glm::vec3(start_joint->offset().x, start_joint->offset().y, start_joint->offset().z));

					// 현재 관절의 모든 프레임에 대한 모션 데이터 가져오기
					std::vector<std::vector<float>> data = start_joint->channel_data();

					// 각 프레임에 대해 반복
					for (int i = 0; i < num_frames_; i++) { // i는 현재 프레임 인덱스
						glm::mat4 offmat = offmat_backup; // 현재 프레임의 오프셋 행렬 (매번 동일)
						glm::mat4 rmat(1.0);  // 현재 프레임의 로컬 회전 행렬 (R_anim^J), 단위 행렬로 초기화
						glm::mat4 tmat(1.0);  // 현재 프레임의 로컬 이동 행렬 (T_anim^J, 주로 루트용), 단위 행렬로 초기화

						// 현재 프레임(i)의 채널 데이터를 기반으로 로컬 이동(tmat) 및 회전(rmat) 행렬 계산
						// BVH 파일의 CHANNELS 순서대로 값을 읽어와 적용
						for (int j = 0; j < start_joint->channels_order().size(); j++) { // j는 채널 인덱스
							// data[i][j]는 현재 프레임(i)의 j번째 채널 값
							if (start_joint->channels_order()[j] == Joint::Channel::XPOSITION)
								// tmat = tmat * translate(X,0,0)
								tmat = glm::translate(tmat, glm::vec3(data[i][j], 0, 0));
							else if (start_joint->channels_order()[j] == Joint::Channel::YPOSITION)
								tmat = glm::translate(tmat, glm::vec3(0, data[i][j], 0));
							else if (start_joint->channels_order()[j] == Joint::Channel::ZPOSITION)
								tmat = glm::translate(tmat, glm::vec3(0, 0, data[i][j]));
							else if (start_joint->channels_order()[j] == Joint::Channel::XROTATION)
								// rmat = rmat * rotateX(angle)
								rmat = utils::rotate(rmat, data[i][j], utils::Axis::X);
							else if (start_joint->channels_order()[j] == Joint::Channel::YROTATION)
								rmat = utils::rotate(rmat, data[i][j], utils::Axis::Y);
							else if (start_joint->channels_order()[j] == Joint::Channel::ZROTATION)
								rmat = utils::rotate(rmat, data[i][j], utils::Axis::Z);
						}
						// 이 루프가 끝나면 tmat은 T_anim^J, rmat은 R_anim^J 가 됩니다.
						// (CHANNELS 순서에 따라 회전이 누적됨, 예: ZYX 순서면 rmat = Rz * Rx * Ry 가 됨)

						// 월드 변환 행렬(ltm) 계산
						glm::mat4 ltm; // 최종적으로 M_world^J 가 될 행렬

						if (start_joint->parent() != NULL) { // 자식 관절인 경우
							// M_world^J = M_world^{Parent(J)} * M_offset^J
							// 부모의 월드 변환 행렬에 자신의 오프셋 변환을 곱한다.
							// 이 시점의 ltm은 현재 관절의 피봇이 월드 공간에서 어디에 위치하고 어떤 방향을 가지는지 나타낸다.
							// (아직 자신의 애니메이션된 회전 R_anim^J 는 적용되지 않음)
							ltm = start_joint->parent()->ltm(i) * offmat;
						} else { // 루트 관절인 경우
							// M_world^{Root} = T_anim^{Root} * M_offset^{Root}
							// 루트의 애니메이션된 이동과 정적 오프셋을 결합.
							// (아직 자신의 애니메이션된 회전 R_anim^{Root} 는 적용되지 않음)
							ltm = tmat * offmat;
						}

						// 현재 관절의 월드 위치 저장
						// ltm 행렬의 4번째 열(인덱스 3)이 이동(translation) 성분을 나타냄.
						// ltm[3]은 glm::vec4 타입이므로, glm::vec3으로 변환하여 저장.
						// 이 위치는 M_world^{Parent(J)} * M_offset^J (또는 T_anim^{Root} * M_offset^{Root})의 결과,
						// 즉, 현재 관절의 '피봇'의 월드 좌표.
						start_joint->set_pos(glm::vec3(ltm[3]));
						LOG(TRACE) << "Joint world position: " << utils::vec3tos(ltm[3]);

						// 최종 월드 변환 행렬 계산: 위에서 계산된 ltm에 자신의 애니메이션된 회전(rmat)을 적용
						// M_world^J = (M_world^{Parent(J)} * M_offset^J) * R_anim^J  (자식 관절)
						// 또는
						// M_world^{Root} = (T_anim^{Root} * M_offset^{Root}) * R_anim^{Root} (루트 관절)
						ltm = ltm * rmat;

						LOG(TRACE) << "Local transformation matrix: \n" << utils::mat4tos(ltm);

						// 계산된 최종 월드 변환 행렬을 현재 관절, 현재 프레임에 저장
						start_joint->set_ltm(ltm, i);
					}

					// 모든 자식 관절에 대해 재귀적으로 이 함수를 호출
					// 부모의 ltm(i)가 이미 계산되어 있어야 자식의 ltm(i)를 올바르게 계산할 수 있으므로,
					// 깊이 우선 탐색(DFS) 방식으로 계층 구조를 따라 내려가며 계산.
					for (auto& child : start_joint->children()) {
						recalculate_joints_ltm(child);
					}
				}
				*/

				joints_world_rotation_map_t joints_world_rotation_map;
				joints_world_position_map_t joints_world_position_map;

				// pre-allocate
				constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
				for (const auto& joint_node : _parser_ctx->ordered_joint_nodes()) {
					joints_world_rotation_map[joint_node].resize(_parser_ctx->num_frames(), Eigen::Matrix3d::Constant(NaN).eval());
					joints_world_position_map[joint_node].resize(_parser_ctx->num_frames(), Eigen::Vector3d::Constant(NaN).eval());
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
						const Eigen::Vector3d curr_local_euler_angles_rad{ joints_euler_angles_map.at(curr_joint)[frame_idx].unaryExpr(&math::deg2rad<double>).eval() };
						const std::string& curr_local_euler_axis_order{ joints_euler_axis_order_map.at(curr_joint) };
						const Eigen::Matrix4d curr_joint_offset_tm = math::translate_local(
							math::mat4_identity<double>(),
							curr_joint->get_t_pose_offset()
						);

						Eigen::Matrix4d local_rotation{ math::mat4_identity<double>() };
						for (size_t i = 0; i < curr_local_euler_axis_order.size(); ++i) {
							const double curr_axis_angle = curr_local_euler_angles_rad(i);
							const char curr_axis = curr_local_euler_axis_order[i];
							if (curr_axis == 'X') local_rotation = rotate(local_rotation, curr_axis_angle, Axis::X);
							else if (curr_axis == 'Y') local_rotation = rotate(local_rotation, curr_axis_angle, Axis::Y);
							else if (curr_axis == 'Z') local_rotation = rotate(local_rotation, curr_axis_angle, Axis::Z);
						}

						Eigen::Matrix3d& curr_world_rotation = joints_world_rotation_map[curr_joint][frame_idx];
						Eigen::Vector3d& curr_world_position = joints_world_position_map[curr_joint][frame_idx];

						if (curr_joint->get_node_type() == bvh_hierarchy_node_type::root) // root joint?
						{
							curr_world_rotation = local_rotation.block<3, 3>(0, 0); // The world rotation of the root joint is equal to its local rotation.
							curr_world_position = root_joint_positions[frame_idx];
						}
						else // child joint
						{
							const auto& parent_joint = joints_parent_map.at(curr_joint);
							const Eigen::Matrix3d& parent_world_rotation = joints_world_rotation_map.at(parent_joint)[frame_idx];
							const Eigen::Vector3d& parent_world_position = joints_world_position_map.at(parent_joint)[frame_idx];
							
							TRIENGINE_ASSERT(!parent_world_rotation.array().isNaN().any());
							TRIENGINE_ASSERT(!parent_world_position.array().isNaN().any());

							const Eigen::Matrix4d parent_world_rotation_tm = math::extend_to_mat4(parent_world_rotation);

							// Calculate current world rotation (rotation apply order: parent -> local)
							curr_world_rotation = (parent_world_rotation_tm * local_rotation).block<3, 3>(0, 0);

							// Calculate world position: Parent world position + (Parent world rotation * T-pose offset of current joint)
							// Since T-pose offset is relative to the parent coordinate system, 
							// apply the parent's world rotation to convert it to a world coordinate system vector and then add it.
							curr_world_position = parent_world_position + Eigen::Vector3d{ 
								(parent_world_rotation_tm * curr_joint_offset_tm).block<3, 1>(0, 3) 
							};
						}

						/*
						// Compute local rotation of current frame (Euler angles -> Quaternion)
						const Eigen::Quaterniond local_rotation = math::quat_from_euler(
							euler_angles_rad,
							euler_axis_order
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
						*/

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