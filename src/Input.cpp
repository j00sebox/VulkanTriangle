#include "config.hpp"
#include "Input.hpp"

GLFWwindow* Input::m_window_handle = nullptr;

bool Input::is_key_pressed(int key_code)
{
	auto state = glfwGetKey(m_window_handle, key_code);
	return (state == GLFW_PRESS) || (state == GLFW_REPEAT);
}

bool Input::is_button_pressed(int button_code)
{
	auto state = glfwGetMouseButton(m_window_handle, button_code);
	return (state == GLFW_PRESS) || (state == GLFW_REPEAT);
}

void Input::set_mouse_pos(int x, int y)
{
	glfwSetCursorPos(m_window_handle, x, y);
}

std::pair<float, float> Input::get_mouse_pos()
{
	double xPos, yPos;
	glfwGetCursorPos(m_window_handle, &xPos, &yPos);

	return { (float)xPos, (float)yPos };
}

void Input::show_cursor(bool show)
{
	if(show)
    {
        glfwSetInputMode(m_window_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
	else
    {
        glfwSetInputMode(m_window_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}
