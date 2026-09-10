#version 330

// Screen-space batch shader.
// a_Position is in pixels, origin at the screen center, +y pointing down.

in vec2 a_Position;
in vec4 a_Color;

uniform vec2 u_HalfViewport;

out vec4 v_Color;

void main()
{
	vec2 ndc = vec2(a_Position.x / u_HalfViewport.x, -a_Position.y / u_HalfViewport.y);
	gl_Position = vec4(ndc, 0.0, 1.0);
	v_Color = a_Color;
}
