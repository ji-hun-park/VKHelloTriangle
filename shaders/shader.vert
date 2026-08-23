#version 450 // GLSL 4.5 버전 사용 (Vulkan 표준)

// 삼각형의 세 꼭짓점 위치 (x, y)
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),  // 위쪽 중앙
    vec2(0.5, 0.5),   // 오른쪽 아래
    vec2(-0.5, 0.5)   // 왼쪽 아래
);

// 각 꼭짓점의 색상 (R, G, B)
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0), // 빨강
    vec3(0.0, 1.0, 0.0), // 초록
    vec3(0.0, 0.0, 1.0)  // 파랑
);

// 프래그먼트 셰이더로 넘겨줄 출력 변수
layout(location = 0) out vec3 fragColor;

void main() {
    // gl_VertexIndex는 Vulkan이 glDraw 호출 시 자동으로 증가시키는 내장 변수입니다.
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
