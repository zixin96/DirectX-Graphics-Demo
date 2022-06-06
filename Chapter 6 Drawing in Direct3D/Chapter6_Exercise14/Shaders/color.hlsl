//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorldViewProj; 
};

cbuffer cbPerPass : register(b1)
{
	float gTime;
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
	
	// animate the colors as a function of tiem by distorting them periodically with the sine functions
	//vin.Color.xy += 0.5f*sin(vin.Color.x)*sin(3.0f*gTime);
 	//vin.Color.z *= 0.6f + 0.4f*sin(2.0f*gTime);

	// Transform to homogeneous clip space.
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	pin.Color.xy += 0.5f*sin(pin.Color.x)*sin(3.0f*gTime);
 	pin.Color.z *= 0.6f + 0.4f*sin(2.0f*gTime);
    return pin.Color;
}


