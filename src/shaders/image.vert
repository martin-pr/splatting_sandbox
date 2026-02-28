#version 450

layout(push_constant) uniform PC {
    float imageAspect;
    float screenAspect;
} pc;

layout(location = 0) out vec2 outUV;

void main() {
    // Two CCW triangles forming a full-screen quad, generated from vertex index.
    // Unit square in [0,1] x [0,1]:  TL, BL, TR, TR, BL, BR
    const vec2 pos[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0)
    );

    vec2 uv = pos[gl_VertexIndex];
    outUV = uv;

    // Letterbox/pillarbox: scale the quad in NDC to preserve the image aspect
    // ratio while fitting entirely within the screen. The renderer clear color
    // shows through in the bars.
    float scaleX = 1.0;
    float scaleY = 1.0;
    if (pc.imageAspect > pc.screenAspect) {
        // Image is wider than screen (relative) -> fit width, letterbox top/bottom
        scaleY = pc.screenAspect / pc.imageAspect;
    } else {
        // Image is taller than screen (relative) -> fit height, pillarbox left/right
        scaleX = pc.imageAspect / pc.screenAspect;
    }

    // Map [0,1] -> [-scale, +scale] in NDC (Vulkan: y positive = down)
    gl_Position = vec4((uv * 2.0 - 1.0) * vec2(scaleX, scaleY), 0.0, 1.0);
}
