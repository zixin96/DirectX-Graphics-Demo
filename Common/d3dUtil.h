// General helper code.

#pragma once

#include <Windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include "d3dx12.h"
#include "DDSTextureLoader12.h"
#include "MathHelper.h"

extern const int gNumFrameResources;

inline void d3dSetDebugName(IDXGIObject* obj, const char* name)
{
	if (obj)
	{
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
	}
}

inline void d3dSetDebugName(ID3D12Device* obj, const char* name)
{
	if (obj)
	{
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
	}
}

inline void d3dSetDebugName(ID3D12DeviceChild* obj, const char* name)
{
	if (obj)
	{
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
	}
}

/**
 * \brief Converts a string to a wstring (used in ThrowIfFailed macro)
 * \param str String to be converted
 * \return the resulting wstring
 */
inline std::wstring AnsiToWString(const std::string& str)
{
	WCHAR buffer[512];                                            // output wide character string buffer
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512); // Maps a character string to a UTF-16 (wide character) string: https://docs.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
	return std::wstring(buffer);
}

/*
#if defined(_DEBUG)
    #ifndef Assert
    #define Assert(x, description)                                  \
    {                                                               \
        static bool ignoreAssert = false;                           \
        if(!ignoreAssert && !(x))                                   \
        {                                                           \
            Debug::AssertResult result = Debug::ShowAssertDialog(   \
            (L#x), description, AnsiToWString(__FILE__), __LINE__); \
        if(result == Debug::AssertIgnore)                           \
        {                                                           \
            ignoreAssert = true;                                    \
        }                                                           \
                    else if(result == Debug::AssertBreak)           \
        {                                                           \
            __debugbreak();                                         \
        }                                                           \
        }                                                           \
    }
    #endif
#else
    #ifndef Assert
    #define Assert(x, description) 
    #endif
#endif 		
    */

/**
 * \brief Utilities class that contains useful D3D helper functions
 */
class d3dUtil
{
public:
	static bool IsKeyDown(int vkeyCode);

	/**
	 * \brief Rounds the byte size of the buffer to be a multiple of the minimum hardware allocation size (256 bytes). 
	 * \param byteSize The given byte size of the buffer
	 * \return The rounded byte size of the buffer
	 */
	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		// Constant buffers must be a multiple of the minimum hardware
		// allocation size (usually 256 bytes).  So round up to nearest
		// multiple of 256.  We do this by adding 255 and then masking off
		// the lower 2 bytes which store all bits < 256.
		// Example: Suppose byteSize = 300.
		// (300 + 255) & ~255
		// 555 & ~255
		// 0x022B & ~0x00ff
		// 0x022B & 0xff00
		// 0x0200
		// 512
		return (byteSize + 255) & ~255;
	}

	static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device*                           device,
		ID3D12GraphicsCommandList*              cmdList,
		const void*                             initData,
		UINT64                                  byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBufferUAV(
		ID3D12Device*                           device,
		ID3D12GraphicsCommandList*              cmdList,
		const void*                             initData,
		UINT64                                  byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTexture(ID3D12Device*                           device,
	                                                            ID3D12GraphicsCommandList*              cmdList,
	                                                            const wchar_t*                          fileName,
	                                                            Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring&     filename,
		const D3D_SHADER_MACRO* defines,
		const std::string&      entrypoint,
		const std::string&      target);
};

class DxException
{
public:
	DxException() = default;
	DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

	std::wstring ToString() const;

	HRESULT      ErrorCode = S_OK;
	std::wstring FunctionName;
	std::wstring Filename;
	int          LineNumber = -1;
};

// Defines a sub-range of geometry in a MeshGeometry.  This is for when multiple
// geometries are stored in one vertex and index buffer.  It provides the offsets
// and data needed to draw a subset of geometry stores in the vertex and index 
// buffers so that we can implement the technique described by Figure 6.3, page 215
//! Think of SubmeshGeometry as a blueprint for each drawable object
struct SubmeshGeometry
{
	UINT IndexCount         = 0;
	UINT StartIndexLocation = 0;
	INT  BaseVertexLocation = 0;

	// Bounding box of the geometry defined by this submesh. 
	DirectX::BoundingBox Bounds;
};

//!? For Chapter 6, Exercise 2: we need to create a new struct representing mesh geometry that has two vertex buffer views
struct MeshGeometryTwoBuffers
{
	std::string Name;

	Microsoft::WRL::ComPtr<ID3DBlob>       VertexPosBufferCPU      = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexPosBufferGPU      = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexPosBufferUploader = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob>       VertexColorBufferCPU      = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexColorBufferGPU      = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexColorBufferUploader = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob>       IndexBufferCPU      = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU      = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	UINT VertexPosByteStride     = 0;
	UINT VertexPosBufferByteSize = 0;

