#version 450

// 버텍스 셰이더에서 넘어온 색상 데이터 (location 번호가 같아야 연결됩니다)
layout(location = 0) in vec3 fragColor;

// 화면 프레임버퍼로 최종 출력될 색상 (R, G, B, Alpha)
layout(location = 0) out vec4 outColor;

void main() {
    // 넘겨받은 색상에 불투명도(Alpha) 1.0을 더해 최종 색상으로 출력합니다.
    // 세 꼭짓점 사이의 픽셀들은 GPU가 자동으로 부드럽게 보간(Interpolation)해 줍니다.
    outColor = vec4(fragColor, 1.0);
}
