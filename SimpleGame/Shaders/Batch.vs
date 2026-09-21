#version 330

// Screen-space batch shader.
// a_Position is in pixels, origin at the screen center, +y pointing down.
// a_Anim carries a per-vertex animation so that water, flame and foliage move
// on the GPU instead of being re-tessellated by the CPU every frame.
//   a_Anim.x = kind, a_Anim.y = phase, a_Anim.z = strength in pixels

in vec2 a_Position;
in vec4 a_Color;
in vec3 a_Anim;

uniform vec2 u_HalfViewport;
uniform float u_Time;

out vec4 v_Color;

void main()
{
	vec2 position = a_Position;
	float glow = 1.0;

	float kind = a_Anim.x;
	float phase = a_Anim.y;
	float strength = a_Anim.z;

	if (kind > 0.5 && kind < 1.5)
	{
		// Water: a slow swell plus a shimmer on the crests.
		position.y += sin(u_Time * 1.7 + phase) * strength;
		position.x += cos(u_Time * 1.1 + phase * 1.3) * strength * 0.6;
		glow = 1.0 + 0.16 * sin(u_Time * 2.3 + phase * 0.7);
	}
	else if (kind > 1.5 && kind < 2.5)
	{
		// Flame: licks upward and brightens on the same beat.
		float pulse = sin(u_Time * 9.0 + phase) * 0.5 + 0.5;
		position.y -= pulse * strength;
		position.x += sin(u_Time * 6.0 + phase * 2.0) * strength * 0.4;
		glow = 1.0 + 0.45 * pulse;
	}
	else if (kind > 2.5 && kind < 3.5)
	{
		// Foliage sway. Strength is baked per vertex so trunks stay still and
		// the crown moves most.
		position.x += sin(u_Time * 1.3 + phase) * strength;
		position.y += sin(u_Time * 0.9 + phase * 1.7) * strength * 0.25;
	}

	vec2 ndc = vec2(position.x / u_HalfViewport.x, -position.y / u_HalfViewport.y);

	gl_Position = vec4(ndc, 0.0, 1.0);
	v_Color = vec4(a_Color.rgb * glow, a_Color.a);
}
