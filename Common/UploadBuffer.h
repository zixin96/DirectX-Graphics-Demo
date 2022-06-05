#pragma once

#include "d3dUtil.h"

/**
 * \brief A light wrapper around upload buffer (e.g. constant buffer)
 * \tparam T Data type in the upload buffer
 */
template <typename T>
class UploadBuffer
{
	public:
		UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) :
			mIsConstantBuffer(isConstantBuffer)
		{
			mElementByteSize = sizeof(T);

			// Constant buffer elements need to be multiples of 256 bytes.
			// This is because the hardware can only view constant data 
			// at m*256 byte offsets and of n*256 byte lengths. 
			// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
			// UINT64 OffsetInBytes; // multiple of 256
			// UINT   SizeInBytes;   // multiple of 256
			// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
			if (isConstantBuffer)
				mElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));

			ThrowIfFailed(device->CreateCommittedResource(
				              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // Upload heap. This is where we commit resources where we need to upload data from the CPU to the GPU resource
				              D3D12_HEAP_FLAG_NONE,
				              &CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize*elementCount),
				              D3D12_RESOURCE_STATE_GENERIC_READ,
				              nullptr,
				              IID_PPV_ARGS(&mUploadBuffer)));

			// obtain a pointer to the resource data
			ThrowIfFailed(mUploadBuffer->Map(
				              0,                                        // a subresource index identifying the subresource to map. For a buffer, the only subresource is the buffer itself, so we just set this to 0
				              nullptr,                                  // an optional pointer to a D3D12_RANGE structure that describes the range of memory to map; specifying null maps the entire resource
				              reinterpret_cast<void**>(&mMappedData))); // a pointer to the mapped data

			// We do not need to unmap until we are done with the resource.  However, we must not write to
			// the resource while it is in use by the GPU (so we must use synchronization techniques).
		}

		UploadBuffer(const UploadBuffer& rhs)            = delete;
		UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

		~UploadBuffer()
		{
			if (mUploadBuffer != nullptr)
				mUploadBuffer->Unmap(0,        // 0 for buffer
				                     nullptr); // unmap the entire resource

			mMappedData = nullptr;
		}

		ID3D12Resource* Resource() const
		{
			return mUploadBuffer.Get();
		}

		/**
		 * \brief Upload buffer may contains data for multiple elements. Update a particular element in the buffer.
		 * Use it when we need to change the contents of an upload buffer from the CPU
		 * \param elementIndex The index of the element to update
		 * \param data Update data
		 */
		void CopyData(int elementIndex, const T& data)
		{
			memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
		BYTE*                                  mMappedData = nullptr;

		UINT mElementByteSize  = 0;
		bool mIsConstantBuffer = false; // we need to know if this upload buffer is a constant buffer b/c constant buffer has 256 bytes size requirement
};
