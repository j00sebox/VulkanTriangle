#include "config.hpp"
#include "Camera.hpp"
#include "Input.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
// #include <imgui.h>

glm::mat4 Camera::camera_look_at()
{
	return glm::lookAt(m_position, m_position + m_forward, m_up);
}

void Camera::resize(int width, int height)
{
	m_screen_width = width; m_screen_height = height;

	float aspect_ratio = (float)m_screen_width / (float)m_screen_height;
    float fov_y = atanf(tanf(glm::radians(m_fov/2)) / aspect_ratio) * 2;

	// create projection matrices
    m_perspective = glm::perspective(fov_y, aspect_ratio, m_near, m_far);

	m_orthographic = glm::mat4(
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f / (m_far - 0.1f), -m_near / (m_far - m_near),
		0.f, 0.f, 0.f, 1.f
	);
}

bool Camera::update(float elapsed_time)
{
	// block camera update if imgui menu is in use
	//if (!ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive())
	{
		if (Input::is_key_pressed(GLFW_KEY_W))
		{
			move_forward(m_speed * elapsed_time);
		}

		if (Input::is_key_pressed(GLFW_KEY_S))
		{
			move_forward(-m_speed * elapsed_time);
		}

		if (Input::is_key_pressed(GLFW_KEY_A))
		{
			move_right(-m_speed * elapsed_time);
		}

		if (Input::is_key_pressed(GLFW_KEY_D))
		{
			move_right(m_speed * elapsed_time);
		}

		if (Input::is_key_pressed(GLFW_KEY_R))
		{
			reset();
		}

		if (Input::is_button_pressed(GLFW_MOUSE_BUTTON_2))
		{
			if (!m_mouse_down)
			{
				Input::show_cursor(false);
				Input::set_mouse_pos((m_screen_width / 2), (m_screen_height / 2));

				m_mouse_down = true;
			}

			auto [x, y] = Input::get_mouse_pos();

			float rotX = elapsed_time * m_sensitivity * (y - (float)(m_screen_height / 2)) / (float)m_screen_width;
			float rotY = elapsed_time * m_sensitivity * (x - (float)(m_screen_width / 2)) / (float)m_screen_height;

			// calculate vertical orientation adjustment
			glm::vec3 new_forward = glm::rotate(m_forward, glm::radians(-rotX), m_right);

			// prevents barrel rolls
			if (std::abs(glm::angle(new_forward, m_up) - glm::radians(90.0f)) <= glm::radians(85.0f))
			{
				m_forward = new_forward;
			}

			// get horizontal orientation adjustment and new right vector
			m_forward = glm::rotate(m_forward, glm::radians(-rotY), m_up);
			m_right = glm::normalize(glm::cross(m_forward, m_up));

			// keep mouse in the center
			Input::set_mouse_pos((m_screen_width / 2), (m_screen_height / 2));
		}
		else
		{
			m_mouse_down = false;

			Input::show_cursor(true);
		}

        return true;
	}

    return false;
}

void Camera::move_forward(float f)
{
	m_position = m_position + (m_forward * f);
}

void Camera::move_right(float r)
{
	m_position = m_position + (m_right * r);
}

void Camera::set_pos(glm::vec3&& pos)
{
    m_position = pos;
}

void Camera::set_forward(glm::vec3 &&fwd)
{
    m_forward = glm::normalize(fwd);
    m_right = glm::normalize(glm::cross(m_forward, m_up));
}

const glm::vec3& Camera::get_pos() const
{
	return m_position;
}

void Camera::reset()
{
	m_forward = { 0.f, 0.f, 1.f };
	m_up = { 0.f, 1.f, 0.f };
	m_right = { 1.f, 0.f, 0.f };

	m_position = { 0.f, 0.f, 0.f };
}


