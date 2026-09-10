#version 330

in vec2 a_Position;
in vec2 a_TexCoord;

uniform vec2 u_HalfViewport;

out vec2 v_TexCoord;

void main()
{
	vec2 ndc = vec2(a_Position.x / u_HalfViewport.x, -a_Position.y / u_HalfViewport.y);
	gl_Position = vec4(ndc, 0.0, 1.0);
	v_TexCoord = a_TexCoord;
}
