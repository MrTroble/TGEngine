{
  "settings": {
    "shaderType": "fragment"
  },
  "codes": [
    {
      "code": [
        "layout(input_attachment_index=0, set=0, binding = 0) uniform subpassInput ALBEDO;",
        "layout(input_attachment_index=1, set=0, binding = 1) uniform subpassInput NORMAL;",
        "layout(input_attachment_index=2, set=0, binding = 2) uniform isubpassInput MATERIAL_ID;",
        "layout(input_attachment_index=3, set=0, binding = 3) uniform subpassInput POSITION;",
        "struct Light {",
        "    vec3 pos;",
        "    float minRatio;",
        "    vec3 color;",
        "    float intensity;",
        "};",
        "layout(set=0, binding = 4) uniform _TMP {",
        "    Light light[50];",
        "    int lightCount;",
        "} lights;",
        "layout(location=0) out vec4 colorout;",
        "void main() {",
        "    vec3 color = subpassLoad(ALBEDO).rgb;",
        "    vec3 normal = normalize(subpassLoad(NORMAL).rgb);",
        "    vec3 pos = subpassLoad(POSITION).rgb;",
        "    int materialID = subpassLoad(MATERIAL_ID).r;",
        "    if(materialID == 0) discard;",
        "    vec3 multiplier = vec3(0.1f, 0.1f, 0.1f);",
        "    for(int x = 0; x < lights.lightCount; x++) {",
        "        Light lightInfo = lights.light[x];",
        "        vec3 distance = pos - lightInfo.pos;",
        "        float distanceFallof = max(1/dot(distance, distance), lightInfo.minRatio);",
        "        vec3 l = normalize(distance);",
        "        float ratio = max(0, dot(normal, l));",
        "        multiplier += lightInfo.color * ratio * distanceFallof * lightInfo.intensity;",
        "    }",
        "    colorout = vec4(color * multiplier, 1);",
        "}"
      ]
    }
  ]
}