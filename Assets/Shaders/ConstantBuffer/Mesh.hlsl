struct Vertex
{
    float3 position;
    float3 color;
};

struct SceneConstants
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 inverseViewProjection;
    float4 tint;
    float4 cameraPosition;
};

RaytracingAccelerationStructure gRtScene : register(t0);
StructuredBuffer<Vertex> gVertices : register(t1);
StructuredBuffer<uint> gIndices : register(t2);
RWTexture2D<float4> gOutput : register(u0);
ConstantBuffer<SceneConstants> gScene : register(b0);

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
    float2 ndc = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    float4 nearPoint = mul(float4(ndc, 0.0f, 1.0f), gScene.inverseViewProjection);
    float4 farPoint = mul(float4(ndc, 1.0f, 1.0f), gScene.inverseViewProjection);
    nearPoint.xyz /= nearPoint.w;
    farPoint.xyz /= farPoint.w;

    RayDesc ray;
    ray.Origin = gScene.cameraPosition.xyz;
    ray.Direction = normalize(farPoint.xyz - nearPoint.xyz);
    ray.TMin = 0.0f;
    ray.TMax = 100000.0f;

    RayPayload payload;
    payload.color = float3(0.0f, 0.2f, 0.4f);
    TraceRay(gRtScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

    gOutput[launchIndex.xy] = float4(payload.color * gScene.tint.rgb, 1.0f);
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
