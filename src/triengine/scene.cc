#include "scene.hh"

#include <atomic>

namespace triengine
{
    namespace {
        scene_id_t _create_unique_scene_id() {
            static std::atomic<scene_id_t> cnt_ = 0;
            return cnt_++; // TODO: overflow check?
        }
    } // namespace
    
    scene::scene(std::shared_ptr<core::gpu_resource_manager> gpu_rsrc_mgr)
        : _id{ _create_unique_scene_id() }
        , _name{ utility::string::c_format("scene #%X", _id) }
        , _gpu_rsrc_mgr{ gpu_rsrc_mgr }
    {
        _camera_map.emplace(camera_type::arcball, std::make_unique<arcball_camera>());
        _camera_map.emplace(camera_type::fly, std::make_unique<fly_camera>());
        _camera_map.emplace(camera_type::ortho, std::make_unique<ortho_camera>());
        _camera_map.emplace(camera_type::pinhole, std::make_unique<pinhole_camera>());
        _active_camera_ptr = _camera_map.find(camera_type::arcball)->second.get();
    }

    void scene::add_geometry(std::shared_ptr<geometry::geometry_object_base> geometry_object)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_PANIC("Failed to access gpu resource manager");
        }

        switch (geometry_object->get_type()) {
        case geometry::geometry_object_type::lineset: {
            auto object = std::static_pointer_cast<geometry::lineset_object>(geometry_object);
            _lineset_geometries.push_back(std::static_pointer_cast<geometry::lineset_object>(object));
            gpu_rsrc_mgr->request_acquire_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            auto object = std::static_pointer_cast<geometry::pcd_object>(geometry_object);
            _pcd_geometries.push_back(object);
            gpu_rsrc_mgr->request_acquire_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::mesh: {
            auto object = std::static_pointer_cast<geometry::mesh_object>(geometry_object);
            _mesh_geometries.push_back(object);
            gpu_rsrc_mgr->request_acquire_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::skeleton: {
            auto object = std::static_pointer_cast<geometry::skeleton_object>(geometry_object);
            _skeleton_geometries.push_back(object);
            for (const auto& joint_obj : object->get_joint_objects()) {
                gpu_rsrc_mgr->request_acquire_geometry_resource(joint_obj);
            }
            for (const auto& bone_obj : object->get_bone_objects()) {
                gpu_rsrc_mgr->request_acquire_geometry_resource(bone_obj);
            }
            break;
        }
        }
    }

    void scene::remove_geometry(std::shared_ptr<geometry::geometry_object_base> geometry_object)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_PANIC("Failed to access gpu resource manager");
        }

        switch (geometry_object->get_type()) {
        case geometry::geometry_object_type::lineset: {
            auto object = std::static_pointer_cast<geometry::lineset_object>(geometry_object);
            object->mark_dirty();
            _lineset_geometries.remove(object);
            gpu_rsrc_mgr->request_release_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            auto object = std::static_pointer_cast<geometry::pcd_object>(geometry_object);
            object->mark_dirty();
            _pcd_geometries.remove(object);
            gpu_rsrc_mgr->request_release_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::mesh: {
            auto object = std::static_pointer_cast<geometry::mesh_object>(geometry_object);
            object->mark_dirty();
            _mesh_geometries.remove(object);
            gpu_rsrc_mgr->request_release_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::skeleton: {
            auto object = std::static_pointer_cast<geometry::skeleton_object>(geometry_object);
            object->mark_dirty();
            _skeleton_geometries.remove(object);
            for (const auto& joint_obj : object->get_joint_objects()) {
                gpu_rsrc_mgr->request_release_geometry_resource(joint_obj);
            }
            for (const auto& bone_obj : object->get_bone_objects()) {
                gpu_rsrc_mgr->request_release_geometry_resource(bone_obj);
            }
            break;
        }
        }
    }

    void scene::clear_geometries()
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_PANIC("Failed to access gpu resource manager");
        }

        for (auto& object : _lineset_geometries) {
            object->mark_dirty();
            gpu_rsrc_mgr->request_release_geometry_resource(object);
        }
        _lineset_geometries.clear();

        for (auto& object : _pcd_geometries) {
            object->mark_dirty();
            gpu_rsrc_mgr->request_release_geometry_resource(object);
        }
        _pcd_geometries.clear();
        
        for (auto& object : _mesh_geometries) {
            object->mark_dirty();
            gpu_rsrc_mgr->request_release_geometry_resource(object);
        }
        _mesh_geometries.clear();
        
        for (auto& object : _skeleton_geometries) {
            for (const auto& joint_obj : object->get_joint_objects()) {
                joint_obj->mark_dirty();
                gpu_rsrc_mgr->request_release_geometry_resource(joint_obj);
            }
            for (const auto& bone_obj : object->get_bone_objects()) {
                bone_obj->mark_dirty();
                gpu_rsrc_mgr->request_release_geometry_resource(bone_obj);
            }
            object->mark_dirty();
        }
        _skeleton_geometries.clear();
    }

    std::shared_ptr<text_3d_object> scene::add_label_3d(
        const std::string_view text, 
        const vec3_f32& world_space_pos, 
        const float scale, 
        const color3_f32& color, 
        const text_alignment_type text_align, 
        const bool make_visible)
    {
        auto new_label = std::make_shared<text_3d_object>();
        new_label->set_text(text);
        new_label->set_text_alignment(text_align);
        new_label->set_position(world_space_pos);
        new_label->set_scale(scale);
        new_label->set_color(color);
        new_label->set_visible(make_visible);

        auto it = _text_3d_objects.insert(_text_3d_objects.end(), new_label);
        _text_3d_objects_id_map[new_label->get_id()] = it;

        return new_label;
    }

    void scene::remove_label_3d(text_object_id_type label_id)
    {
        auto map_it = _text_3d_objects_id_map.find(label_id);
        if (map_it == _text_3d_objects_id_map.end()) {
            return;
        }
        _text_3d_objects.erase(map_it->second);
        _text_3d_objects_id_map.erase(map_it);
    }

    void scene::clear_label_3d()
    {
        _text_3d_objects_id_map.clear();
        _text_3d_objects.clear();
    }

    std::shared_ptr<text_2d_object> scene::add_label_2d(
        const std::string_view text, 
        const vec2_f32 screen_space_pos, 
        const float scale, 
        const color3_f32& color, 
        const text_alignment_type text_align, 
        const bool make_visible)
    {
        auto new_label = std::make_shared<text_2d_object>();
        new_label->set_text(text);
        new_label->set_text_alignment(text_align);
        new_label->set_position(screen_space_pos);
        new_label->set_scale(scale);
        new_label->set_color(color);
        new_label->set_visible(make_visible);

        auto it = _text_2d_objects.insert(_text_2d_objects.end(), new_label);
        _text_2d_objects_id_map[new_label->get_id()] = it;

        return new_label;
    }

    void scene::remove_label_2d(text_object_id_type label_id)
    {
        auto map_it = _text_2d_objects_id_map.find(label_id);
        if (map_it == _text_2d_objects_id_map.end()) {
            return;
        }
        _text_2d_objects.erase(map_it->second);
        _text_2d_objects_id_map.erase(map_it);
    }

    void scene::clear_label_2d()
    {
        _text_2d_objects_id_map.clear();
        _text_2d_objects.clear();
    }

    texture_handle_t scene::create_texture_2d(
        const std::shared_ptr<image_buffer>& tex_image,
        const texture_params_t& tex_params)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_PANIC("Failed to access gpu resource manager");
        }

        return gpu_rsrc_mgr->request_create_texture_resource(tex_image, tex_params);
    }

    texture_handle_t scene::create_texture_2d_from_memory(
        const uint8_t* const image_file_buff,
        const size_t image_file_buff_size,
        const texture_params_t& tex_params)
    {
        auto tex_image = std::make_shared<image_buffer>();
        tex_image->load_from_memory(image_file_buff, image_file_buff_size);
        return this->create_texture_2d(tex_image, tex_params);
    }

    texture_handle_t scene::create_texture_2d_from_file(
        const std::filesystem::path& image_path, 
        const texture_params_t& tex_params,
        const bool flip_image)
    {
        auto tex_image = std::make_shared<image_buffer>();
        tex_image->load_from_file(image_path, flip_image);
        return this->create_texture_2d(tex_image, tex_params);
    }

    texture_handle_t scene::create_texture_2d_from_uniform_color(
        const color3_f32& color)
    {
        // Create a 1x1 image with the specified uniform color
        auto tex_image = std::make_shared<image_buffer>();
        tex_image->prepare(1, 1, image_format_type::rgb);

        // Convert float color to 0-255 range
        uint8_t* const rgb0 = tex_image->data();
        rgb0[0] = static_cast<uint8_t>(std::clamp(color.r() * 255.0f, 0.0f, 255.0f));
        rgb0[1] = static_cast<uint8_t>(std::clamp(color.g() * 255.0f, 0.0f, 255.0f));
        rgb0[2] = static_cast<uint8_t>(std::clamp(color.b() * 255.0f, 0.0f, 255.0f));

        return this->create_texture_2d(tex_image, texture_params_t{});
    }

    void scene::update_texture_2d(
        const texture_handle_t texture_handle,
        const std::shared_ptr<image_buffer>& tex_image)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_PANIC("Failed to access gpu resource manager");
        }

        gpu_rsrc_mgr->request_update_texture_resource(texture_handle, tex_image);
    }

    void scene::destroy_texture(
        const texture_handle_t texture_handle)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_PANIC("Failed to access gpu resource manager");
        }

        gpu_rsrc_mgr->request_destroy_texture_resource(texture_handle);
    }

} // namespace