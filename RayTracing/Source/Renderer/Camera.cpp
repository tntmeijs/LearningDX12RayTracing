#include "Renderer/Camera.hpp"

using namespace DirectX::SimpleMath;

tnt::graphics::Camera::Camera()
{
	SetProjection(85.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
}

tnt::graphics::Camera::~Camera()
{
}

void tnt::graphics::Camera::SetProjection(float t_field_of_view, float t_aspect_ratio, float t_near_plane, float t_far_plane)
{
	m_projection_matrix = Matrix::CreatePerspectiveFieldOfView(t_field_of_view, t_aspect_ratio, t_near_plane, t_far_plane);
}

void tnt::graphics::Camera::SetPosition(float x, float y, float z)
{
	m_position.x = x;
	m_position.y = y;
	m_position.z = z;
}

void tnt::graphics::Camera::AddPosition(float x, float y, float z)
{
	m_position.x += x;
	m_position.y += y;
	m_position.z += z;
}

void tnt::graphics::Camera::SetTarget(float x, float y, float z)
{
	m_target.x = x;
	m_target.y = y;
	m_target.z = z;
}

void tnt::graphics::Camera::AddTarget(float x, float y, float z)
{
	m_target.x += x;
	m_target.y += y;
	m_target.z += z;
}

const DirectX::SimpleMath::Vector3 & tnt::graphics::Camera::GetPosition() const
{
	return m_position;
}

const DirectX::SimpleMath::Vector3 & tnt::graphics::Camera::GetTarget() const
{
	return m_target;
}

void tnt::graphics::Camera::CreateViewProjectionMatrix(DirectX::SimpleMath::Matrix& result) const
{
	Matrix view_matrix;

	view_matrix.CreateLookAt(m_position, m_target, Vector3(0.0f, 1.0f, 0.0f));

	result = view_matrix * m_projection_matrix;
}
