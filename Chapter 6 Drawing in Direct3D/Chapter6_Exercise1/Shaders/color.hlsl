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
    float4 tan : TANGENT; 
    float4 nor : NORMAL; 
    float2 t1 : TEX0; 
    float2 t2 : TEX1; 
    float4 xmC : XMCOLOR; 
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
    vout.Color = vin.Color * vin.xmC * float4(vin.t1, vin.t2) * vin.nor * vin.tan;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}


