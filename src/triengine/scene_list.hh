#pragma once
#include <triengine/scene.hh>

#include <list>
#include <memory>
#include <unordered_map>

namespace triengine
{
    // Scenes in insertion order with id lookup and a current-scene cursor.
    class scene_list
    {
    public:
        // Panics on a duplicate id. The first scene added becomes current.
        void add(std::shared_ptr<scene> new_scn);

        // Removing the current scene moves the cursor to the first remaining scene.
        void remove(scene_id_t scn_id);

        // Panics if `scn_id` is not in the list.
        void switch_to(scene_id_t scn_id);

        // Both wrap around at the ends.
        void switch_to_previous();
        void switch_to_next();

        // `nullptr` if not found / if the list is empty.
        std::shared_ptr<scene> find(scene_id_t scn_id) const;
        std::shared_ptr<scene> current() const;

    private:
        using scene_ptr = std::shared_ptr<scene>;

        std::list<scene_ptr> _scn_list;
        std::list<scene_ptr>::iterator _curr_scn_it{ _scn_list.end() };
        std::unordered_map<scene_id_t, std::list<scene_ptr>::iterator> _scn_id_map;
    };

} // namespace triengine
