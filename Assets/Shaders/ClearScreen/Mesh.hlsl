RWTexture2D<float4> gOutput : register(u0);

[shader("raygeneration")]
void rayGen()
{
    uint2 id = DispatchRaysIndex().xy;
    float2 uv = id / float2(DispatchRaysDimensions().xy);
    gOutput[id] = float4(uv, 0.5, 1.0);


}
