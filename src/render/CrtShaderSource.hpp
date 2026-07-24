#pragma once

// M57: the advanced "1985 consumer CRT television" fragment shader (GLSL 330),
// embedded as a C++ string so there is no new asset type and no dependency. It
// is applied ONLY around the final window blit in VirtualScreen::blitToWindow
// (never to the capture path, which exports the pre-shader virtual target).
//
// Design contract (see docs/milestone_notes/M57_crt_strength.md):
//   * intensity 0 never runs this shader at all (VirtualScreen uses the plain
//     DrawTexturePro path), so 0 == the exact unfiltered image;
//   * every sub-effect is driven by its OWN non-linear activation curve of the
//     0..1 intensity, so the slider reads as one coherent continuum;
//   * a fixed, restrained texture-sample count (11) — no loops, no dynamic
//     bounds, no intermediate render target;
//   * the scan structure is anchored to the 240-line virtual source; the RGB
//     slot mask is anchored to destination/framebuffer pixels and fades out
//     when the window is too small to resolve a triad.
//
// Uniforms supplied by VirtualScreen (locations cached once):
//   crtIntensity   float  0..1
//   crtSourceRes   vec2   virtual resolution (426 x 240)
//   crtOutputRes   vec2   viewport size in framebuffer pixels
//   crtSourceTexel vec2   1/sourceRes (source texel size)
//   crtTime        float  seconds, only for the high-strength grain

namespace cd {

inline constexpr const char* kCrtFragmentSource = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float crtIntensity;    // 0..1
uniform vec2  crtSourceRes;    // 426 x 240
uniform vec2  crtOutputRes;    // viewport size in framebuffer pixels
uniform vec2  crtSourceTexel;  // 1 / sourceRes
uniform float crtTime;         // seconds

out vec4 finalColor;

const float PI  = 3.14159265;
const float TAU = 6.28318530;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

// Cheap deterministic hash for the restrained high-strength grain.
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    float I = clamp(crtIntensity, 0.0, 1.0);

    // Per-component non-linear activation curves (see section 6 of the spec):
    float scanAct   = 0.55 * pow(I, 0.8);         // scanlines: immediate, moderate
    float maskAct   = smoothstep(0.20, 1.00, I);  // slot mask: from ~0.2
    float beamAct   = smoothstep(0.25, 1.00, I);  // beam spread: from ~0.25
    float glowAct   = smoothstep(0.30, 1.00, I);  // glow: from ~0.3
    float curveAct  = smoothstep(0.20, 0.75, I);  // curvature/vignette: obvious ~0.5
    float chromaAct = smoothstep(0.40, 1.00, I);  // chroma bleed: from ~0.4
    float grainAct  = smoothstep(0.70, 1.00, I);  // grain: from ~0.7
    float toneAct   = smoothstep(0.15, 1.00, I);  // tonal response

    // Screen-space coord: the mask and vignette live on the physical glass, so
    // they are anchored to the flat screen, not the warped image.
    vec2 screenUv = fragTexCoord;

    // --- A. barrel curvature (identity when curveAct == 0, so no crop at 0) ---
    vec2 cc = screenUv * 2.0 - 1.0;
    cc *= 1.0 + 0.045 * curveAct;                 // slight inset before curving
    vec2 warp = (cc.yx * cc.yx) * (0.22 * curveAct);
    cc += cc * warp;
    vec2 uv = cc * 0.5 + 0.5;

    // --- B. rounded screen edge + gentle vignette ---
    float b = mix(0.0006, 0.006, curveAct);       // near-zero AA when flat
    vec2 edge = smoothstep(vec2(0.0), vec2(b), uv) *
                smoothstep(vec2(0.0), vec2(b), 1.0 - uv);
    float screenMask = edge.x * edge.y;           // 0 outside the curved glass
    vec2  vc  = uv * 2.0 - 1.0;
    float vig = clamp(1.0 - dot(vc, vc) * 0.20 * curveAct, 0.0, 1.0);

