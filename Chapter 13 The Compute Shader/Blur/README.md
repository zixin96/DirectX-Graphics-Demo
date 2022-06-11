# Blur Demo

Changes: 

To fix D3D resource transition errors, the initial state of ping-pong buffers are changed. 

ToDO:

Consider using `D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE` (since this demo only accesses `texture2D` in the compute shader)