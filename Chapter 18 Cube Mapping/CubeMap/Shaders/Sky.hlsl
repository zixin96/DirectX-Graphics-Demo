//=============================================================================
// Sky.fx by Frank Luna (C) 2011 All Rights Reserved.
//=============================================================================

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Use local vertex position as cubemap 3D lookup vector. This will project the cube map onto the sphere.
	vout.PosL = vin.PosL;

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gObjConstants.gWorld);

	// Center the sky sphere about the camera in world space so that it's always centered about the camera
	// Consequently, as the camera moves, we are getting no closer to the surface of the sphere
	posW.xyz += gPassConstants.gEyePosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(posW, gPassConstants.gViewProj).xyww;

	return vout;
}

// To create the illusion of distant mountains fra in the horizon and a sky, we texture the sphere using an environment map.
// The environment map is projected onto the sphere's surface.

float4 PS(VertexOut pin) : SV_Target
{
	return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}
