#include "camera.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  Camera::Camera() :
    position_(0.0f, 0.0f, 0.0f),
    rotation_(0.0f, 0.0f, 0.0f),
    view_(DirectX::XMMatrixIdentity()),
    projection_(DirectX::XMMatrixIdentity()),
    view_dirty_(true),
    projection_dirty_(true),
    near_plane_(1.0f),
    far_plane_(1000.0f),
    aperture_(0.1f),
    translation_acculumator_(0.0f, 0.0f, 0.0f),
    fov_in_radians_(DirectX::XM_PIDIV2)
  {

  }

  //------------------------------------------------------------------------------------------------------
  Camera::~Camera()
  {

  }

  //------------------------------------------------------------------------------------------------------
  void Camera::Translate(const DirectX::XMFLOAT3& translation)
  {
    view_dirty_ = true;
    translation_acculumator_.x += translation.x;
    translation_acculumator_.y += translation.y;
    translation_acculumator_.z += translation.z;
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::Rotate(const DirectX::XMFLOAT3& rotation)
  {
    view_dirty_ = true;
    rotation_.x += rotation.x;
    rotation_.y += rotation.y;
    rotation_.z += rotation.z;
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::SetPosition(const DirectX::XMFLOAT3& position)
  {
    view_dirty_ = true;
    position_ = position;
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::SetRotation(const DirectX::XMFLOAT3& rotation)
  {
    view_dirty_ = true;
    rotation_ = rotation;
  }

  //------------------------------------------------------------------------------------------------------
  const DirectX::XMFLOAT3& Camera::GetPosition() const
  {
    return position_;
  }

  //------------------------------------------------------------------------------------------------------
  const DirectX::XMFLOAT3& Camera::GetRotation() const
  {
    return rotation_;
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::SetNearPlane(float near_plane)
  {
    projection_dirty_ = true;
    near_plane_ = near_plane;
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::SetFarPlane(float far_plane)
  {
    projection_dirty_ = true;
    far_plane_ = far_plane;
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::SetAperture(float aperture)
  {
    aperture_ = aperture;
  }

  //------------------------------------------------------------------------------------------------------
  float Camera::GetNearPlane() const
  {
    return near_plane_;
  }

  //------------------------------------------------------------------------------------------------------
  float Camera::GetFarPlane() const
  {
    return far_plane_;
  }

  //------------------------------------------------------------------------------------------------------
  float Camera::GetAperture() const
  {
    return aperture_;
  }

  //------------------------------------------------------------------------------------------------------
  const DirectX::XMMATRIX& Camera::GetViewMatrix()
  {
    if (view_dirty_ == true)
    {
      UpdateViewMatrix();
    }

    return view_;
  }

  //------------------------------------------------------------------------------------------------------
  const DirectX::XMMATRIX& Camera::GetProjectionMatrix()
  {
    if (projection_dirty_ == true)
    {
      UpdateProjectionMatrix();
    }

    return projection_;
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::UpdateViewMatrix()
  {
    static const DirectX::XMVECTOR camera_forward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    static const DirectX::XMVECTOR camera_right = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX rotation_matrix = DirectX::XMMatrixRotationRollPitchYaw(rotation_.x, rotation_.y, rotation_.z);

    DirectX::XMVECTOR rotated_forward = DirectX::XMVector3TransformCoord(camera_forward, rotation_matrix);
    DirectX::XMVECTOR rotated_right = DirectX::XMVector3TransformCoord(camera_right, rotation_matrix);
    DirectX::XMVECTOR rotated_up = DirectX::XMVector3Cross(rotated_forward, rotated_right);

    DirectX::XMVECTOR target = DirectX::XMVector3Normalize(rotated_forward);

    DirectX::XMVECTOR position_vector = DirectX::XMVectorSet(position_.x, position_.y, position_.z, 1.0f);

    position_vector = DirectX::XMVectorAdd(DirectX::XMVectorScale(rotated_forward, translation_acculumator_.z), position_vector);
    position_vector = DirectX::XMVectorAdd(DirectX::XMVectorScale(rotated_right, translation_acculumator_.x), position_vector);
    position_vector = DirectX::XMVectorAdd(DirectX::XMVectorScale(rotated_up, translation_acculumator_.y), position_vector);

    target = DirectX::XMVectorAdd(target, position_vector);

    view_ = DirectX::XMMatrixLookAtLH(
      position_vector,
      target,
      rotated_up
    );

    translation_acculumator_ = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    DirectX::XMStoreFloat3(&position_, position_vector);
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::SetFovRadians(float fov_in_radians)
  {
    projection_dirty_ = true;
    fov_in_radians_ = fov_in_radians;
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::SetFovDegrees(float fov_in_degrees)
  {
    projection_dirty_ = true;
    fov_in_radians_ = DirectX::XMConvertToRadians(fov_in_degrees);
  }

  //------------------------------------------------------------------------------------------------------
  float Camera::GetFovRadians() const
  {
    return fov_in_radians_;
  }

  //------------------------------------------------------------------------------------------------------
  float Camera::GetFovDegrees() const
  {
    return DirectX::XMConvertToDegrees(fov_in_radians_);
  }

  //------------------------------------------------------------------------------------------------------
  void Camera::UpdateProjectionMatrix()
  {
    projection_ = DirectX::XMMatrixPerspectiveFovLH(
      fov_in_radians_,
      1280.0f / 720.0f,
      near_plane_,
      far_plane_
    );
  }
}