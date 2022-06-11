//=============================================================================
// Performs a separable Guassian blur with a blur radius up to 5 pixels.
//=============================================================================

cbuffer cbSettings : register(b0) // compute shader can access values in constant buffers
{
// We cannot have an array entry in a constant buffer that gets mapped onto
// root constants, so list each element.  

int gBlurRadius;

// Support up to 11 blur weights.
float w0;
float w1;
float w2;
float w3;
float w4;
float w5;
float w6;
float w7;
float w8;
float w9;
float w10;
};

static const int gMaxBlurRadius = 5;

Texture2D           gInput : register(t0);  // texture input
RWTexture2D<float4> gOutput : register(u0); // texture output

#define N 256
#define CacheSize (N + 2*gMaxBlurRadius) // a thread group of N threads requires N + 2 * R texels to perform the blur
groupshared float4 gCache[CacheSize];

// The # of threads in the thread group. The threads in a group can be arranged in a 1D, 2D, or 3D grid layout
[numthreads(N, 1, 1)] // specify the thread group dimension: N x 1 x 1
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID,
                int3 dispatchThreadID : SV_DispatchThreadID)
{
	// Put in an array for each indexing.
	float weights[11] = {w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10};

	//
	// Fill local thread storage to reduce bandwidth.  To blur 
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	//

	// See D3D3

	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.

	// These following two if statement will make some threads on extra work

	// For thread 0, 1, 2, ..., (gBlurRadius - 1), do this: 
	if (groupThreadID.x < gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int x                   = max(dispatchThreadID.x - gBlurRadius, 0);
		gCache[groupThreadID.x] = gInput[int2(x, dispatchThreadID.y)];
	}

	// For thread N - gBlurRadius, N - gBlurRadius + 1, ..., N - 1, do this: 
	if (groupThreadID.x >= N - gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int x                                     = min(dispatchThreadID.x + gBlurRadius, gInput.Length.x - 1); // gInput.Length is texture dimension
		gCache[groupThreadID.x + 2 * gBlurRadius] = gInput[int2(x, dispatchThreadID.y)];
	}

	//! Note: All threads in this thread group will execute the following statement
	//! If you put all gCache statement in if-else statement, then only threads in the range [gBlurRadius, N - gBlurRadius) will execute the following statement

	// For thread 0 to N - 1, do this: 
	gCache[groupThreadID.x + gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy - 1)]; // Clamp out of bound samples that occur at image borders.

	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();

	//
	// Now blur each pixel.
	//

	float4 blurColor = float4(0, 0, 0, 0);

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = groupThreadID.x + gBlurRadius + i;

		blurColor += weights[i + gBlurRadius] // access weights from start to end
				* gCache[k];
	}

	gOutput[dispatchThreadID.xy] = blurColor;
}

[numthreads(1, N, 1)] // specify the thread group dimension: 1 x N x 1
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID,
                int3 dispatchThreadID : SV_DispatchThreadID)
{
	// Put in an array for each indexing.
	float weights[11] = {w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10};

	//
	// Fill local thread storage to reduce bandwidth.  To blur 
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	//

	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.
	if (groupThreadID.y < gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int y                   = max(dispatchThreadID.y - gBlurRadius, 0);
		gCache[groupThreadID.y] = gInput[int2(dispatchThreadID.x, y)];
	}

	if (groupThreadID.y >= N - gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int y                                     = min(dispatchThreadID.y + gBlurRadius, gInput.Length.y - 1);
		gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[int2(dispatchThreadID.x, y)];
	}

	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.y + gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy - 1)];


	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();

	//
	// Now blur each pixel.
	//

	float4 blurColor = float4(0, 0, 0, 0);

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = groupThreadID.y + gBlurRadius + i;

		blurColor += weights[i + gBlurRadius] * gCache[k];
	}

	gOutput[dispatchThreadID.xy] = blurColor;
}
