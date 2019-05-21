#include <application.h>
#include <mesh.h>
#include <camera.h>
#include <material.h>
#include <memory>
#include <iostream>
#include <stack>
#include <random>
#include <chrono>
#include "camera.h"

#define CAMERA_FAR_PLANE 1000.0f

// Uniform buffer data structure.
struct ObjectUniforms
{
	DW_ALIGNED(16) glm::mat4 model;
};

struct GlobalUniforms
{
    DW_ALIGNED(16) glm::mat4 view;
    DW_ALIGNED(16) glm::mat4 projection;
	DW_ALIGNED(16) glm::mat4 inv_view;
	DW_ALIGNED(16) glm::mat4 inv_projection;
	DW_ALIGNED(16) glm::mat4 inv_view_projection;
	DW_ALIGNED(16) glm::vec4 view_pos;
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

		render_scene(nullptr, m_program);

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
		ProjectionInfo info;

		info.type = PROJ_PERSPECTIVE;
		info.perspective.n = 0.1f;
		info.perspective.f = CAMERA_FAR_PLANE;
		info.perspective.fov = 60.0f;
		info.perspective.aspect_ratio = float(m_width) / float(m_height);

		m_flythrough_camera->set_projection(info);
		m_debug_camera->set_projection(info);
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
            m_sideways_speed = m_camera_speed;
        else if(code == GLFW_KEY_D)
            m_sideways_speed = -m_camera_speed;

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

	bool create_shaders()
	{
		{
			// Create general shaders
			m_vs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_VERTEX_SHADER, "shader/scene_vs.glsl"));
			m_fs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_FRAGMENT_SHADER, "shader/scene_fs.glsl"));

			if (!m_vs || !m_fs)
			{
				DW_LOG_FATAL("Failed to create Shaders");
				return false;
			}

			// Create general shader program
			dw::Shader* shaders[] = { m_vs.get(), m_fs.get() };
			m_program = std::make_unique<dw::Program>(2, shaders);

			if (!m_program)
			{
				DW_LOG_FATAL("Failed to create Shader Program");
				return false;
			}

			m_program->uniform_block_binding("u_GlobalUBO", 0);
			m_program->uniform_block_binding("u_ObjectUBO", 1);
		}

		return true;
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
		ProjectionInfo info;

		info.type = PROJ_PERSPECTIVE;
		info.perspective.n = 0.1f;
		info.perspective.f = CAMERA_FAR_PLANE;
		info.perspective.fov = 60.0f;
		info.perspective.aspect_ratio = float(m_width) / float(m_height);

		m_flythrough_camera = std::make_unique<FlythroughCamera>(glm::vec3(0.0f), 0.1f, 90.0f, -90.0f, info);
		m_debug_camera = std::make_unique<FlythroughCamera>(glm::vec3(0.0f), 0.1f, 90.0f, -90.0f, info);
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

	void render_scene(std::unique_ptr<dw::Framebuffer> fbo, std::unique_ptr<dw::Program>& program)
	{
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

        if (fbo)
            fbo->bind();
        else
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
		glViewport(0, 0, m_width, m_height);

		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
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
    
    void update_transforms(Camera* camera)
    {
        // Update camera matrices.
		m_global_uniforms.view = camera->view;
        m_global_uniforms.projection = camera->projection;
		
		glm::mat4 cubemap_view = glm::mat4(glm::mat3(m_global_uniforms.view));
		glm::mat4 view_proj = m_global_uniforms.projection * cubemap_view;

		m_global_uniforms.inv_view = glm::inverse(cubemap_view);
		m_global_uniforms.inv_projection = glm::inverse(m_global_uniforms.projection);
		m_global_uniforms.inv_view_projection = glm::inverse(view_proj);
		m_global_uniforms.view_pos = glm::vec4(camera->transform.position, 0.0f);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------
    
    void update_camera()
    {
		FlythroughCamera* camera = m_flythrough_camera.get();

		if (m_debug_mode)
			camera = m_debug_camera.get();

        float forward_delta = m_heading_speed * m_delta;
        float right_delta = m_sideways_speed * m_delta;
        
		if (forward_delta != 0.0f)
			camera->move_forwards(forward_delta);

		if (right_delta != 0.0f)
			camera->move_sideways(right_delta);

		m_camera_x = m_mouse_delta_x * m_camera_sensitivity;
		m_camera_y = m_mouse_delta_y * m_camera_sensitivity;

		float handedness = 1.0f;

#ifndef GLM_FORCE_LEFT_HANDED
		handedness = -1.0f;
#endif
        
        if (m_mouse_look)
			camera->rotate(glm::vec3((float)(handedness * m_camera_y), (float)(handedness * m_camera_x), (float)(0.0f)));
  
		update_transforms(camera);

		camera->update(m_delta);
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
	std::unique_ptr<FlythroughCamera> m_flythrough_camera;
	std::unique_ptr<FlythroughCamera> m_debug_camera;
    
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
