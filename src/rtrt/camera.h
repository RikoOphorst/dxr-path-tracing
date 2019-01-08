#pragma once

namespace rtrt
{
  class Camera
  {
  public:
    Camera();
    ~Camera();

    void Translate(const DirectX::XMFLOAT3& translation);

    void Rotate(const DirectX::XMFLOAT3& rotation);

    void SetPosition(const DirectX::XMFLOAT3& position);

    void SetRotation(const DirectX::XMFLOAT3& rotation);

    const DirectX::XMFLOAT3& GetPosition() const;
    const DirectX::XMFLOAT3& GetRotation() const;

    void SetNearPlane(float near_plane);
    void SetFarPlane(float far_plane);
    void SetAperture(float aperture);

    float GetNearPlane() const;
    float GetFarPlane() const;
    float GetAperture() const;

    const DirectX::XMMATRIX& GetViewMatrix();

    const DirectX::XMMATRIX& GetProjectionMatrix();

    void SetFovRadians(float fov_in_radians);
    void SetFovDegrees(float fov_in_degrees);

    float GetFovRadians() const;
    float GetFovDegrees() const;

  protected:
    void UpdateViewMatrix();

    void UpdateProjectionMatrix();

  protected:
    DirectX::XMFLOAT3 position_;
    DirectX::XMFLOAT3 rotation_;

    DirectX::XMMATRIX view_;
    DirectX::XMMATRIX projection_;
    bool view_dirty_;
    bool projection_dirty_;

    float near_plane_;
    float far_plane_;
    float aperture_;

    float fov_in_radians_;

  private:
    DirectX::XMFLOAT3 translation_acculumator_;
  };
}