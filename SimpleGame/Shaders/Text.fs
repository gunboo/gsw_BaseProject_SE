#version 330

in vec3 v_TexCoord;
in vec4 v_Color;

uniform sampler2DArray u_Texture;
layout(location = 0) out vec4 FragColor;

void main()
{
	float coverage = texture(u_Texture, v_TexCoord).r;
	FragColor = vec4(v_Color.rgb, v_Color.a * coverage);
}
