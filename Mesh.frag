#version 420

uniform layout (binding=0) sampler2D paletteTexture;

uniform vec4 shadowColor=vec4(0.6,0.5,0.7,1.0);

uniform layout (binding=1) sampler2D lightTexture;

in vec2 uvCoords;
in vec3 normal;
in vec4 posOnLightTexture;

out layout(location=0) vec4 color;


uniform float bias=0.01;

void main()
{
color=texture(paletteTexture,uvCoords);
vec4 darkColor=color*shadowColor;

int isInShadow;



vec3 projCoords = posOnLightTexture.xyz/posOnLightTexture.w;
projCoords = projCoords * 0.5 + 0.5; 
float closestDepth = texture(lightTexture, projCoords.xy).r;   

float currentDepth = posOnLightTexture.z;  


if(currentDepth>=closestDepth+bias)
	{
isInShadow= 1;
	}else{
isInShadow= 0;
}




color=color+(darkColor-color)*isInShadow;


}