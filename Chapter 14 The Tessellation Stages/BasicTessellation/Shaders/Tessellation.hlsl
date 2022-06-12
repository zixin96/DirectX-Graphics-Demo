//***************************************************************************************
// Tessellation.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
float4x4 gWorld;
float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
{
float4x4 gView;
float4x4 gInvView;
float4x4 gProj;
float4x4 gInvProj;
float4x4 gViewProj;
float4x4 gInvViewProj;
float3   gEyePosW;
float    cbPerObjectPad1;
float2   gRenderTargetSize;
float2   gInvRenderTargetSize;
float    gNearZ;
float    gFarZ;
float    gTotalTime;
float    gDeltaTime;
float4   gAmbientLight;

float4 gFogColor;
float  gFogStart;
float  gFogRange;
float2 cbPerObjectPad2;

// Indices [0, NUM_DIR_LIGHTS) are directional lights;
// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
// are spot lights for a maximum of MaxLights per object.
Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
float4   gDiffuseAlbedo;
float3   gFresnelR0;
float    gRoughness;
float4x4 gMatTransform;
};

struct VertexIn
{
	float3 PosL : POSITION;
};

struct VertexOut
{
	float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
	// vertex shader is a pass-through shader
	VertexOut vout;

	vout.PosL = vin.PosL;

	return vout;
}

struct PatchTess
{
	float EdgeTess[4] : SV_TessFactor;         // output: tessellation factors: Defines the tessellation amount on each edge of a patch
	float InsideTess[2] : SV_InsideTessFactor; // output: tessellation factors: Defines the tessellation amount within a patch surface
};

// Reminder: constant hull shader is evaluated per patch
PatchTess ConstantHS(InputPatch<VertexOut, 4> patch,                    // input: all the control points (4) of the patch
                     uint                     patchID : SV_PrimitiveID) // The ID that identifies the patch
{
	// determine the tessellation factors based on the distance from the eye:
	// use a low-poly mesh in the distance, and increase the tessellation as the mesh approaches the eye

	PatchTess pt;

	// See "Quad Patch"
	float3 centerL = 0.25f * (patch[0].PosL + patch[1].PosL + patch[2].PosL + patch[3].PosL);
	float3 centerW = mul(float4(centerL, 1.0f), gWorld).xyz;

	// compute the distance from the eye to the patch
	float d = distance(centerW, gEyePosW);

	// Tessellate the patch based on distance from the eye such that
	// the tessellation is 0 if d >= d1 and 64 if d <= d0.  The interval
	// [d0, d1] defines the range we tessellate in.

	const float d0   = 20.0f;
	const float d1   = 100.0f;
	float       tess = 64.0f * saturate((d1 - d) / (d1 - d0)); // See "Tessellation-Distance Function" 

	// Uniformly tessellate the patch.

	pt.EdgeTess[0] = tess;
	pt.EdgeTess[1] = tess;
	pt.EdgeTess[2] = tess;
	pt.EdgeTess[3] = tess;

	pt.InsideTess[0] = tess;
	pt.InsideTess[1] = tess;

	return pt;
}

struct HullOut
{
	float3 PosL : POSITION;
};

[domain("quad")]                                                 // the patch type
[partitioning("integer")]                                        // the subdivision mode
[outputtopology("triangle_cw")]                                  // the winding order of the triangles created via subdivision
[outputcontrolpoints(4)]                                         // # of times the hull shader executes, outputting one control point each time
[patchconstantfunc("ConstantHS")]                                // a string specifying the constant hull shader function name
[maxtessfactor(64.0f)]                                           // a hint to the driver specifying the max tessellation factor your shader uses
HullOut HS(InputPatch<VertexOut, 4> p,                           // input: all of the control points of the patch
           uint                     i : SV_OutputControlPointID, // an index identifying the output control point the hull shader is working on 
           uint                     patchId : SV_PrimitiveID)    // The ID that identifies the patch
{
	// in this demo, the control point hull shader is a pass-through shader

	HullOut hout;

	// pass the control point through unmodified
	hout.PosL = p[i].PosL;

	return hout;
}

struct DomainOut
{
	float4 PosH : SV_POSITION;
};

// The domain shader is called for every vertex created by the tessellator.  
// It is like the vertex shader after tessellation.
[domain("quad")]
DomainOut DS(PatchTess                     patchTess,              // input: the tessellation factors
             float2                        uv : SV_DomainLocation, // the parametric coordinates of the tessellated vertex positions
             const OutputPatch<HullOut, 4> quad)                   // all the patch control points output from the control point hull shader
{
	// Simply tessellating is not enough to add detail, as the new triangles just lie on the patch that was subdivided.
	// We must offset those extra vertices in some way to better approximate the shape of the object we are modeling
	// In order to do this, we offset the y-coord by the "hills" functions in the domain shader

	DomainOut dout;

	// Bilinear interpolation.
	float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x);
	float3 v2 = lerp(quad[2].PosL, quad[3].PosL, uv.x);
	float3 p  = lerp(v1, v2, uv.y);

	// Displacement mapping: almost exactly the same as GetHillsHeight() function
	p.y = 0.3f * (p.z * sin(p.x) + p.x * cos(p.z));

	float4 posW = mul(float4(p, 1.0f), gWorld);
	dout.PosH   = mul(posW, gViewProj);

	return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
