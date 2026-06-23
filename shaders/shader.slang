struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;
Sampler2D texture;

struct VertexInput {
    float2 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct VertexOutput {
    float3 color;
    float4 sv_position : SV_Position;
    float2 fragTexCoord;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;
    output.sv_position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition,0.0,1.0))));
    output.color = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput vertIn) : SV_TARGET {
   return float4(vertIn.color * texture.Sample(vertIn.fragTexCoord).rgb, 1.0);
}