    // --- E/G. horizontal beam spread + chroma convergence ---
    float edgeDist = clamp(dot(vc, vc), 0.0, 1.0);        // 0 centre -> 1 corner
    float sp = beamAct   * 0.80;                          // beam, in source texels
    float cb = chromaAct * 0.90 * (1.0 + 0.6 * edgeDist); // chroma, more at edges
    vec2  tx = vec2(crtSourceTexel.x, 0.0);

    // Green stays sharp (3 tight taps); red/blue displaced and softened
    // (2 taps each). Vertical detail is untouched, so the pixel font stays legible.
    float g = texture(texture0, uv).g * 0.50
            + texture(texture0, uv - tx * sp).g * 0.25
            + texture(texture0, uv + tx * sp).g * 0.25;
    float r = texture(texture0, uv - tx * cb).r * 0.60
            + texture(texture0, uv - tx * (cb + sp)).r * 0.40;
    float bl = texture(texture0, uv + tx * cb).b * 0.60
             + texture(texture0, uv + tx * (cb + sp)).b * 0.40;
    vec3 col = vec3(r, g, bl);

    // --- F. bright-pixel glow / halation (4 luminance-thresholded taps) ---
    vec2 gr = crtSourceTexel * 1.5;
    vec3 n1 = texture(texture0, uv + vec2( gr.x, 0.0)).rgb;
    vec3 n2 = texture(texture0, uv + vec2(-gr.x, 0.0)).rgb;
    vec3 n3 = texture(texture0, uv + vec2(0.0,  gr.y)).rgb;
    vec3 n4 = texture(texture0, uv + vec2(0.0, -gr.y)).rgb;
    vec3 glow = n1 * smoothstep(0.55, 1.0, luma(n1))
              + n2 * smoothstep(0.55, 1.0, luma(n2))
              + n3 * smoothstep(0.55, 1.0, luma(n3))
              + n4 * smoothstep(0.55, 1.0, luma(n4));
    glow *= glowAct * 0.22;                        // local, restrained

    // --- C. scanline / beam structure, anchored to the 240-line source ---
    float ph   = fract(uv.y * crtSourceRes.y);
    float beam = sin(ph * PI);                     // 1 at row centre, 0 at the gap
    float scan = 1.0 - scanAct * (1.0 - beam);     // dark gaps, never dead rows

    // --- D. consumer slot / shadow mask, anchored to destination pixels ---
    vec2  dst = screenUv * crtOutputRes;
    float row = floor(dst.y / 3.0);
    float px  = dst.x + mod(row, 2.0) * 1.5;       // slot stagger every 3 rows
    vec3  m;
    m.r = 0.5 + 0.5 * cos(px * (TAU / 3.0));
    m.g = 0.5 + 0.5 * cos(px * (TAU / 3.0) + TAU / 3.0);
    m.b = 0.5 + 0.5 * cos(px * (TAU / 3.0) + 2.0 * TAU / 3.0);
    float scale     = crtOutputRes.x / crtSourceRes.x;
    float maskFade  = smoothstep(1.5, 3.5, scale); // fade when too small to resolve
    float maskDepth = 0.16 * maskAct * maskFade;
    vec3  mask = mix(vec3(1.0), m, maskDepth);

    // Combine: multiplicative darkening, then add glow so halation fills the gaps.
    col *= scan;
    col *= mask;
    col *= vig;
    col *= screenMask;
    col += glow * screenMask;

    // --- H. mild CRT tonal response (keep deep blacks, modest saturation) ---
    float l = luma(col);
    col = mix(vec3(l), col, 1.0 + 0.12 * toneAct);        // modest saturation lift
    col = pow(max(col, vec3(0.0)), vec3(1.0 + 0.06 * toneAct));

    // --- I. very restrained high-strength luminance grain ---
    float grain = hash21(screenUv * crtOutputRes + crtTime) - 0.5;
    col += grain * grainAct * 0.03;

    col = clamp(col, 0.0, 1.0);
    finalColor = vec4(col, 1.0) * colDiffuse * fragColor;
}
)";

}  // namespace cd
