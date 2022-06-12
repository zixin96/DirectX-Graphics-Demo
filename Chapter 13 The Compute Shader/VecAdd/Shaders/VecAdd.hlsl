struct Data
{
	float3 v1;
	float2 v2;
};

// Data resources and outputs
StructuredBuffer<Data>   gInputA : register(t0);
StructuredBuffer<Data>   gInputB : register(t1);
RWStructuredBuffer<Data> gOutput : register(u0);

// The # of threads in the thread group. The threads in a group can be arranged in a 1D, 2D, or 3D grid layout
[numthreads(32, 1, 1)]
void CS(int3 dtid : SV_DispatchThreadID)
{
	gOutput[dtid.x].v1 = gInputA[dtid.x].v1 + gInputB[dtid.x].v1;
	gOutput[dtid.x].v2 = gInputA[dtid.x].v2 + gInputB[dtid.x].v2;
}
