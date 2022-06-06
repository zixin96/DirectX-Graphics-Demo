//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorldViewProj; 
};

struct VertexIn
{
	// L: local space
	float3 PosL  : POSITION; // this is the same as "float3 PosL  : POSITION0;"
    float4 Color : COLOR; // this is the same as "float4 Color  : COLOR0;"
};

struct VertexOut
{
	// H: homogenenous clip space
	float4 PosH  : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	// Transform to homogeneous clip space.
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	// clip(pin.Color.r - 0.5f);
    return pin.Color;
}


