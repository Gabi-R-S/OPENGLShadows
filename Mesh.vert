#version 420
layout(location=0)in vec3 position;
layout(location=1)in vec3 vNormal;
layout(location=2)in vec2 uv;

uniform mat4 world=mat4(vec4(1,0,0,0),vec4(0,1,0,0),vec4(0,0,1,0),vec4(0,0,0,1));
uniform mat4 viewProjection=mat4(vec4(1,0,0,0),vec4(0,1,0,0),vec4(0,0,1,0),vec4(0,0,0,1));
uniform mat4 lightProj;

out vec2 uvCoords;
out vec3 normal;
out vec4 posOnLightTexture;

void main()
{

posOnLightTexture= lightProj*world*vec4(position,1);
gl_Position=viewProjection*world*vec4(position,1);



uvCoords=uv;
normal=vNormal;
}