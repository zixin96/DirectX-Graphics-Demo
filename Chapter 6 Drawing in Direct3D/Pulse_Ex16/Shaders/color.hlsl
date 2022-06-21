cbuffer cbPerObject : register(b0)
{
float4x4 gWorldViewProj;
//!? add a pulse color
float4 gPulseColor;
//!? add global time
float gTime;
};

struct VertexIn
{
	// L: local space
	float3 PosL : POSITION; // this is the same as "float3 PosL  : POSITION0;"
	float4 Color : COLOR;   // this is the same as "float4 Color  : COLOR0;"
};

struct VertexOut
{
	// H: homogenenous clip space
	float4 PosH : SV_POSITION;
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
	const float pi = 3.14159;
	//!? Oscillate a value in [0,1] over time using a sine function.
	float s = 0.5f * sin(2 * gTime - 0.25f * pi) + 0.5f; // https://graphtoy.com/?f1(x,t)=0.5*sin(2*t-0.25*PI)+0.5&v1=true&f2(x,t)=&v2=false&f3(x,t)=&v3=false&f4(x,t)=&v4=false&f5(x,t)=&v5=false&f6(x,t)=&v6=false&grid=1&coords=0,0,2.3741360268016223
	//!? Linearly interpolate between pin.Color and gPulseColor based on parameter s.
	float4 c = lerp(pin.Color, gPulseColor, s);
	return c;
}
