#include "gpu_resource_manager.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <atomic>

namespace triengine::core
{
    namespace
    {
        uint64_t _create_unique_gpu_resource_id() {
            static std::atomic_uint64_t cnt_ = 0;
            return cnt_++; // TODO: overflow check?
        }

    } // namespace

    void gpu_resource_manager::request_create_geometry_resource(
        std::shared_ptr<geometry::geometry_object_base> object) 
    {
        command_data cmd;
        cmd.cmd_type = command_type::create_geometry_resource;
        cmd.obj_type = object->get_type();
        cmd.obj_id = object->get_id();

        std::scoped_lock lk{ _cmd_q_mtx };
        _cmd_q.emplace_back(std::move(cmd));
    }

    void gpu_resource_manager::request_destroy_geometry_resource(
        std::shared_ptr<geometry::geometry_object_base> object)
    {
        command_data cmd;
        cmd.cmd_type = command_type::delete_geometry_resource;
        cmd.obj_type = object->get_type();
        cmd.obj_id = object->get_id();
        
        std::scoped_lock lk{ _cmd_q_mtx };
        _cmd_q.emplace_back(std::move(cmd));
    }

    // NOTE: must be called in render thread
    void gpu_resource_manager::process_pending_requests()
    {
        std::deque<command_data> cmd_q; {
            std::scoped_lock lk{ _cmd_q_mtx };    
            if (_cmd_q.empty()) { return; }
            cmd_q.swap(_cmd_q); // swap queue (minimize lock time)
        }

        for (const auto& cmd : cmd_q)
        {
            if (cmd.cmd_type == command_type::create_geometry_resource)
            {
                this->_create_geometry_resource(cmd.obj_type, cmd.obj_id);
            }
            else if (cmd.cmd_type == command_type::delete_geometry_resource)
            {
                this->_destroy_geometry_resource(cmd.obj_type, cmd.obj_id);
            }
        } // for
    }

