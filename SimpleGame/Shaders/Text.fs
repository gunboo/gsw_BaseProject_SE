#version 330

in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec4 u_Color;

layout(location=0) out vec4 FragColor;

void main()
{
	// The glyph atlas stores coverage in the alpha channel; color comes from the uniform.
	float coverage = texture(u_Texture, v_TexCoord).a;
	FragColor = vec4(u_Color.rgb, u_Color.a * coverage);
}
