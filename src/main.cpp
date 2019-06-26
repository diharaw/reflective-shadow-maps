#include <application.h>
#include <mesh.h>
#include <camera.h>
#include <material.h>
#include <memory>
#include <iostream>
#include <stack>
#include <random>
#include <chrono>

#define CAMERA_FAR_PLANE 1000.0f
#define RSM_SIZE 1024

// Uniform buffer data structure.
struct ObjectUniforms
{
	DW_ALIGNED(16) glm::mat4 model;
};

struct GlobalUniforms
{
    DW_ALIGNED(16) glm::mat4 view_proj;
    DW_ALIGNED(16) glm::mat4 light_view_proj;
    DW_ALIGNED(16) glm::mat4 shadow_mat;
	DW_ALIGNED(16) glm::vec4 cam_pos;
};

class ReflectiveShadowMaps : public dw::Application
{
protected:
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
	bool init(int argc, const char* argv[]) override
	{
		// Create GPU resources.
		if (!create_shaders())
			return false;

		if (!create_uniform_buffer())
			return false;

		// Load scene.
		if (!load_scene())
			return false;
        
        create_framebuffers();
        
        create_spot_light();

		// Create camera.
		create_camera();

		// Object transforms
        m_object_transforms.model = glm::scale(glm::mat4(1.0f), glm::vec3(10.0f));

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void update(double delta) override
	{
		// Update camera.
        update_camera();

		update_global_uniforms(m_global_uniforms);
		update_object_uniforms(m_object_transforms);

//        render_rsm();
        render_gbuffer();
//        direct_lighting();
//        indirect_lighting();
        
//        if (m_debug_mode)
//            m_debug_draw.frustum(m_flythrough_camera->projection * m_flythrough_camera->view, glm::vec3(0.0f, 1.0f, 0.0f));
//
//        // Render debug draw.
//        m_debug_draw.render(nullptr, m_width, m_height, m_global_uniforms.projection * m_global_uniforms.view);
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void shutdown() override
	{
		for (auto mesh : m_scene)
			dw::Mesh::unload(mesh);

		m_scene.clear();
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void window_resized(int width, int height) override
	{
        // Override window resized method to update camera projection.
        m_main_camera->update_projection(60.0f, 0.1f, CAMERA_FAR_PLANE, float(m_width) / float(m_height));
        m_debug_camera->update_projection(60.0f, 0.1f, CAMERA_FAR_PLANE * 2.0f, float(m_width) / float(m_height));
        
        create_framebuffers();
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
    
    void key_pressed(int code) override
    {
        // Handle forward movement.
        if(code == GLFW_KEY_W)
            m_heading_speed = m_camera_speed;
        else if(code == GLFW_KEY_S)
            m_heading_speed = -m_camera_speed;
        
        // Handle sideways movement.
        if(code == GLFW_KEY_A)
            m_sideways_speed = -m_camera_speed;
        else if(code == GLFW_KEY_D)
            m_sideways_speed = m_camera_speed;

		if (code == GLFW_KEY_K)
			m_debug_mode = !m_debug_mode;
        
        if (code == GLFW_KEY_SPACE)
            m_mouse_look = true;
    }
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void key_released(int code) override
    {
        // Handle forward movement.
        if(code == GLFW_KEY_W || code == GLFW_KEY_S)
            m_heading_speed = 0.0f;
        
        // Handle sideways movement.
        if(code == GLFW_KEY_A || code == GLFW_KEY_D)
            m_sideways_speed = 0.0f;
        
        if (code == GLFW_KEY_SPACE)
            m_mouse_look = false;
    }
    
    // -----------------------------------------------------------------------------------------------------------------------------------

	void mouse_pressed(int code) override
	{
		// Enable mouse look.
		if (code == GLFW_MOUSE_BUTTON_RIGHT)
			m_mouse_look = true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void mouse_released(int code) override
	{
		// Disable mouse look.
		if (code == GLFW_MOUSE_BUTTON_RIGHT)
			m_mouse_look = false;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

protected:

	// -----------------------------------------------------------------------------------------------------------------------------------

	dw::AppSettings intial_app_settings() override
	{
		dw::AppSettings settings;

		settings.resizable = true;
		settings.maximized = false;
		settings.refresh_rate = 60;
		settings.major_ver = 4;
		settings.width = 1280;
		settings.height = 720;
		settings.title = "Reflective Shadow Maps";

		return settings;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

private:
    
    void create_spot_light()
    {
        m_cone_angle = 30.0f;
        m_light_view = glm::lookAt(m_light_pos, m_light_pos + m_light_dir, glm::vec3(0.0f, 1.0f, 0.0f));
        m_light_proj = glm::perspective(glm::radians(2.0f * m_cone_angle), 1.0f, 0.1f, 1000.0f);
    }
    
    // -----------------------------------------------------------------------------------------------------------------------------------

	bool create_shaders()
	{
		{
			// Create general shaders
			m_fullscreen_triangle_vs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_VERTEX_SHADER, "shader/fullscreen_triangle_vs.glsl"));
			m_direct_fs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_FRAGMENT_SHADER, "shader/direct_light_fs.glsl"));
            m_indirect_fs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_FRAGMENT_SHADER, "shader/indirect_light_fs.glsl"));
            m_rsm_vs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_VERTEX_SHADER, "shader/rsm_vs.glsl"));
            m_gbuffer_vs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_VERTEX_SHADER, "shader/gbuffer_vs.glsl"));
            m_gbuffer_fs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_FRAGMENT_SHADER, "shader/gbuffer_fs.glsl"));

            {
                if (!m_fullscreen_triangle_vs || !m_direct_fs)
                {
                    DW_LOG_FATAL("Failed to create Shaders");
                    return false;
                }
                
                // Create general shader program
                dw::Shader* shaders[] = { m_fullscreen_triangle_vs.get(), m_direct_fs.get() };
                m_direct_program = std::make_unique<dw::Program>(2, shaders);
                
                if (!m_direct_program)
                {
                    DW_LOG_FATAL("Failed to create Shader Program");
                    return false;
                }
                
                m_direct_program->uniform_block_binding("GlobalUniforms", 0);
                m_direct_program->uniform_block_binding("ObjectUniforms", 1);
            }
            
            {
                if (!m_fullscreen_triangle_vs || !m_indirect_fs)
                {
                    DW_LOG_FATAL("Failed to create Shaders");
                    return false;
                }
                
                // Create general shader program
                dw::Shader* shaders[] = { m_fullscreen_triangle_vs.get(), m_indirect_fs.get() };
                m_indirect_program = std::make_unique<dw::Program>(2, shaders);
                
                if (!m_indirect_program)
                {
                    DW_LOG_FATAL("Failed to create Shader Program");
                    return false;
                }
                
                m_indirect_program->uniform_block_binding("GlobalUniforms", 0);
                m_indirect_program->uniform_block_binding("ObjectUniforms", 1);
            }
            
            {
                if (!m_rsm_vs || !m_gbuffer_fs)
                {
                    DW_LOG_FATAL("Failed to create Shaders");
                    return false;
                }
                
                // Create general shader program
                dw::Shader* shaders[] = { m_rsm_vs.get(), m_gbuffer_fs.get() };
                m_rsm_program = std::make_unique<dw::Program>(2, shaders);
                
                if (!m_rsm_program)
                {
                    DW_LOG_FATAL("Failed to create Shader Program");
                    return false;
                }
                
                m_rsm_program->uniform_block_binding("GlobalUniforms", 0);
                m_rsm_program->uniform_block_binding("ObjectUniforms", 1);
            }
            
            {
                if (!m_gbuffer_vs || !m_gbuffer_fs)
                {
                    DW_LOG_FATAL("Failed to create Shaders");
                    return false;
                }
                
                // Create general shader program
                dw::Shader* shaders[] = { m_gbuffer_vs.get(), m_gbuffer_fs.get() };
                m_gbuffer_program = std::make_unique<dw::Program>(2, shaders);
                
                if (!m_gbuffer_program)
                {
                    DW_LOG_FATAL("Failed to create Shader Program");
                    return false;
                }
                
                m_gbuffer_program->uniform_block_binding("GlobalUniforms", 0);
                m_gbuffer_program->uniform_block_binding("ObjectUniforms", 1);
            }
		}

		return true;
	}
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void create_framebuffers()
    {
        m_gbuffer_albedo_rt = std::make_unique<dw::Texture2D>(m_width, m_height, 1, 1, 1, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
        m_gbuffer_normals_rt = std::make_unique<dw::Texture2D>(m_width, m_height, 1, 1, 1, GL_RGB16F, GL_RGB, GL_HALF_FLOAT);
        m_gbuffer_world_pos_rt = std::make_unique<dw::Texture2D>(m_width, m_height, 1, 1, 1, GL_RGB32F, GL_RGB, GL_FLOAT);
        m_gbuffer_depth_rt = std::make_unique<dw::Texture2D>(m_width, m_height, 1, 1, 1, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
        
        m_rsm_flux_rt = std::make_unique<dw::Texture2D>(RSM_SIZE, RSM_SIZE, 1, 1, 1, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
        m_rsm_normals_rt = std::make_unique<dw::Texture2D>(RSM_SIZE, RSM_SIZE, 1, 1, 1, GL_RGB16F, GL_RGB, GL_HALF_FLOAT);
        m_rsm_world_pos_rt = std::make_unique<dw::Texture2D>(RSM_SIZE, RSM_SIZE, 1, 1, 1, GL_RGB32F, GL_RGB, GL_FLOAT);
        m_rsm_depth_rt = std::make_unique<dw::Texture2D>(RSM_SIZE, RSM_SIZE, 1, 1, 1, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
        
        m_direct_light_rt = std::make_unique<dw::Texture2D>(m_width, m_height, 1, 1, 1, GL_RGB16F, GL_RGB, GL_HALF_FLOAT);
        
        m_gbuffer_fbo = std::make_unique<dw::Framebuffer>();
        
        dw::Texture* gbuffer_rts[] = { m_gbuffer_albedo_rt.get(), m_gbuffer_normals_rt.get(), m_gbuffer_world_pos_rt.get() };
        m_gbuffer_fbo->attach_multiple_render_targets(3, gbuffer_rts);
        m_gbuffer_fbo->attach_depth_stencil_target(m_gbuffer_depth_rt.get(), 0, 0);
        
        m_rsm_fbo = std::make_unique<dw::Framebuffer>();
        
        dw::Texture* rsm_rts[] = { m_rsm_flux_rt.get(), m_rsm_normals_rt.get(), m_rsm_world_pos_rt.get() };
        m_rsm_fbo->attach_multiple_render_targets(3, rsm_rts);
        m_rsm_fbo->attach_depth_stencil_target(m_rsm_depth_rt.get(), 0, 0);
        
        m_direct_light_fbo = std::make_unique<dw::Framebuffer>();
        m_direct_light_fbo->attach_render_target(0, m_direct_light_rt.get(), 0, 0);
    }

	// -----------------------------------------------------------------------------------------------------------------------------------

	bool create_uniform_buffer()
	{
		// Create uniform buffer for object matrix data
        m_object_ubo = std::make_unique<dw::UniformBuffer>(GL_DYNAMIC_DRAW, sizeof(ObjectUniforms));
        
        // Create uniform buffer for global data
        m_global_ubo = std::make_unique<dw::UniformBuffer>(GL_DYNAMIC_DRAW, sizeof(GlobalUniforms));
        
		return true;
	}
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void render_rsm()
    {
        render_scene(m_rsm_fbo.get(), m_rsm_program, RSM_SIZE, RSM_SIZE, GL_FRONT);
    }
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void render_gbuffer()
    {
        render_scene(nullptr, m_gbuffer_program, m_width, m_height, GL_BACK);
    }
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void direct_lighting()
    {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        
        m_direct_light_fbo->bind();
        
        glViewport(0, 0, m_width, m_height);
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Bind shader program.
        m_direct_program->use();
        
        if (m_direct_program->set_uniform("s_Albedo", 0))
            m_gbuffer_albedo_rt->bind(0);
        
        if (m_direct_program->set_uniform("s_Normals", 1))
            m_gbuffer_normals_rt->bind(1);
        
        if (m_direct_program->set_uniform("s_WorldPos", 2))
            m_gbuffer_world_pos_rt->bind(2);
        
        // Bind uniform buffers.
        m_global_ubo->bind_base(0);

        // Render fullscreen triangle
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void indirect_lighting()
    {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        
        m_direct_light_fbo->bind();
        
        glViewport(0, 0, m_width, m_height);
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Bind shader program.
        m_indirect_program->use();
        
        if (m_indirect_program->set_uniform("s_DirectLight", 0))
            m_direct_light_rt->bind(0);
        
        if (m_indirect_program->set_uniform("s_Normals", 1))
            m_gbuffer_normals_rt->bind(1);
        
        if (m_indirect_program->set_uniform("s_WorldPos", 2))
            m_gbuffer_world_pos_rt->bind(2);
        
        if (m_indirect_program->set_uniform("s_RSMFlux", 3))
            m_rsm_flux_rt->bind(3);
        
        if (m_indirect_program->set_uniform("s_RSMNormals", 4))
            m_rsm_normals_rt->bind(4);
        
        if (m_indirect_program->set_uniform("s_RSMWorldPos", 5))
            m_rsm_world_pos_rt->bind(5);
        
        if (m_indirect_program->set_uniform("s_RSM", 6))
            m_rsm_depth_rt->bind(6);
        
        // Bind uniform buffers.
        m_global_ubo->bind_base(0);
        
        // Render fullscreen triangle
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

	// -----------------------------------------------------------------------------------------------------------------------------------

	bool load_scene()
	{
		dw::Mesh* sponza = dw::Mesh::load("cornell_box.obj");

		if (!sponza)
		{
			DW_LOG_FATAL("Failed to load mesh!");
			return false;
		}

		m_scene.push_back(sponza);

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

    void create_camera()
    {
        m_main_camera = std::make_unique<dw::Camera>(60.0f, 0.1f, CAMERA_FAR_PLANE, float(m_width) / float(m_height), glm::vec3(0.0f, 5.0f, 150.0f), glm::vec3(0.0f, 0.0, -1.0f));
        m_debug_camera = std::make_unique<dw::Camera>(60.0f, 0.1f, CAMERA_FAR_PLANE * 2.0f, float(m_width) / float(m_height), glm::vec3(0.0f, 5.0f, 150.0f), glm::vec3(0.0f, 0.0, -1.0f));
    }
	// -----------------------------------------------------------------------------------------------------------------------------------

	void render_mesh(dw::Mesh* mesh, std::unique_ptr<dw::Program>& program)
	{
		// Bind uniform buffers.
		m_object_ubo->bind_base(1);

		// Bind vertex array.
		mesh->mesh_vertex_array()->bind();

		dw::SubMesh* submeshes = mesh->sub_meshes();

		for (uint32_t i = 0; i < mesh->sub_mesh_count(); i++)
		{
			dw::SubMesh& submesh = submeshes[i];

			if (submesh.mat)
                program->set_uniform("u_Diffuse", submesh.mat->albedo_value());

			// Issue draw call.
			glDrawElementsBaseVertex(GL_TRIANGLES, submesh.index_count, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * submesh.base_index), submesh.base_vertex);
		}
	}
    
	// -----------------------------------------------------------------------------------------------------------------------------------

	void render_scene(dw::Framebuffer* fbo, std::unique_ptr<dw::Program>& program, int w, int h, GLenum cull_face)
	{
		glEnable(GL_DEPTH_TEST);
        
        if (cull_face == GL_NONE)
            glDisable(GL_CULL_FACE);
        else
        {
            glEnable(GL_CULL_FACE);
            glCullFace(cull_face);
        }

        if (fbo)
            fbo->bind();
        else
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
		glViewport(0, 0, w, h);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClearDepth(1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		// Bind shader program.
		program->use();

		// Bind uniform buffers.
		m_global_ubo->bind_base(0);

		// Draw scene.
		for (auto& mesh : m_scene)
			render_mesh(mesh, program);
	}
    
	// -----------------------------------------------------------------------------------------------------------------------------------

	void update_object_uniforms(const ObjectUniforms& transform)
	{
        void* ptr = m_object_ubo->map(GL_WRITE_ONLY);
		memcpy(ptr, &transform, sizeof(ObjectUniforms));
        m_object_ubo->unmap();
	}
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void update_global_uniforms(const GlobalUniforms& global)
    {
        void* ptr = m_global_ubo->map(GL_WRITE_ONLY);
        memcpy(ptr, &global, sizeof(GlobalUniforms));
        m_global_ubo->unmap();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void update_transforms(dw::Camera* camera)
    {
        // Update camera matrices.
		m_global_uniforms.view_proj = camera->m_projection * camera->m_view;
        m_global_uniforms.light_view_proj = m_light_proj * m_light_view;
		m_global_uniforms.cam_pos = glm::vec4(camera->m_position, 0.0f);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void update_camera()
    {
        dw::Camera* current = m_main_camera.get();
        
        if (m_debug_mode)
            current = m_debug_camera.get();
        
        float forward_delta = m_heading_speed * m_delta;
        float right_delta = m_sideways_speed * m_delta;
        
        current->set_translation_delta(current->m_forward, forward_delta);
        current->set_translation_delta(current->m_right, right_delta);
        
        m_camera_x = m_mouse_delta_x * m_camera_sensitivity;
        m_camera_y = m_mouse_delta_y * m_camera_sensitivity;
        
        if (m_mouse_look)
        {
            // Activate Mouse Look
            current->set_rotatation_delta(glm::vec3((float)(m_camera_y),
                                                    (float)(m_camera_x),
                                                    (float)(0.0f)));
        }
        else
        {
            current->set_rotatation_delta(glm::vec3((float)(0),
                                                    (float)(0),
                                                    (float)(0)));
        }
        
        current->update();
        
        update_transforms(current);
    }
    
    // -----------------------------------------------------------------------------------------------------------------------------------
    
private:
	// General GPU resources.
    std::unique_ptr<dw::Shader> m_fullscreen_triangle_vs;
	std::unique_ptr<dw::Shader> m_direct_fs;
    std::unique_ptr<dw::Shader> m_indirect_fs;
    std::unique_ptr<dw::Shader> m_rsm_vs;
    std::unique_ptr<dw::Shader> m_gbuffer_vs;
    std::unique_ptr<dw::Shader> m_gbuffer_fs;
    
    std::unique_ptr<dw::Program> m_indirect_program;
    std::unique_ptr<dw::Program> m_rsm_program;
    std::unique_ptr<dw::Program> m_gbuffer_program;
    std::unique_ptr<dw::Program> m_direct_program;
    
    std::unique_ptr<dw::Texture2D> m_gbuffer_albedo_rt;
    std::unique_ptr<dw::Texture2D> m_gbuffer_normals_rt;
    std::unique_ptr<dw::Texture2D> m_gbuffer_world_pos_rt;
    std::unique_ptr<dw::Texture2D> m_gbuffer_depth_rt;
    std::unique_ptr<dw::Texture2D> m_rsm_flux_rt;
    std::unique_ptr<dw::Texture2D> m_rsm_normals_rt;
    std::unique_ptr<dw::Texture2D> m_rsm_world_pos_rt;
    std::unique_ptr<dw::Texture2D> m_rsm_depth_rt;
    std::unique_ptr<dw::Texture2D> m_direct_light_rt;
    
    std::unique_ptr<dw::Framebuffer> m_gbuffer_fbo;
    std::unique_ptr<dw::Framebuffer> m_rsm_fbo;
    std::unique_ptr<dw::Framebuffer> m_direct_light_fbo;
    
	std::unique_ptr<dw::UniformBuffer> m_object_ubo;
    std::unique_ptr<dw::UniformBuffer> m_global_ubo;

    // Camera.
    std::unique_ptr<dw::Camera> m_main_camera;
    std::unique_ptr<dw::Camera> m_debug_camera;
    
    // Light
    glm::mat4 m_light_view;
    glm::mat4 m_light_proj;
    glm::vec3 m_light_dir;
    glm::vec3 m_light_pos;
    float     m_cone_angle;
    
	// Uniforms.
	ObjectUniforms m_object_transforms;
    GlobalUniforms m_global_uniforms;

	// Scene
	std::vector<dw::Mesh*> m_scene;

    // Camera controls.
    bool m_mouse_look = false;
    bool m_debug_mode = false;
    float m_heading_speed = 0.0f;
    float m_sideways_speed = 0.0f;
    float m_camera_sensitivity = 0.05f;
    float m_camera_speed = 0.02f;
    
	// Camera orientation.
	float m_camera_x;
	float m_camera_y;
};

DW_DECLARE_MAIN(ReflectiveShadowMaps)
