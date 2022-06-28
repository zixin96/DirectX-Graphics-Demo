struct Data
{
	float3 v1;
};

ConsumeStructuredBuffer<Data> gInputA : register(t0);
AppendStructuredBuffer<float> gOutput : register(u0);

[numthreads(64, 1, 1)]
void CS()
{
    gOutput.Append(length(gInputA.Consume().v1));
}