	UINT VertexColorByteStride     = 0;
	UINT VertexColorBufferByteSize = 0;

	DXGI_FORMAT IndexFormat         = DXGI_FORMAT_R16_UINT;
	UINT        IndexBufferByteSize = 0;

	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexPosBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexPosBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes  = VertexPosByteStride;
		vbv.SizeInBytes    = VertexPosBufferByteSize;

		return vbv;
	}

	D3D12_VERTEX_BUFFER_VIEW VertexColorBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexColorBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes  = VertexColorByteStride;
		vbv.SizeInBytes    = VertexColorBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format         = IndexFormat;
		ibv.SizeInBytes    = IndexBufferByteSize;

		return ibv;
	}

	void DisposeUploaders()
	{
		VertexPosBufferUploader   = nullptr;
		VertexColorBufferUploader = nullptr;
		IndexBufferUploader       = nullptr;
	}
};

/**
 * \brief A structure that groups a vertex and index buffer together to define a group of geometry
 * We say "a group" because one vertex/index buffer can contain multiple geometry. See page 215
 */
struct MeshGeometry
{
	// Give it a name so we can look it up by name.
	std::string Name;

	// keep a system memory backing of the vertex and index data so that it can be read by the CPU
	// The CPU needs access to the geometry data for things like picking and collision detection. 
	// Use Blobs because the vertex/index format can be generic. It is up to the client to cast appropriately.
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU  = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU  = nullptr;

	// we must cache intermediate upload buffers b/c they have to be kept alive until the command list has finished executing the copy command
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader  = nullptr;

	// caches the important properties of the vertex and index buffers (needed when creating vertex and index buffer view): 
	UINT        VertexByteStride     = 0;
	UINT        VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat          = DXGI_FORMAT_R16_UINT;
	UINT        IndexBufferByteSize  = 0;

	// A MeshGeometry may store multiple geometries in one vertex/index buffer.
	// Use this container to define the Submesh geometries so we can draw the Submeshes individually.
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	// Also, provide functions that return view to the buffers:

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes  = VertexByteStride;
		vbv.SizeInBytes    = VertexBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format         = IndexFormat;
		ibv.SizeInBytes    = IndexBufferByteSize;

		return ibv;
	}

	// We can free this memory after we finish upload to the GPU.
	void DisposeUploaders()
	{
		VertexBufferUploader = nullptr;
		IndexBufferUploader  = nullptr;
	}
};

/**
 * \brief A directional, point, or spot light
 * Matches Light struct in LightingUtil.hlsl
 * Note: the order of data members follow HLSL packing rules
 */
struct Light
{
	DirectX::XMFLOAT3 Strength     = {0.5f, 0.5f, 0.5f};
	float             FalloffStart = 1.0f;                // point/spot light only
	DirectX::XMFLOAT3 Direction    = {0.0f, -1.0f, 0.0f}; // directional/spot light only. Direction the light ray travels
	float             FalloffEnd   = 10.0f;               // point/spot light only
	DirectX::XMFLOAT3 Position     = {0.0f, 0.0f, 0.0f};  // point/spot light only
	float             SpotPower    = 64.0f;               // spot light only
};

#define MaxLights 16

/**
 * \brief MaterialConstants contains a subset of Material data.
 * It just contains the data the shaders need for rendering.
 * Note: the order of data members follow HLSL packing rules
 */
struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
	DirectX::XMFLOAT3 FresnelR0     = {0.01f, 0.01f, 0.01f};
	float             Roughness     = 0.25f;
	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

// Simple struct to represent a material for our demos.  A production 3D engine
// would likely create a class hierarchy of Materials.
struct Material
{
	// Unique material name for lookup.
	std::string Name;

	// each material object has an index that specifies where its constant data is in the material constant buffer
	int MatCBIndex = -1;

	// Index into SRV heap for diffuse texture.
	int DiffuseSrvHeapIndex = -1; //! where to find the diffuse texture descriptor for this material in the SRV heap? 

	// Index into SRV heap for normal texture.
	int NormalSrvHeapIndex = -1; //! where to find the normal texture descriptor for this material in the SRV heap? 

	// Dirty flag indicating the material has changed and we need to update the constant buffer.
	// Because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify a material we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Material constant buffer data used for shading.
	DirectX::XMFLOAT4   DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f}; // when we use diffuse map, this value is used to tweak the diffuse texture without having to author a new texture
	DirectX::XMFLOAT3   FresnelR0     = {0.01f, 0.01f, 0.01f};
	float               Roughness     = .25f; // in normalized [0,1] range. 0: perfectly smooth. 1: the roughest surface possible. Used to derive m (Eq. 8.4, page 333) in the shader code. 
	DirectX::XMFLOAT4X4 MatTransform  = MathHelper::Identity4x4();
};

struct Texture
{
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif
