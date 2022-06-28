struct Data
{
	float3 v1;
};

ConsumeStructuredBuffer<Data> gInputA : register(u0);
AppendStructuredBuffer<float> gOutput : register(u1);

[numthreads(64, 1, 1)]
void CS()
{
	gOutput.Append(length(gInputA.Consume().v1));
}
