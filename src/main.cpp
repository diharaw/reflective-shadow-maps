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

struct Sphere
{
	glm::vec3 position;
	glm::vec3 color;
	float radius;
};

#define CAMERA_FAR_PLANE 10000.0f
#define VOXEL_GRID_SIZE 256

class VoxelConeTracing : public dw::Application
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

		if (!create_framebuffer())
			return false;

		// Create camera.
		create_camera();

		// Object transforms
		m_object_transforms.model = glm::mat4(1.0f);

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void update(double delta) override
	{
		// Update camera.
        update_camera();

		update_global_uniforms(m_global_uniforms);
		update_object_uniforms(m_object_transforms);

		render_scene(m_fbo, m_program);

		render_fullscreen_triangle();

		if (m_debug_mode)
			m_debug_draw.frustum(m_flythrough_camera->projection * m_flythrough_camera->view, glm::vec3(0.0f, 1.0f, 0.0f));

		// Render debug draw.
		m_debug_draw.render(nullptr, m_width, m_height, m_global_uniforms.projection * m_global_uniforms.view);
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

		create_framebuffer();
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
		settings.title = "Voxel Cone Tracing";

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

		//{
		//	// Create voxelize shaders
		//	m_voxelize_vs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_VERTEX_SHADER, "shader/voxelize_vs.glsl"));
		//	m_voxelize_gs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_GEOMETRY_SHADER, "shader/voxelize_gs.glsl"));
		//	m_voxelize_fs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_FRAGMENT_SHADER, "shader/voxelize_fs.glsl"));

		//	if (!m_voxelize_vs || !m_voxelize_gs || !m_voxelize_fs)
		//	{
		//		DW_LOG_FATAL("Failed to create Shaders");
		//		return false;
		//	}

		//	// Create general shader program
		//	dw::Shader* shaders[] = { m_voxelize_vs.get(), m_voxelize_gs.get(), m_voxelize_fs.get() };
		//	m_voxelize_program = std::make_unique<dw::Program>(3, shaders);

		//	if (!m_voxelize_program)
		//	{
		//		DW_LOG_FATAL("Failed to create Shader Program");
		//		return false;
		//	}

		//	m_voxelize_program->uniform_block_binding("u_GlobalUBO", 0);
		//	m_voxelize_program->uniform_block_binding("u_ObjectUBO", 1);
		//}

		//{
		//	// Create voxelize shaders
		//	m_voxel_render_vs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_VERTEX_SHADER, "shader/voxel_render_vs.glsl"));
		//	m_voxel_render_fs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_FRAGMENT_SHADER, "shader/voxel_render_fs.glsl"));

		//	if (!m_voxel_render_vs || !m_voxel_render_fs)
		//	{
		//		DW_LOG_FATAL("Failed to create Shaders");
		//		return false;
		//	}

		//	// Create general shader program
		//	dw::Shader* shaders[] = { m_voxel_render_vs.get(), m_voxel_render_fs.get() };
		//	m_voxel_render_program = std::make_unique<dw::Program>(2, shaders);

		//	if (!m_voxel_render_program)
		//	{
		//		DW_LOG_FATAL("Failed to create Shader Program");
		//		return false;
		//	}

		//	m_voxel_render_program->uniform_block_binding("u_GlobalUBO", 0);
		//	m_voxel_render_program->uniform_block_binding("u_ObjectUBO", 1);
		//}

		{
			// Fullscreen shaders
			m_fullscreen_vs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_VERTEX_SHADER, "shader/fullscreen_vs.glsl"));
			m_fullscreen_fs = std::unique_ptr<dw::Shader>(dw::Shader::create_from_file(GL_FRAGMENT_SHADER, "shader/fullscreen_fs.glsl"));

			if (!m_fullscreen_vs || !m_fullscreen_fs)
			{
				DW_LOG_FATAL("Failed to create Shaders");
				return false;
			}

			// Create general shader program
			dw::Shader* shaders[] = { m_fullscreen_vs.get(), m_fullscreen_fs.get() };
			m_fullscreen_program = std::make_unique<dw::Program>(2, shaders);

			if (!m_fullscreen_program)
			{
				DW_LOG_FATAL("Failed to create Shader Program");
				return false;
			}
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

	bool create_framebuffer()
	{
		if (m_fbo)
			m_fbo.reset();

		if (m_color_rt)
			m_color_rt.reset();

		if (m_depth_rt)
			m_depth_rt.reset();

		m_color_rt = std::make_unique<dw::Texture2D>(m_width, m_height, 1, 1, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
		m_color_rt->set_min_filter(GL_LINEAR);
		m_color_rt->set_wrapping(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		m_depth_rt = std::make_unique<dw::Texture2D>(m_width, m_height, 1, 1, 1, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
		m_depth_rt->set_min_filter(GL_LINEAR);

		m_fbo = std::make_unique<dw::Framebuffer>();

		m_fbo->attach_render_target(0, m_color_rt.get(), 0, 0);
		m_fbo->attach_depth_stencil_target(m_depth_rt.get(), 0, 0);
		
		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	bool initialize_voxels()
	{
		m_voxel_grid = std::make_unique<dw::Texture3D>(VOXEL_GRID_SIZE, VOXEL_GRID_SIZE, VOXEL_GRID_SIZE, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	bool load_scene()
	{
		dw::Mesh* sponza = dw::Mesh::load("sponza.obj");

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
			{
				if (program->set_uniform("s_Diffuse", 0) && submesh.mat->texture(0))
					submesh.mat->texture(0)->bind(0);
			}
			
#if defined(__EMSCRIPTEN__)

#else
			// Issue draw call.
			glDrawElementsBaseVertex(GL_TRIANGLES, submesh.index_count, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * submesh.base_index), submesh.base_vertex);
#endif
		}
	}
    
	// -----------------------------------------------------------------------------------------------------------------------------------

	void render_scene(std::unique_ptr<dw::Framebuffer>& fbo, std::unique_ptr<dw::Program>& program)
	{
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		fbo->bind();
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

	void render_fullscreen_triangle()
	{
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		m_fullscreen_program->use();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, m_width, m_height);

		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		m_fullscreen_program->set_uniform("s_Texture", 0);
		m_color_rt->bind(0);

		glDrawArrays(GL_TRIANGLES, 0, 3);
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
    std::unique_ptr<dw::Shader> m_vs;
	std::unique_ptr<dw::Shader> m_fs;
	std::unique_ptr<dw::Program> m_program;

	std::unique_ptr<dw::Shader> m_fullscreen_vs;
	std::unique_ptr<dw::Shader> m_fullscreen_fs;
	std::unique_ptr<dw::Program> m_fullscreen_program;

	std::unique_ptr<dw::Shader>  m_voxelize_vs;
	std::unique_ptr<dw::Shader>  m_voxelize_gs;
	std::unique_ptr<dw::Shader>  m_voxelize_fs;
	std::unique_ptr<dw::Program> m_voxelize_program;

	std::unique_ptr<dw::Shader>  m_voxel_render_vs;
	std::unique_ptr<dw::Shader>  m_voxel_render_fs;
	std::unique_ptr<dw::Program> m_voxel_render_program;

	std::unique_ptr<dw::UniformBuffer> m_object_ubo;
    std::unique_ptr<dw::UniformBuffer> m_global_ubo;

	std::unique_ptr<dw::Texture2D> m_color_rt;
	std::unique_ptr<dw::Texture2D> m_depth_rt;
	std::unique_ptr<dw::Framebuffer> m_fbo;

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
    float m_camera_speed = 0.2f;

	// Voxel Grid
	std::unique_ptr<dw::Texture3D> m_voxel_grid;

	// Camera orientation.
	float m_camera_x;
	float m_camera_y;
	float m_springness = 1.0f;
};

DW_DECLARE_MAIN(VoxelConeTracing)
