//***************************************************************************************
// BlurFilter.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "BlurFilter.h"

BlurFilter::BlurFilter(ID3D12Device* device,
                       UINT          width,
                       UINT          height,
                       DXGI_FORMAT   format)
{
	md3dDevice = device;

	mWidth  = width;
	mHeight = height;
	mFormat = format;

	BuildResources();
}

ID3D12Resource* BlurFilter::Output()
{
	return mBlurMap0.Get();
}

void BlurFilter::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
                                  CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
                                  UINT                          descriptorSize)
{
	// Save references to the descriptors. 
	mBlur0CpuSrv = hCpuDescriptor;
	mBlur0CpuUav = hCpuDescriptor.Offset(1, descriptorSize);
	mBlur1CpuSrv = hCpuDescriptor.Offset(1, descriptorSize);
	mBlur1CpuUav = hCpuDescriptor.Offset(1, descriptorSize);

	mBlur0GpuSrv = hGpuDescriptor;
	mBlur0GpuUav = hGpuDescriptor.Offset(1, descriptorSize);
	mBlur1GpuSrv = hGpuDescriptor.Offset(1, descriptorSize);
	mBlur1GpuUav = hGpuDescriptor.Offset(1, descriptorSize);

	BuildDescriptors();
}

void BlurFilter::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth  = newWidth;
		mHeight = newHeight;

		BuildResources();

		// New resource, so we need new descriptors to that resource.
		BuildDescriptors();
	}
}

void BlurFilter::Execute(ID3D12GraphicsCommandList* cmdList,
                         ID3D12RootSignature*       rootSig,
                         ID3D12PipelineState*       horzBlurPSO,
                         ID3D12PipelineState*       vertBlurPSO,
                         ID3D12Resource*            input,
                         int                        blurCount)
{
	auto weights    = CalcGaussWeights(2.5f);
	int  blurRadius = (int)weights.size() / 2;

	cmdList->SetComputeRootSignature(rootSig);

	// bind gaussian blur radius and weights as root constants to the shader
	cmdList->SetComputeRoot32BitConstants(0, 1, &blurRadius, 0);
	cmdList->SetComputeRoot32BitConstants(0, (UINT)weights.size(), weights.data(), 1);

	// Next, we will copy data from back buffer (COPY_SOURCE) to mBlurMap0 (COPY_DEST)

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(input,
	                                                                  D3D12_RESOURCE_STATE_RENDER_TARGET,
	                                                                  D3D12_RESOURCE_STATE_COPY_SOURCE));

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
	                                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
	                                                                  D3D12_RESOURCE_STATE_COPY_DEST));

	cmdList->CopyResource(mBlurMap0.Get(), input);

	// The contents of the back buffer is now in mBlurMap0.

	// transition mBlurMap0 to READ state (compute shader input state)
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
	                                                                  D3D12_RESOURCE_STATE_COPY_DEST,
	                                                                  D3D12_RESOURCE_STATE_GENERIC_READ));

	// blur multiple times
	for (int i = 0; i < blurCount; ++i)
	{
		//
		// Horizontal Blur pass.
		//

		cmdList->SetPipelineState(horzBlurPSO);

		cmdList->SetComputeRootDescriptorTable(1, mBlur0GpuSrv); // t0 is mBlurMap0 (as our input source image)
		cmdList->SetComputeRootDescriptorTable(2, mBlur1GpuUav); // u0 is mBlurMap1 (as our blurred output image)

		// How many groups do we need to dispatch to cover a row of pixels, where each
		// group covers 256 pixels (the 256 is defined in the ComputeShader).
		//! See bottom of page 498
		UINT numGroupsX = (UINT)ceilf(mWidth / 256.0f);
		cmdList->Dispatch(numGroupsX, mHeight, 1); // launch a 2D grids of thread groups

		//
		// "swap" ping-pong buffer
		//

		// transition mBlurMap1 to READ state (compute shader input state)
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
		                                                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		                                                                  D3D12_RESOURCE_STATE_GENERIC_READ));

		// transition mBlurMap0 to UA state (compute shader output state)
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
		                                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
		                                                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		//
		// Vertical Blur pass.
		//

		cmdList->SetPipelineState(vertBlurPSO);

		cmdList->SetComputeRootDescriptorTable(1, mBlur1GpuSrv); // t0 is mBlurMap1 (as our input source image)
		cmdList->SetComputeRootDescriptorTable(2, mBlur0GpuUav); // u0 is mBlurMap0 (as our blurred output image)

		// How many groups do we need to dispatch to cover a column of pixels, where each
		// group covers 256 pixels  (the 256 is defined in the ComputeShader).
		//! See bottom of page 498
		UINT numGroupsY = (UINT)ceilf(mHeight / 256.0f);
		cmdList->Dispatch(mWidth, numGroupsY, 1); // launch a 2D grids of thread groups

		//
		// "swap" ping-pong buffer
		//

		// transition mBlurMap0 to READ state (compute shader input state)
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
		                                                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		                                                                  D3D12_RESOURCE_STATE_GENERIC_READ));

		// transition mBlurMap1 to UA state (compute shader output state)
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
		                                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
		                                                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}
}

std::vector<float> BlurFilter::CalcGaussWeights(float sigma)
{
	// using the formula at the bottom of page 490:

	float twoSigma2 = 2.0f * sigma * sigma;
	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	int blurRadius = (int)ceil(2.0f * sigma);

	assert(blurRadius <= MAX_BLUR_RADIUS);

	std::vector<float> weights;
	weights.resize(2 * blurRadius + 1);

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		float x = (float)i;

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < weights.size(); ++i)
	{
		weights[i] /= weightSum;
	}

	return weights;
}

void BlurFilter::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format                          = mFormat;
	srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip       = 0;
	srvDesc.Texture2D.MipLevels             = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format                           = mFormat;
	uavDesc.ViewDimension                    = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice               = 0;

	md3dDevice->CreateShaderResourceView(mBlurMap0.Get(), &srvDesc, mBlur0CpuSrv);
	md3dDevice->CreateUnorderedAccessView(mBlurMap0.Get(), nullptr, &uavDesc, mBlur0CpuUav);

	md3dDevice->CreateShaderResourceView(mBlurMap1.Get(), &srvDesc, mBlur1CpuSrv);
	md3dDevice->CreateUnorderedAccessView(mBlurMap1.Get(), nullptr, &uavDesc, mBlur1CpuUav);
}

void BlurFilter::BuildResources()
{
	// Create resources for pingpong buffers

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment          = 0;
	texDesc.Width              = mWidth;
	texDesc.Height             = mHeight;
	texDesc.DepthOrArraySize   = 1;
	texDesc.MipLevels          = 1;
	texDesc.Format             = mFormat;
	texDesc.SampleDesc.Count   = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAV will be created to both buffers

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		              D3D12_HEAP_FLAG_NONE,
		              &texDesc,
		              D3D12_RESOURCE_STATE_GENERIC_READ, // initially, mBlurMap0 will be in READ state (compute shader input state)
		              nullptr,
		              IID_PPV_ARGS(&mBlurMap0)));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		              D3D12_HEAP_FLAG_NONE,
		              &texDesc,
		              D3D12_RESOURCE_STATE_UNORDERED_ACCESS, // initially, mBlurMap1 will be in unordered access state (compute shader output state)
		              nullptr,
		              IID_PPV_ARGS(&mBlurMap1)));
}
