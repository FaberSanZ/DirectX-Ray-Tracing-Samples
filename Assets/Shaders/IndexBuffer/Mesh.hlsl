struct Vertex
{
    float3 position;
    float3 color;
};

RaytracingAccelerationStructure gRtScene : register(t0);
StructuredBuffer<Vertex> gVertices : register(t1);
StructuredBuffer<uint> gIndices : register(t2);
RWTexture2D<float4> gOutput : register(u0);

struct RayPayload
{
    float3 color;
};

[shader("raygeneration")]
void rayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);
    float2 d = ((crd / dims) * 2.0f - 1.0f);
    float aspectRatio = dims.x / dims.y;

    RayDesc ray;
    ray.Origin = float3(0.0f, 0.0f, -2.0f);
    ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1.0f));
    ray.TMin = 0.0f;
    ray.TMax = 100000.0f;

    RayPayload payload;
    payload.color = float3(0.1f, 0.2f, 0.4f);
    TraceRay(gRtScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

    gOutput[launchIndex.xy] = float4(payload.color, 1.0f);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.color = float3(0.0f, 0.2f, 0.4f);
}

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint triIndex = PrimitiveIndex() * 3;
    Vertex v0 = gVertices[gIndices[triIndex + 0]];
    Vertex v1 = gVertices[gIndices[triIndex + 1]];
    Vertex v2 = gVertices[gIndices[triIndex + 2]];

    float u = attribs.barycentrics.x;
    float v = attribs.barycentrics.y;
    float w = 1.0f - u - v;

    payload.color = v0.color * w + v1.color * u + v2.color * v;
}
