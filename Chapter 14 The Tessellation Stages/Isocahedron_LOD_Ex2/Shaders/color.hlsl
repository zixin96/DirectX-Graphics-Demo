cbuffer cbPerObject : register(b0)
{
float4x4 gWorld;
float    gRadius;
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
	float3 PosL : POSITION;
	float4 Color : COLOR;
};

// struct GeoOut
// {
// 	float4 PosH : SV_POSITION;
// 	float4 Color : COLOR;
// };

struct PatchTess
{
	float EdgeTess[3] : SV_TessFactor; //!? we are tessellating triangle patch, so change this to match triangle mesh 
	float InsideTess : SV_InsideTessFactor;
};

struct HullOut
{
	float3 PosL : POSITION;
	float4 Color : COLOR;
};

struct DomainOut
{
	float4 PosH : SV_POSITION;
	float4 Color : COLOR;
};

// void SubdivideTriangle(VertexOut                    v0,
//                        VertexOut                    v1,
//                        VertexOut                    v2,
//                        inout TriangleStream<GeoOut> triangleStream);

VertexOut VS(VertexIn vin)
{
	// vertex shader is a pass-through shader
	VertexOut vout;

	vout.PosL  = vin.PosL;
	vout.Color = vin.Color;

	return vout;
}

// Reminder: constant hull shader is evaluated per patch
PatchTess ConstantHS(InputPatch<VertexOut, 3> patch,                    // input: all the control points (3) of the patch
                     uint                     patchID : SV_PrimitiveID) // The ID that identifies the patch
{
	float d = length(gEyePosW);

	PatchTess pt;

	// Tessellate the patch based on distance from the eye such that
	// the tessellation is 0 if d >= d1 and 64 if d <= d0.  The interval
	// [d0, d1] defines the range we tessellate in.

	const float d0   = 1.0f;
	const float d1   = 30.0f;
	float       tess = 20.0f * saturate((d1 - d) / (d1 - d0));

	// Uniformly tessellate the patch.

	pt.EdgeTess[0] = tess;
	pt.EdgeTess[1] = tess;
	pt.EdgeTess[2] = tess;

	pt.InsideTess = tess;

	return pt;
}

[domain("tri")] //!? the patch type will change to "tri"
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 3> p, //!? triangle has 3 control points
           uint                     i : SV_OutputControlPointID,
           uint                     patchId : SV_PrimitiveID)
{
	// in this demo, the control point hull shader is a pass-through shader
	HullOut hout;

	// pass the control point through unmodified
	hout.PosL  = p[i].PosL;
	hout.Color = p[i].Color;

	return hout;
}


[domain("tri")]
DomainOut DS(PatchTess                     patchTess,
             float3                        uvw : SV_DomainLocation, //!? UVW are barycentric coordinates
             const OutputPatch<HullOut, 3> tri)
{
	DomainOut dout;

	//!? follow equation on page 791
	float3 v1 = tri[0].PosL * uvw.x;
	float3 v2 = tri[1].PosL * uvw.y;
	float3 v3 = tri[2].PosL * uvw.z;
	float3 p  = normalize(v1 + v2 + v3) * gRadius; //!? must first project vertices onto a unit sphere and scaled by radius to make it a sphere

	float4 posW = mul(float4(p, 1.0f), gWorld);
	dout.PosH   = mul(posW, gViewProj);
	dout.Color  = tri[0].Color;

	return dout;
}


float4 PS(DomainOut pin) : SV_Target
{
	return pin.Color;
}
