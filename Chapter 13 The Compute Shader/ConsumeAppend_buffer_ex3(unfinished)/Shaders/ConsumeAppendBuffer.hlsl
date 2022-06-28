struct InputData
{
	float3 v;
};

struct OutputData
{
	float v;
};

ConsumeStructuredBuffer<InputData> gInputA : register(u0);
AppendStructuredBuffer<OutputData> gOutput : register(u1);

[numthreads(64, 1, 1)]
void CS()
{
	float3     extractedInput = gInputA.Consume().v;

	OutputData outputData;
	outputData.v = length(extractedInput);

	gOutput.Append(outputData);
}
