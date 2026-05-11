#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out mat3 TBN;

uniform mat4 world;
uniform mat4 view;
uniform mat4 projection;

uniform float useNormalMap;

void main()
{
	TexCoord = aTexCoord;

	// normal is always transformed into world space for lighting calc
	Normal = normalize(mat3(transpose(inverse(world))) * aNormal);
	FragPos = vec3(world * vec4(aPos, 1.0));

	// TBN matrix for normal mapping
	vec3 T = normalize(vec3(world * vec4(aTangent, 0.0)));
	vec3 N = normalize(mat3(transpose(inverse(world))) * aNormal);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T);
	TBN = mat3(T, B, N);

	gl_Position = projection * view * vec4(FragPos, 1.0);
}
