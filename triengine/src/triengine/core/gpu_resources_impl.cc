#include "gpu_resource_manager.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <atomic>

namespace triengine::core
{
    namespace
    {
        gpu_resource_id_t _create_unique_gpu_resource_id() {
            static std::atomic<gpu_resource_id_t> cnt_ = 0;
            return cnt_++; // TODO: overflow check?
        }

    } // namespace

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    mesh_gpu_rsrc::mesh_gpu_rsrc()
        : core::geometry_gpu_rsrc_base<mesh_gpu_rsrc>{ _create_unique_gpu_resource_id() }
    {
        TRIENGINE_TRACE("CREATE mesh_gpu_rsrc(#%X)", this->get_id());

        GLCall(::glCreateVertexArrays(1, &vao));
        GLCall(::glCreateBuffers(1, &vbo));
        GLCall(::glCreateBuffers(1, &ibo));
        
        GLCall(::glEnableVertexArrayAttrib(vao, 0/*attribindex*/)); // attrib 0 = positions
        GLCall(::glEnableVertexArrayAttrib(vao, 1/*attribindex*/)); // attrib 1 = normals
        GLCall(::glEnableVertexArrayAttrib(vao, 2/*attribindex*/)); // attrib 2 = texcoords / colors

        GLCall(::glVertexArrayAttribBinding(vao, 0/*attribindex*/, 0/*bindingindex*/)); // attrib 0 <- binding 0
        GLCall(::glVertexArrayAttribBinding(vao, 1/*attribindex*/, 1/*bindingindex*/)); // attrib 1 <- binding 1
        GLCall(::glVertexArrayAttribBinding(vao, 2/*attribindex*/, 2/*bindingindex*/)); // attrib 2 <- binding 2

        GLCall(::glVertexArrayElementBuffer(vao, ibo));
        
        TRIENGINE_ASSERT(this->is_valid());
    }

