cbuffer cbPerObject : register(b0)
{
float4x4 gWorld;
};

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
};

struct VertexIn
{
	float3 PosL : POSITION;
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

void SubdivideTriangle(VertexOut                    v0,
                       VertexOut                    v1,
                       VertexOut                    v2,
                       inout TriangleStream<GeoOut> triangleStream);


VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// pass vertex world position and color to geometry shader
	vout.PosW  = mul(float4(vin.PosL, 1.0), gWorld).xyz;
	vout.Color = vin.Color;

	return vout;
}

// We need to output at most 32 vertices: 8 + 8 + 8 + 8. Subdivide 4 inner triangles if we do subdivision twice
[maxvertexcount(32)]
void GS(triangle VertexOut gin[3], inout TriangleStream<GeoOut> triangleStream)
{
	// distance to the camera
	float distance = length(gEyePosW);

	// no subdivision
	if (distance >= 30.f)
	{
		float4 v[3];
		v[0] = float4(gin[0].PosW, 1.0);
		v[1] = float4(gin[1].PosW, 1.0);
		v[2] = float4(gin[2].PosW, 1.0);

		GeoOut geoOut[3];
		[unroll]
		for (int i = 0; i < 3; ++i)
		{
			geoOut[i].PosH  = mul(v[i], gViewProj);
			geoOut[i].Color = gin[0].Color;
			triangleStream.Append(geoOut[i]);
		}
	}
	// one subdivision
    else if (distance >= 15.f)
	{
		SubdivideTriangle(gin[0], gin[1], gin[2], triangleStream);
	}
	// two subdivisions
	else
	{
		// compute the vertices of the first division
		float len = length(gin[0].PosW);

		float3 m0 = normalize((gin[0].PosW + gin[1].PosW) * 0.5) * len;
		float3 m1 = normalize((gin[1].PosW + gin[2].PosW) * 0.5) * len;
		float3 m2 = normalize((gin[0].PosW + gin[2].PosW) * 0.5) * len;

		VertexOut v[6];
		v[0].PosW = gin[0].PosW;
		v[1].PosW = m0;
		v[2].PosW = m2;
		v[3].PosW = m1;
		v[4].PosW = gin[2].PosW;
		v[5].PosW = gin[1].PosW;

		[unroll]
		for (int i = 0; i < 6; ++i)
		{
			v[i].Color = gin[0].Color;
		}

		// 4 subdivision, each subdivides one inner triangle
		SubdivideTriangle(v[0], v[1], v[2], triangleStream);
		SubdivideTriangle(v[1], v[3], v[2], triangleStream);
		SubdivideTriangle(v[2], v[3], v[4], triangleStream);
		SubdivideTriangle(v[1], v[5], v[3], triangleStream);
	}
}


float4 PS(GeoOut pin) : SV_Target
{
	return pin.Color;
}

void SubdivideTriangle(VertexOut                    v0,
                       VertexOut                    v1,
                       VertexOut                    v2,
                       inout TriangleStream<GeoOut> triangleStream)
{
	float len = length(v0.PosW);

	float3 m0 = normalize((v0.PosW + v1.PosW) * 0.5f) * len;
	float3 m1 = normalize((v1.PosW + v2.PosW) * 0.5f) * len;
	float3 m2 = normalize((v0.PosW + v2.PosW) * 0.5f) * len;

	float3 v[6];
	v[0] = v0.PosW;
	v[1] = m0;
	v[2] = m2;
	v[3] = m1;
	v[4] = v2.PosW;
	v[5] = v1.PosW;

	GeoOut geoOut[6];
	[unroll]
	for (int i = 0; i < 6; ++i)
	{
		geoOut[i].PosH  = mul(float4(v[i], 1.f), gViewProj);
		geoOut[i].Color = v0.Color;
	}

	[unroll]
	for (int j = 0; j < 5; ++j)
	{
		triangleStream.Append(geoOut[j]);
	}

	triangleStream.RestartStrip();
	triangleStream.Append(geoOut[1]);
	triangleStream.Append(geoOut[5]);
	triangleStream.Append(geoOut[3]);
	// restart the strip to prepare for next round of subdivision
	triangleStream.RestartStrip();
}
