#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <SimpleMath.h>

namespace tnt
{
	namespace graphics
	{
		class Camera
		{
		public:
			Camera();
			~Camera();

			void SetProjection(float t_field_of_view, float t_aspect_ratio, float t_near_plane, float t_far_plane);

			void SetPosition(float x, float y, float z);
			void AddPosition(float x, float y, float z);

			void SetTarget(float x, float y, float z);
			void AddTarget(float x, float y, float z);

			const DirectX::SimpleMath::Vector3& GetPosition() const;
			const DirectX::SimpleMath::Vector3& GetTarget() const;

			void CreateViewProjectionMatrix(DirectX::SimpleMath::Matrix& result) const;

		private:
			DirectX::SimpleMath::Vector3 m_position;
			DirectX::SimpleMath::Vector3 m_target;

			DirectX::SimpleMath::Matrix m_projection_matrix;
		};
	}
}

#endif