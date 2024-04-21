{
  "settings": {
    "shaderType": "vertex"
  },
  "codes": [
    {
      "code": [
        "#version 460",
        "struct ValueSystem {",
        "   mat4 model;",
        "   mat4 normalModel;",
        "   vec4 color;",
        "   vec4 padding[7];",
        "};",
        "layout(binding=2) uniform _system { ValueSystem values; } system;",
        "layout(binding=3) uniform PROJ {",
        "   mat4 proj;",
        "} proj;",

        "$next_in vec3 inpos;"
      ]
    },
    {
      "code": [
        "$next_in vec3 innormal;"
      ],
      "dependsOn": [ "NORMAL" ]
    },
    {
      "code": [
        "$next_in vec2 inuv;"
      ],
      "dependsOn": [ "UV" ]
    },
    {
      "code": [
        "out gl_PerVertex {",
        "   vec4 gl_Position;",
        "};",
        "void main() {",
        "   vec4 POSITIONOUT = system.values.model * vec4(inpos, 1);",
        "   gl_Position = proj.proj * POSITIONOUT;",
        "}"
      ]
    }
  ]
}