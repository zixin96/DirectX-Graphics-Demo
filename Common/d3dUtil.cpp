#include "d3dUtil.h"
#include <comdef.h>
#include <fstream>

using Microsoft::WRL::ComPtr;

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
	ErrorCode(hr),
	FunctionName(functionName),
	Filename(filename),
	LineNumber(lineNumber)
{
}

bool d3dUtil::IsKeyDown(int vkeyCode)
{
	return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0; // GetAsyncKeyState returns a 16-bit signed integer. The highest bit of the returned value represents whether the key is currently being held down: https://stackoverflow.com/questions/64901061/what-is-the-difference-between-using-getasynckeystate-by-checking-with-its-ret
}

/**
 * \brief Load binary data from files.
 * This method can be used to load the compiled shader object bytecode from the .cso files.
 * \param filename Name of the file
 * \return D3D binary blob object
 */
ComPtr<ID3DBlob> d3dUtil::LoadBinary(const std::wstring& filename)
{
	std::ifstream fin(filename, std::ios::binary);

	fin.seekg(0, std::ios_base::end);
	std::ifstream::pos_type size = (int)fin.tellg();
	fin.seekg(0, std::ios_base::beg);

	// create D3D buffer for the binary data
	ComPtr<ID3DBlob> blob;
	ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

	fin.read((char*)blob->GetBufferPointer(), size);
	fin.close();

	return blob;
}

ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(
	ID3D12Device*              device,
	ID3D12GraphicsCommandList* cmdList,
	const void*                initData,
	UINT64                     byteSize,
	ComPtr<ID3D12Resource>&    uploadBuffer)
{
	ComPtr<ID3D12Resource> defaultBuffer;

	// Create the actual default buffer resource.
	ThrowIfFailed(device->CreateCommittedResource(
		              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // A pointer to a D3D12_HEAP_PROPERTIES structure that provides properties for the resource's heap
		              D3D12_HEAP_FLAG_NONE,                              // Heap option
		              &CD3DX12_RESOURCE_DESC::Buffer(byteSize),          // A pointer to a D3D12_RESOURCE_DESC structure that describes the resource
		              D3D12_RESOURCE_STATE_COMMON,                       // The initial state of the resource
		              nullptr,                                           // Specifies a D3D12_CLEAR_VALUE structure that describes the default value for a clear color
		              IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	ThrowIfFailed(device->CreateCommittedResource(
		              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		              D3D12_HEAP_FLAG_NONE,
		              &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		              D3D12_RESOURCE_STATE_GENERIC_READ, // this is the required starting state for an upload heap
		              nullptr,
		              IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData                  = initData;
	subResourceData.RowPitch               = byteSize;
	subResourceData.SlicePitch             = subResourceData.RowPitch;

	// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
	// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
	// the intermediate upload heap data will be copied to mBuffer.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
	                                                                  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(cmdList,
	                      defaultBuffer.Get(),
	                      uploadBuffer.Get(),
	                      0,
	                      0,
	                      1,
	                      &subResourceData);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
	                                                                  D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.
	// The caller can Release the uploadBuffer after it knows the copy has been executed.


	return defaultBuffer;
}

ComPtr<ID3DBlob> d3dUtil::CompileShader(
	const std::wstring&     filename,
	const D3D_SHADER_MACRO* defines,
	const std::string&      entrypoint,
	const std::string&      target)
{
	// use debug flags in debug mode
	UINT compileFlags = 0;
	#if defined(DEBUG) || defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
	                        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

	if (errors != nullptr)
		OutputDebugStringA((char*)errors->GetBufferPointer());

	ThrowIfFailed(hr);

	return byteCode;
}

std::wstring DxException::ToString() const
{
	// Get the string description of the error code.
	_com_error   err(ErrorCode);
	std::wstring msg = err.ErrorMessage();

	return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}
