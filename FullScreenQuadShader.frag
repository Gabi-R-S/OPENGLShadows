
#version 420 
out vec4 FragColor;
  
in vec2 texCoords;

uniform layout(binding=0) sampler2D depthMap;
uniform int invert=1;
void main()
{             
    vec4 color = texture(depthMap, texCoords);
    FragColor = vec4( ((vec3(1,1,1)-color.rgb)*invert) + (color.rgb*(1-invert)), color.a);
} 