    // NOTE: must be called in render thread
    triangle_mesh_gpu_rsrc_ptr gpu_resource_manager::get_triangle_mesh_resource(
        const std::shared_ptr<geometry::triangle_mesh_object>& object) const
    {
        auto it = _triangle_mesh_rsrc_map.find(object->get_id());
        if (it == _triangle_mesh_rsrc_map.end()) {
            TRIENGINE_TRACE("Failed to get triangle mesh gpu resource: %s", object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    pcd_gpu_rsrc_ptr gpu_resource_manager::get_pcd_resource(
        const std::shared_ptr<geometry::pcd_object>& object) const
    {
        auto it = _pcd_rsrc_map.find(object->get_id());
        if (it == _pcd_rsrc_map.end()) {
            TRIENGINE_TRACE("Failed to get pcd gpu resource: %s", object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    lineset_gpu_rsrc_ptr gpu_resource_manager::get_lineset_resource(
        const std::shared_ptr<geometry::lineset_object>& object) const
    {
        auto it = _lineset_rsrc_map.find(object->get_id());
        if (it == _lineset_rsrc_map.end()) {
            TRIENGINE_TRACE("Failed to get lineset gpu resource: %s", object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    void gpu_resource_manager::_create_geometry_resource(
        const geometry::geometry_object_type obj_type,
        const geometry::geometry_object_id_t obj_id)
    {
        switch (obj_type) {
        case geometry::geometry_object_type::triangle_mesh: {
            const auto new_gpu_rsrc = std::make_shared<triangle_mesh_gpu_rsrc>(_create_unique_gpu_resource_id());
            const auto [insert_it, success] = _triangle_mesh_rsrc_map.insert(
                { obj_id, new_gpu_rsrc }
            );

            if (success) {
                TRIENGINE_TRACE("Created triangle_mesh_gpu_rsrc(#%llX) for geometry object #%llX"
                    , new_gpu_rsrc->get_id()
                    , obj_id
                );
            } else {
                TRIENGINE_TRACE("Failed to create triangle_mesh_gpu_rsrc for geometry object #%llX", obj_id);
            }

            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            const auto new_gpu_rsrc = std::make_shared<pcd_gpu_rsrc>(_create_unique_gpu_resource_id());
            const auto [insert_it, success] = _pcd_rsrc_map.insert(
                { obj_id, new_gpu_rsrc }
            );
    
            if (success) {
                TRIENGINE_TRACE("Created pcd_gpu_rsrc(#%llX) for geometry object #%llX"
                    , new_gpu_rsrc->get_id()
                    , obj_id
                );
            } else {
                TRIENGINE_TRACE("Failed to create pcd_gpu_rsrc for geometry object #%llX", obj_id);
            }

            break;
        }
        case geometry::geometry_object_type::lineset: {
            const auto new_gpu_rsrc = std::make_shared<lineset_gpu_rsrc>(_create_unique_gpu_resource_id());
            const auto [insert_it, success] = _lineset_rsrc_map.insert(
                { obj_id, new_gpu_rsrc }
            );

            if (success) {
                TRIENGINE_TRACE("Created lineset_gpu_rsrc(#%llX) for geometry object #%llX"
                    , new_gpu_rsrc->get_id()
                    , obj_id
                );
            } else {
                TRIENGINE_TRACE("Failed to create lineset_gpu_rsrc for geometry object #%llX", obj_id);
            }

            break;
        }
        default: {
            TRIENGINE_PANIC("Failed to create geometry resource. unsupported geometry type %d"
                , static_cast<int>(obj_type)
            );
            break;
        }
        } // switch
    }

    // NOTE: must be called in render thread
    void gpu_resource_manager::_destroy_geometry_resource(
        const geometry::geometry_object_type obj_type,
        const geometry::geometry_object_id_t obj_id)
    {
        switch (obj_type) {
        case geometry::geometry_object_type::triangle_mesh: {
            if (const auto it = _triangle_mesh_rsrc_map.find(obj_id);
                it != _triangle_mesh_rsrc_map.end()) {
                TRIENGINE_TRACE("Delete triangle_mesh_gpu_rsrc(#%llX) for geometry object #%llX"
                    , it->second->get_id()
                    , obj_id
                );
                _triangle_mesh_rsrc_map.erase(it);
            }    
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            if (const auto it = _pcd_rsrc_map.find(obj_id);
                it != _pcd_rsrc_map.end()) {
                TRIENGINE_TRACE("Delete pcd_gpu_rsrc(#%llX) for geometry object #%llX"
                    , it->second->get_id()
                    , obj_id
                );
                _pcd_rsrc_map.erase(it);
            }
            break;
        }
        case geometry::geometry_object_type::lineset: {
            if (const auto it = _lineset_rsrc_map.find(obj_id);
                it != _lineset_rsrc_map.end()) {
                TRIENGINE_TRACE("Delete lineset_gpu_rsrc(#%llX) for geometry object #%llX"
                    , it->second->get_id()
                    , obj_id
                );
                _lineset_rsrc_map.erase(it);
            }
            break;
        }
        default: {
            TRIENGINE_PANIC("Failed to destroy geometry resource (unsupported geometry type %d)"
                , static_cast<int>(obj_type)
            );
            break;
        }
        } // switch
    }

} // namespace

/*
void ResourceManager::ProcessRequests() {
    //...

    // 선택 사항: 풀 크기가 최대치를 초과하면 리소스 제거
    {
        std::lock_guard<std::mutex> lock(m_mutex);
         while (m_resourcePool.size() > MAX_POOL_SIZE) {
             // 가장 오래된 것 제거 (FIFO - push_back 사용 시 벡터의 앞부분)
             std::cout << "Resource pool limit (" << MAX_POOL_SIZE << ") exceeded. Releasing oldest resource." << std::endl;
             m_resourcePool.front().release(); // push_back/pop_front 의미론 가정
             m_resourcePool.erase(m_resourcePool.begin()); // 또는 효율적인 앞부분 제거를 위해 std::deque 사용
         }
    }
}

void ResourceManager::CreateGPUResources(const CreateRequest& request) {
    //...

    GPUResource gpuRes;
    bool reused = false;

    // 1. 풀에서 재사용 시도 (뮤텍스로 보호)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_resourcePool.empty()) {
            // 간단한 풀링: 마지막 것을 가져옴.
            // 더 복잡하게: 호환되는 버퍼 크기를 가진 것을 찾기?
            gpuRes = std::move(m_resourcePool.back()); // 이동 의미론 사용
            m_resourcePool.pop_back();
            reused = true;
            std::cout << "Reusing GPU resource from pool for ID " << meshId << std::endl;

            // 새 메시가 다른 수나 종류의 텍스처를 사용하는 경우
            // 이전 텍스처를 삭제해야 할 수 있음.
            // 새 텍스처 로드 전에 기존 텍스처 ID 목록 지우기.
            // 실제 glDeleteTextures는 아래에서 필요 시 발생하거나, 개수가 다르면 여기서 바로 발생.
            for(GLuint texID : gpuRes.textureIDs) {
                 if (texID) glDeleteTextures(1, &texID);
             }
             gpuRes.textureIDs.clear();
        }
    } // 뮤텍스 해제

    // 2. 재사용하지 않았다면 새 OpenGL 핸들 생성
    if (!reused) {
        glGenVertexArrays(1, &gpuRes.vao);
        glGenBuffers(1, &gpuRes.vboVertices);
        glGenBuffers(1, &gpuRes.vboIndices);
        // 텍스처는 LoadTexture에서 생성됨
        std::cout << "Creating new GPU resource (VAO: " << gpuRes.vao << ") for ID " << meshId << std::endl;
        // OpenGL 오류 확인 추가 권장
        // GLenum err; while((err = glGetError()) != GL_NO_ERROR) { std::cerr << "OpenGL Error after Gen Buffers: " << err << std::endl; }
    }

    //...
}

void ResourceManager::DestroyOrPoolGPUResources(MeshID meshId) {
    // ProcessRequests에 의해 뮤텍스가 이미 잠겨 있다고 가정
    auto it = m_resourceMap.find(meshId);
    if (it != m_resourceMap.end()) {
        std::cout << "Pooling GPU resource for ID " << meshId << std::endl;
        // 리소스를 맵에서 풀로 이동
        m_resourcePool.push_back(std::move(it->second)); // 이동 의미론 사용
        m_resourceMap.erase(it); // 맵에서 항목 제거
    } else {
         // 이미 처리되었거나 유효하지 않은 ID일 수 있음
         std::cerr << "Warning: Request to destroy/pool non-existent or already processed resource ID: " << meshId << std::endl;
    }
}

// 풀 정리 헬퍼
void ResourceManager::ClearPool_NoLock() {
     std::cout << "Clearing resource pool (" << m_resourcePool.size() << " items)." << std::endl;
     for (GPUResource& res : m_resourcePool) {
          res.release(); // glDelete* 호출
     }
     m_resourcePool.clear();
}
*/