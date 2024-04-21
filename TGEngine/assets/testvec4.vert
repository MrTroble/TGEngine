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

				"layout(location=0) in vec4 inpos;",
				"out gl_PerVertex {",
				"   vec4 gl_Position;",
				"};",
				"void main() {",
				"   gl_Position = inpos;",
				"}"
			]
		}
	]
}