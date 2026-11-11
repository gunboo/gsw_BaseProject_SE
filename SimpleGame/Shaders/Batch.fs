#version 330

in vec4 v_Color;
in vec2 v_GroundCoord;
in float v_GrassCoverage;
flat in float v_GroundKind;

layout(location=0) out vec4 FragColor;

float GroundHash(vec2 cell)
{
	return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
}

float GroundNoise(vec2 position)
{
	vec2 cell = floor(position);
	vec2 fraction = fract(position);
	vec2 curve = fraction * fraction * (3.0 - 2.0 * fraction);

	return mix(mix(GroundHash(cell), GroundHash(cell + vec2(1.0, 0.0)), curve.x),
		mix(GroundHash(cell + vec2(0.0, 1.0)), GroundHash(cell + vec2(1.0, 1.0)), curve.x), curve.y);
}

vec3 GroundMaterial(vec3 base)
{
	vec2 position = v_GroundCoord;
	float broad = GroundNoise(position * 0.48 + vec2(5.7, 12.9));
	float grain = GroundNoise(position * 11.0);
	float detail = GroundNoise(position * 2.1 + vec2(9.1, 3.4));
	float grass = smoothstep(0.30, 0.70, v_GrassCoverage + (detail - 0.5) * 0.35);
	float strength = smoothstep(0.12, 0.36, max(base.r, max(base.g, base.b)));
	vec3 material = base;

	if (v_GroundKind < 4.5)
	{
		material *= mix(vec3(1.09, 0.96, 0.89), vec3(0.91, 1.13, 0.86), grass);
		material += (grass - v_GrassCoverage) * vec3(-0.055, 0.065, -0.035);
	}
	else if (v_GroundKind < 5.5)
	{
		float moss = smoothstep(0.59, 0.80, broad) * smoothstep(0.35, 0.70, detail)
			* mix(0.16, 0.62, grass);
		material = mix(material, material * vec3(0.69, 1.07, 0.60), moss);
	}

	// Broad pools are broken by smaller leaf-shaped patches, beneath all cast shadows.
	float pool = smoothstep(0.56, 0.83, GroundNoise(position * 0.22 + vec2(17.2, 23.8)));
	float dapple = smoothstep(0.33, 0.75, GroundNoise(position * 1.15 + vec2(3.2, 7.6)));
	float light = pool * mix(0.60, 1.0, dapple);
	material *= (0.88 + broad * 0.16) * (0.975 + grain * 0.05);
	material *= mix(vec3(0.91), vec3(1.44, 1.40, 1.20), light);

	return mix(base, max(material, vec3(0.0)), strength);
}

void main()
{
	FragColor = v_Color;

	if (v_GroundKind > 0.5)
	{
		FragColor.rgb = GroundMaterial(v_Color.rgb);
	}
}
