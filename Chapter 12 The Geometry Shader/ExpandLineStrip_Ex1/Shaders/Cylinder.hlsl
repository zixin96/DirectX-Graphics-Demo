//***************************************************************************************
// TreeSprite.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2DArray gTreeMapArray : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per object.
cbuffer cbPerObject : register(b0)
{
float4x4 gWorld;
float4x4 gTexTransform;
};

// Constant data that varies per frame.
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
	float3 PosW : POSITION;
	float4 Color : COLOR;
};

struct VertexOut
{
	float3 PosW : POSITION;
	float4 Color : COLOR;
};

struct GeoOut
{
	float4 PosH : SV_POSITION;
	float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Just pass data over to geometry shader.
	vout.PosW  = vin.PosW;
	vout.Color = vin.Color;

	return vout;
}

// generate cylinder from cycle:
// each line segment on the cycle should generate a line strip of 4 vertices

/*
a----b

|
V

c----d
|    |
|    |
a----b
*/

[maxvertexcount(4)]
void GS(line VertexOut           gin[2],     // if input is line strips, the # of vertex is 2 by definition
        inout LineStream<GeoOut> lineStream) // output is line strip
{
	GeoOut a;
	a.PosH  = mul(float4(gin[0].PosW, 1.0f), gViewProj);
	a.Color = gin[0].Color;

	GeoOut b;
	b.PosH  = mul(float4(gin[1].PosW, 1.0f), gViewProj);
	b.Color = gin[1].Color;

	GeoOut c;
	c.PosH  = mul(float4(gin[0].PosW.x, gin[0].PosW.y + 12.0f, gin[0].PosW.z, 1.0f), gViewProj);
	c.Color = gin[0].Color;

	GeoOut d;
	d.PosH  = mul(float4(gin[1].PosW.x, gin[1].PosW.y + 12.0f, gin[1].PosW.z, 1.0f), gViewProj);
	d.Color = gin[1].Color;

	// the order of appending these vertices matters! 
	lineStream.Append(a);
	lineStream.Append(b);
	lineStream.Append(d);
	lineStream.Append(c);
}

float4 PS(GeoOut pin) : SV_Target
{
	return pin.Color;
}
