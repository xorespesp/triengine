#include "scene_list.hh"
#include <triengine/utility/debug_utils.hh>

#include <iterator>
#include <utility>

namespace triengine
{
    void scene_list::add(std::shared_ptr<scene> new_scn)
    {
        const scene_id_t scn_id = new_scn->get_id();
        if (_scn_id_map.count(scn_id)) {
            TRIENGINE_PANIC("Failed to add scene (id #%X already exists)", scn_id);
        }

        const bool is_first{ _scn_list.empty() };

        _scn_list.push_back(std::move(new_scn));
        _scn_id_map[scn_id] = std::prev(_scn_list.end());

        if (is_first) {
            _curr_scn_it = std::prev(_scn_list.end());
        }
    }

    void scene_list::remove(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it != _scn_id_map.end()) {
            // erase invalidates iterator, so we must check it before the erase.
            const bool removing_curr_scn = (_curr_scn_it == map_it->second);
            _scn_list.erase(map_it->second);
            _scn_id_map.erase(map_it);
            if (removing_curr_scn) {
                _curr_scn_it = _scn_id_map.empty()
                    ? _scn_list.end()
                    : _scn_list.begin();
            }
        } else {
            TRIENGINE_WARN("Failed to remove scene #%X (not found)", scn_id);
        }
    }

    void scene_list::switch_to(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it == _scn_id_map.end()) {
            TRIENGINE_PANIC("Failed to change scene (invalid scene id #%X)", scn_id);
        }
        _curr_scn_it = map_it->second;
    }

    void scene_list::switch_to_previous()
    {
        if (_curr_scn_it != _scn_list.end()) {
            _curr_scn_it = std::prev((_curr_scn_it != _scn_list.begin())
                ? _curr_scn_it
                : _scn_list.end()
            );
        }
    }

    void scene_list::switch_to_next()
    {
        if (_curr_scn_it != _scn_list.end()) {
            const auto next_it = std::next(_curr_scn_it);
            _curr_scn_it = (next_it != _scn_list.end())
                ? next_it
                : _scn_list.begin();
        }
    }

    std::shared_ptr<scene> scene_list::find(scene_id_t scn_id) const
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<scene> scene_list::current() const
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

} // namespace triengine
