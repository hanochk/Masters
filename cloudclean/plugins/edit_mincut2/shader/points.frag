#version 330
 
layout(location = 0) out vec4 fragColor;

uniform vec4 colour;

void main( void )
{
    fragColor = colour;
}
