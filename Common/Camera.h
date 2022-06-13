#ifndef CAMERA_H
#define CAMERA_H

#include "d3dUtil.h"

/**
 * \brief Simple first person style camera class that lets the viewer explore the 3D scene.
 *  It keeps track of the camera coordinate system relative to the world space so that the view matrix can be constructed.
 *  It keeps track of the viewing frustum of the camera so that the projection matrix can be obtained.
 */
class Camera
{
public:
	Camera();
	~Camera();

	// Notice that we provide XMVECTOR return variations for many of the "get" method; this is just for convenience so that the client code doesn't need to convert if they need an XMVECTOR

	// Get/Set world camera position.
	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMFLOAT3 GetPosition3f() const;
	void              SetPosition(float x, float y, float z);
	void              SetPosition(const DirectX::XMFLOAT3& v);

	// Get camera basis vectors.
	DirectX::XMVECTOR GetRight() const;
	DirectX::XMFLOAT3 GetRight3f() const;
	DirectX::XMVECTOR GetUp() const;
	DirectX::XMFLOAT3 GetUp3f() const;
	DirectX::XMVECTOR GetLook() const;
	DirectX::XMFLOAT3 GetLook3f() const;

	// Get frustum properties.
	float GetNearZ() const;
	float GetFarZ() const;
	float GetAspect() const;
	float GetFovY() const; // get the cached vertical FOV
	float GetFovX() const; // compute and get the horizontal FOV

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth() const;
	float GetNearWindowHeight() const;
	float GetFarWindowWidth() const;
	float GetFarWindowHeight() const;

	// Set frustum. Think of the frustum as the lens of our camera
	void SetLens(float fovY, float aspect, float zn, float zf);

	// Define camera space via LookAt parameters.
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);              // VECTOR version
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up); // FLOAT3 version

	// Get View/Proj matrices.
	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProj() const;

	DirectX::XMFLOAT4X4 GetView4x4f() const;
	DirectX::XMFLOAT4X4 GetProj4x4f() const;

	// Strafe/Walk the camera a distance d.
	void Strafe(float d);
	void Walk(float d);

	// Rotate the camera.
	void Pitch(float angle);
	void RotateY(float angle);

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();

private:
	// The data of the camera class stores 2 key pieces of information:
	// 1. The position, right, up, and look vectors defining the origin, x,y,z axis of the view space coordinates system in world coordinates
	// 2. The properties of the frustum (the lens of the camera, as FOV, near, and far plane, etc)

	// Camera coordinate system with coordinates relative to world space.
	DirectX::XMFLOAT3 mPosition = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 mRight    = {1.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 mUp       = {0.0f, 1.0f, 0.0f};
	DirectX::XMFLOAT3 mLook     = {0.0f, 0.0f, 1.0f};

	// Cache frustum properties.
	float mNearZ            = 0.0f;
	float mFarZ             = 0.0f;
	float mAspect           = 0.0f;
	float mFovY             = 0.0f;
	float mNearWindowHeight = 0.0f; // height of the near plane
	float mFarWindowHeight  = 0.0f; // height of the far plane

	bool mViewDirty = true; // signal that our view data is outdated

	// Cache View/Proj matrices.
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};

#endif // CAMERA_H
