#version 330

layout(location = 0) in ivec2 a_Command;

uniform samplerBuffer u_Meshes;
uniform samplerBuffer u_Instances;
uniform vec2 u_HalfViewport;
uniform float u_Time;

out vec4 v_Color;

void main()
{
	// Each hardware instance references one triangle of immutable mesh data.
	// The ordered command stream preserves polygon-level painter ordering.
	vec2 local = texelFetch(u_Meshes, a_Command.x + gl_VertexID).xy;
	int first = a_Command.y * 8;
	vec4 corners01 = texelFetch(u_Instances, first);
	vec4 corners23 = texelFetch(u_Instances, first + 1);
	vec4 color0 = texelFetch(u_Instances, first + 2);
	vec4 color1 = texelFetch(u_Instances, first + 3);
	vec4 color2 = texelFetch(u_Instances, first + 4);
	vec4 color3 = texelFetch(u_Instances, first + 5);
	vec4 animation = texelFetch(u_Instances, first + 6);
	vec4 transform = texelFetch(u_Instances, first + 7);
	vec2 position;
	vec4 color = color0;

	if (animation.w > 0.5)
	{
		// Deform the cached unit triangle/quad using instance control points.
		position = mix(mix(corners01.xy, corners01.zw, local.x),
			mix(corners23.zw, corners23.xy, local.x), local.y);
		color = mix(mix(color0, color1, local.x), mix(color3, color2, local.x), local.y);
	}
	else
	{
		position = transform.xy + local * transform.zw;
	}

	float glow = 1.0;
	float kind = animation.x;
	float phase = animation.y;
	float strength = animation.z;

	if (kind > 0.5 && kind < 1.5)
	{
		position.y += sin(u_Time * 1.7 + phase) * strength;
		position.x += cos(u_Time * 1.1 + phase * 1.3) * strength * 0.6;
		glow = 1.0 + 0.16 * sin(u_Time * 2.3 + phase * 0.7);
	}
	else if (kind > 1.5 && kind < 2.5)
	{
		float pulse = sin(u_Time * 9.0 + phase) * 0.5 + 0.5;
		position.y -= pulse * strength;
		position.x += sin(u_Time * 6.0 + phase * 2.0) * strength * 0.4;
		glow = 1.0 + 0.45 * pulse;
	}
	else if (kind > 2.5 && kind < 3.5)
	{
		position.x += sin(u_Time * 1.3 + phase) * strength;
		position.y += sin(u_Time * 0.9 + phase * 1.7) * strength * 0.25;
	}

	gl_Position = vec4(position.x / u_HalfViewport.x, -position.y / u_HalfViewport.y, 0.0, 1.0);
	v_Color = vec4(color.rgb * glow, color.a);
}
