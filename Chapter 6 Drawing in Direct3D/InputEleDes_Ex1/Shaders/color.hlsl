cbuffer cbPerObject : register(b0)
{
	float4x4 gWorldViewProj; 
};

//!? we changed the input signature of the vertex shader
struct VertexIn
{
    float3 PosL : POSITION;
    float4 TangentL : TANGENT;
    float4 NormalL : NORMAL;
    float2 Tex0L : TEX0;
    float2 Tex1L : TEX1;
    float4 Color : COLOR; //!? tricky: on the shader side, we specify color as a float4, with this range [0.0, 1.0] for each element
};

struct VertexOut
{
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
    return pin.Color;
}


