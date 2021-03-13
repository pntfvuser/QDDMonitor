uniform highp mat4 matrix;
attribute highp vec4 position;
attribute highp vec2 texCoordIn;
varying highp vec2 texCoord;

void main()
{
    gl_Position = matrix * position;
    texCoord = texCoordIn;
}