    mesh_gpu_rsrc::~mesh_gpu_rsrc()
    {
        TRIENGINE_TRACE("DESTROY mesh_gpu_rsrc(#%X)", this->get_id());

        if (ibo) {
            ::glDeleteBuffers(1, &ibo);
            ibo = 0;
        }
        
        if (vbo) {
            ::glDeleteBuffers(1, &vbo);
            vbo = 0;
        }

        if (vao) {
            ::glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
    }

    bool mesh_gpu_rsrc::is_valid_impl() const
    { 
        return 
            vao != 0 && 
            vbo != 0 && 
            ibo != 0;
    }

    void mesh_gpu_rsrc::update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object)
    {
        //TRIENGINE_TRACE("UPDATE mesh_gpu_rsrc(#%X)", this->get_id());

        TRIENGINE_ASSERT(geometry_object != nullptr);
        TRIENGINE_ASSERT(geometry_object->get_type() == geometry::geometry_object_type::mesh);

        auto mesh_object = std::static_pointer_cast<geometry::mesh_object>(geometry_object);

        /**
         * glNamedBufferStorage -> Cannot be resized. Calling it again with the same ID but a different size will result in an error.
         * glNamedBufferData    -> Can be resized. Calling it again with the same ID but a different size will result in a reallocation.
         */
        switch (mesh_object->get_shading_mode()) {
        case geometry::mesh_object::shading_mode::vertex:
        {
            const auto& positions = mesh_object->vertex_positions;
            const auto& normals = mesh_object->vertex_normals;
            const auto& colors = mesh_object->vertex_colors;
            const auto& triangle_indices = mesh_object->triangle_indices;

            using position_value_type = std::decay_t<decltype(positions)>::value_type;
            using normal_value_type = std::decay_t<decltype(normals)>::value_type;
            using color_value_type = std::decay_t<decltype(colors)>::value_type;
            using triangle_index_value_type = std::decay_t<decltype(triangle_indices)>::value_type;

            TRIENGINE_ASSERT(!positions.empty());
            TRIENGINE_ASSERT(!triangle_indices.empty());
            TRIENGINE_ASSERT(colors.size() == positions.size());
            TRIENGINE_ASSERT(normals.size() == positions.size() || normals.empty());

            const GLsizei 
                positions_size_bytes = static_cast<GLsizei>(positions.size() * sizeof(position_value_type)),
                normals_size_bytes = static_cast<GLsizei>(normals.size() * sizeof(normal_value_type)),
                colors_size_bytes = static_cast<GLsizei>(colors.size() * sizeof(color_value_type));
            
            const GLintptr
                positions_offset = 0,
                normals_offset   = 0 + positions_size_bytes,
                colors_offset    = 0 + positions_size_bytes + normals_size_bytes;

            const GLsizeiptr
                total_vertices_size_bytes = static_cast<GLsizeiptr>(positions_size_bytes + normals_size_bytes + colors_size_bytes),
                total_indices_size_bytes = static_cast<GLsizeiptr>(triangle_indices.size() * sizeof(triangle_index_value_type));

            //
            // ---------- Upload memory data ----------
            //

            // Upload vertices to VBO
            GLCall(::glNamedBufferData(vbo, total_vertices_size_bytes, nullptr/* initial data */, GL_DYNAMIC_DRAW)); // allocate space only
            GLCall(::glNamedBufferSubData(vbo, positions_offset, positions_size_bytes, positions.data()));
            GLCall(::glNamedBufferSubData(vbo, normals_offset, normals_size_bytes, normals.data()));
            GLCall(::glNamedBufferSubData(vbo, colors_offset, colors_size_bytes, colors.data()));

            // Upload indices to IBO
            GLCall(::glNamedBufferData(ibo, total_indices_size_bytes, triangle_indices.data()/* initial data */, GL_DYNAMIC_DRAW)); // allocate & copy data

            //
            // ---------- Setup memory layout (SOA) ----------
            //

            // Link VAO's binding index to the VBO.
            GLCall(::glVertexArrayVertexBuffer(vao, 0/*bindingindex*/, vbo, positions_offset, sizeof(position_value_type)/*stride*/));
            GLCall(::glVertexArrayVertexBuffer(vao, 1/*bindingindex*/, vbo, normals_offset, sizeof(normal_value_type)/*stride*/));
            GLCall(::glVertexArrayVertexBuffer(vao, 2/*bindingindex*/, vbo, colors_offset, sizeof(color_value_type)/*stride*/));
            
            // Define the format of each vertex attributes
            GLCall(::glVertexArrayAttribFormat(vao, 0/*attribindex*/, position_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
            GLCall(::glVertexArrayAttribFormat(vao, 1/*attribindex*/, normal_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
            GLCall(::glVertexArrayAttribFormat(vao, 2/*attribindex*/, color_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));

            break;
        }
        case geometry::mesh_object::shading_mode::texture:
        {
            const auto& positions = mesh_object->vertex_positions;
            const auto& normals = mesh_object->vertex_normals;
            const auto& texcoords = mesh_object->vertex_uvs;
            const auto& triangle_indices = mesh_object->triangle_indices;

            using position_value_type = std::decay_t<decltype(positions)>::value_type;
            using normal_value_type = std::decay_t<decltype(normals)>::value_type;
            using texcoord_value_type = std::decay_t<decltype(texcoords)>::value_type;
            using triangle_index_value_type = std::decay_t<decltype(triangle_indices)>::value_type;

            TRIENGINE_ASSERT(!positions.empty());
            TRIENGINE_ASSERT(!triangle_indices.empty());
            TRIENGINE_ASSERT(texcoords.size() == positions.size());
            TRIENGINE_ASSERT(normals.size() == positions.size() || normals.empty());

            const GLsizei 
                positions_size_bytes = static_cast<GLsizei>(positions.size() * sizeof(position_value_type)),
                normals_size_bytes = static_cast<GLsizei>(normals.size() * sizeof(normal_value_type)),
                texcoords_size_bytes = static_cast<GLsizei>(texcoords.size() * sizeof(texcoord_value_type));

            const GLintptr
                positions_offset = 0,
                normals_offset   = 0 + positions_size_bytes,
                texcoords_offset = 0 + positions_size_bytes + normals_size_bytes;

            const GLsizeiptr
                total_vertices_size_bytes = static_cast<GLsizeiptr>(positions_size_bytes + normals_size_bytes + texcoords_size_bytes),
                total_indices_size_bytes = static_cast<GLsizeiptr>(triangle_indices.size() * sizeof(triangle_index_value_type));
            
            //
            // ---------- Upload memory data ----------
            //

            // Upload vertices to VBO
            GLCall(::glNamedBufferData(vbo, total_vertices_size_bytes, nullptr/* initial data */, GL_DYNAMIC_DRAW)); // allocate space only
            GLCall(::glNamedBufferSubData(vbo, positions_offset, positions_size_bytes, positions.data()));
            GLCall(::glNamedBufferSubData(vbo, normals_offset, normals_size_bytes, normals.data()));
            GLCall(::glNamedBufferSubData(vbo, texcoords_offset, texcoords_size_bytes, texcoords.data()));

            // Upload indices to IBO
            GLCall(::glNamedBufferData(ibo, total_indices_size_bytes, triangle_indices.data()/* initial data */, GL_DYNAMIC_DRAW)); // allocate & copy data

            //
            // ---------- Setup memory layout (SOA) ----------
            //

            // Link VAO's binding index to the VBO. (SOA memory layout)
            GLCall(::glVertexArrayVertexBuffer(vao, 0/*bindingindex*/, vbo, positions_offset, sizeof(position_value_type)/*stride*/));
            GLCall(::glVertexArrayVertexBuffer(vao, 1/*bindingindex*/, vbo, normals_offset, sizeof(normal_value_type)/*stride*/));
            GLCall(::glVertexArrayVertexBuffer(vao, 2/*bindingindex*/, vbo, texcoords_offset, sizeof(texcoord_value_type)/*stride*/));
            
            // Define the format of each vertex attributes (SOA memory layout)
            GLCall(::glVertexArrayAttribFormat(vao, 0/*attribindex*/, position_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
            GLCall(::glVertexArrayAttribFormat(vao, 1/*attribindex*/, normal_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
            GLCall(::glVertexArrayAttribFormat(vao, 2/*attribindex*/, texcoord_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));

            break;
        }
        } // switch
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    pcd_gpu_rsrc::pcd_gpu_rsrc()
        : core::geometry_gpu_rsrc_base<pcd_gpu_rsrc>{ _create_unique_gpu_resource_id() }
    {
        TRIENGINE_TRACE("CREATE pcd_gpu_rsrc(#%X)", this->get_id());

        GLCall(::glCreateVertexArrays(1, &vao));
        GLCall(::glCreateBuffers(1, &vbo));
        
        GLCall(::glEnableVertexArrayAttrib(vao, 0/*attribindex*/)); // attrib 0 = positions
        GLCall(::glEnableVertexArrayAttrib(vao, 1/*attribindex*/)); // attrib 1 = normals
        GLCall(::glEnableVertexArrayAttrib(vao, 2/*attribindex*/)); // attrib 2 = colors

        GLCall(::glVertexArrayAttribBinding(vao, 0/*attribindex*/, 0/*bindingindex*/)); // attrib 0 <- binding 0
        GLCall(::glVertexArrayAttribBinding(vao, 1/*attribindex*/, 1/*bindingindex*/)); // attrib 1 <- binding 1
        GLCall(::glVertexArrayAttribBinding(vao, 2/*attribindex*/, 2/*bindingindex*/)); // attrib 2 <- binding 2

        TRIENGINE_ASSERT(this->is_valid());
    }

    pcd_gpu_rsrc::~pcd_gpu_rsrc()
    {
        TRIENGINE_TRACE("DESTROY pcd_gpu_rsrc(#%X)", this->get_id());

        if (vbo) {
            ::glDeleteBuffers(1, &vbo);
            vbo = 0;
        }

        if (vao) {
            ::glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
    }
    
    bool pcd_gpu_rsrc::is_valid_impl() const
    {
        return 
            vao != 0 && 
            vbo != 0;
    }

    void pcd_gpu_rsrc::update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object)
    {
        //TRIENGINE_TRACE("UPDATE pcd_gpu_rsrc(#%X)", this->get_id());

        TRIENGINE_ASSERT(geometry_object != nullptr);
        TRIENGINE_ASSERT(geometry_object->get_type() == geometry::geometry_object_type::pointcloud);

        auto pcd_object = std::static_pointer_cast<geometry::pcd_object>(geometry_object);

        const auto& positions = pcd_object->points;
        const auto& normals = pcd_object->normals;
        const auto& colors = pcd_object->colors;

        using position_value_type = std::decay_t<decltype(positions)>::value_type;
        using normal_value_type = std::decay_t<decltype(normals)>::value_type;
        using color_value_type = std::decay_t<decltype(colors)>::value_type;

        TRIENGINE_ASSERT(!positions.empty());
        TRIENGINE_ASSERT(colors.size() == positions.size());
        TRIENGINE_ASSERT(normals.size() == positions.size() || normals.empty());

        const GLsizei 
            positions_size_bytes = static_cast<GLsizei>(positions.size() * sizeof(position_value_type)),
            normals_size_bytes = static_cast<GLsizei>(normals.size() * sizeof(normal_value_type)),
            colors_size_bytes = static_cast<GLsizei>(colors.size() * sizeof(color_value_type));
        
        const GLintptr
            positions_offset = 0,
            normals_offset   = 0 + positions_size_bytes,
            colors_offset    = 0 + positions_size_bytes + normals_size_bytes;

        const GLsizeiptr
            total_vertices_size_bytes = static_cast<GLsizeiptr>(positions_size_bytes + normals_size_bytes + colors_size_bytes);

        //
        // ---------- Upload memory data ----------
        //

        // Upload vertices to VBO
        GLCall(::glNamedBufferData(vbo, total_vertices_size_bytes, nullptr/* initial data */, GL_DYNAMIC_DRAW)); // allocate space only
        GLCall(::glNamedBufferSubData(vbo, positions_offset, positions_size_bytes, positions.data()));
        GLCall(::glNamedBufferSubData(vbo, normals_offset, normals_size_bytes, normals.data()));
        GLCall(::glNamedBufferSubData(vbo, colors_offset, colors_size_bytes, colors.data()));

        //
        // ---------- Setup memory layout (SOA) ----------
        //

        // Link VAO's binding index to the VBO.
        GLCall(::glVertexArrayVertexBuffer(vao, 0/*bindingindex*/, vbo, positions_offset, sizeof(position_value_type)/*stride*/));
        GLCall(::glVertexArrayVertexBuffer(vao, 1/*bindingindex*/, vbo, normals_offset, sizeof(normal_value_type)/*stride*/));
        GLCall(::glVertexArrayVertexBuffer(vao, 2/*bindingindex*/, vbo, colors_offset, sizeof(color_value_type)/*stride*/));
        
        // Define the format of each vertex attributes
        GLCall(::glVertexArrayAttribFormat(vao, 0/*attribindex*/, position_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
        GLCall(::glVertexArrayAttribFormat(vao, 1/*attribindex*/, normal_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
        GLCall(::glVertexArrayAttribFormat(vao, 2/*attribindex*/, color_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    lineset_gpu_rsrc::lineset_gpu_rsrc()
        : core::geometry_gpu_rsrc_base<lineset_gpu_rsrc>{ _create_unique_gpu_resource_id() }
    {
        TRIENGINE_TRACE("CREATE lineset_gpu_rsrc(#%X)", this->get_id());
        
        GLCall(::glCreateVertexArrays(1, &vao));
        GLCall(::glCreateBuffers(1, &vbo));
        GLCall(::glCreateBuffers(1, &ibo));
        
        GLCall(::glEnableVertexArrayAttrib(vao, 0/*attribindex*/)); // attrib 0 = positions
        GLCall(::glEnableVertexArrayAttrib(vao, 1/*attribindex*/)); // attrib 1 = colors

        GLCall(::glVertexArrayAttribBinding(vao, 0/*attribindex*/, 0/*bindingindex*/)); // attrib 0 <- binding 0
        GLCall(::glVertexArrayAttribBinding(vao, 1/*attribindex*/, 1/*bindingindex*/)); // attrib 1 <- binding 1

        GLCall(::glVertexArrayElementBuffer(vao, ibo));
        
        TRIENGINE_ASSERT(this->is_valid());
    }

    lineset_gpu_rsrc::~lineset_gpu_rsrc()
    {
        TRIENGINE_TRACE("DESTROY lineset_gpu_rsrc(#%X)", this->get_id());
        
        if (ibo) {
            ::glDeleteBuffers(1, &ibo);
            ibo = 0;
        }
        
        if (vbo) {
            ::glDeleteBuffers(1, &vbo);
            vbo = 0;
        }

        if (vao) {
            ::glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
    }
    
    bool lineset_gpu_rsrc::is_valid_impl() const
    {
        return 
            vao != 0 && 
            vbo != 0 && 
            ibo != 0;
    }

    void lineset_gpu_rsrc::update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object)
    {
        //TRIENGINE_TRACE("UPDATE lineset_gpu_rsrc(#%X)", this->get_id());
        
        TRIENGINE_ASSERT(geometry_object != nullptr);
        TRIENGINE_ASSERT(geometry_object->get_type() == geometry::geometry_object_type::lineset);

        auto lineset_object = std::static_pointer_cast<geometry::lineset_object>(geometry_object);

        const auto& positions = lineset_object->line_points;
        const auto& colors = lineset_object->line_colors;
        const auto& line_indices = lineset_object->line_indices;

        using position_value_type = std::decay_t<decltype(positions)>::value_type;
        using color_value_type = std::decay_t<decltype(colors)>::value_type;
        using line_index_value_type = std::decay_t<decltype(line_indices)>::value_type;

        TRIENGINE_ASSERT(!positions.empty());
        TRIENGINE_ASSERT(!line_indices.empty());
        TRIENGINE_ASSERT(colors.size() == positions.size());

        const GLsizei 
            positions_size_bytes = static_cast<GLsizei>(positions.size() * sizeof(position_value_type)),
            colors_size_bytes = static_cast<GLsizei>(colors.size() * sizeof(color_value_type));
        
        const GLintptr
            positions_offset = 0,
            colors_offset    = 0 + positions_size_bytes;

        const GLsizeiptr
            total_vertices_size_bytes = static_cast<GLsizeiptr>(positions_size_bytes + colors_size_bytes),
            total_indices_size_bytes = static_cast<GLsizeiptr>(line_indices.size() * sizeof(line_index_value_type));

        //
        // ---------- Upload memory data ----------
        //

        // Upload vertices to VBO
        GLCall(::glNamedBufferData(vbo, total_vertices_size_bytes, nullptr/* initial data */, GL_DYNAMIC_DRAW)); // allocate space only
        GLCall(::glNamedBufferSubData(vbo, positions_offset, positions_size_bytes, positions.data()));
        GLCall(::glNamedBufferSubData(vbo, colors_offset, colors_size_bytes, colors.data()));

        // Upload indices to IBO
        GLCall(::glNamedBufferData(ibo, total_indices_size_bytes, line_indices.data()/* initial data */, GL_DYNAMIC_DRAW)); // allocate & copy data

        //
        // ---------- Setup memory layout (SOA) ----------
        //

        // Link VAO's binding index to the VBO.
        GLCall(::glVertexArrayVertexBuffer(vao, 0/*bindingindex*/, vbo, positions_offset, sizeof(position_value_type)/*stride*/));
        GLCall(::glVertexArrayVertexBuffer(vao, 1/*bindingindex*/, vbo, colors_offset, sizeof(color_value_type)/*stride*/));
        
        // Define the format of each vertex attributes
        GLCall(::glVertexArrayAttribFormat(vao, 0/*attribindex*/, position_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
        GLCall(::glVertexArrayAttribFormat(vao, 1/*attribindex*/, color_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
