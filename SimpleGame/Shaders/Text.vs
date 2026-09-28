#version 330

layout(location = 0) in vec4 a_Rectangle;
layout(location = 1) in vec4 a_UV;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Layer;

uniform samplerBuffer u_Meshes;
uniform int u_Quad;
uniform vec2 u_HalfViewport;

out vec3 v_TexCoord;
out vec4 v_Color;

void main()
{
	vec2 corner = texelFetch(u_Meshes, u_Quad + gl_VertexID).xy;
	vec2 position = a_Rectangle.xy + corner * a_Rectangle.zw;
	gl_Position = vec4(position.x / u_HalfViewport.x, -position.y / u_HalfViewport.y, 0.0, 1.0);
	v_TexCoord = vec3(mix(a_UV.xy, a_UV.zw, corner), a_Layer);
	v_Color = a_Color;
}
