#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 world;
uniform mat4 view;
uniform mat4 projection;

out vec3 WorldPosition;

void main()
{
    WorldPosition = vec3(world * vec4(aPosition, 1.0));
    gl_Position = projection * view * vec4(WorldPosition, 1.0);
}
