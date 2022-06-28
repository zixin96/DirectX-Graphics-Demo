struct Data
{
	float3 v1;
};

StructuredBuffer<Data>   gInputA : register(t0);
RWStructuredBuffer<float> gOutput : register(u0);

// The # of threads in the thread group. The threads in a group can be arranged in a 1D, 2D, or 3D grid layout
[numthreads(64, 1, 1)]
void CS(int3 dtid : SV_DispatchThreadID)
{
    // compute the length of the vectors and outputs the result into a floating point buffer 
    gOutput[dtid.x] = length(gInputA[dtid.x].v1);
}
