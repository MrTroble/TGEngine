{
	"settings": {
		"shaderType": "fragment"
	},
	"codes": [
		{
			"code": [
				"#version 460",
				"",
				"layout(location=0) out vec4 COLOR;",
				"layout(location=1) out vec4 NORMAL;",
				"layout(location=2) out float ROUGHNESS;",
				"layout(location=3) out float METALLIC;",
				"",
				"void main() {",
				"   COLOR = vec4(1, 0, 0, 1);",
				"   NORMAL = vec4(1, 1, 1, 1);",
				"   ROUGHNESS = 0;",
				"   METALLIC = 0;",
				"}"
			]
		}
	]
}