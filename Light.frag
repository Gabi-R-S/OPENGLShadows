#version 420
out layout(location=0) float color;

void main()
{
color=gl_FragCoord.z/gl_FragCoord.w;

}