uniform highp mat4 matrix;
attribute highp vec4 positionIn;
attribute highp vec2 texCoordIn;
varying highp vec2 texCoord;

void main()
{
    gl_Position = matrix * positionIn;
    texCoord = texCoordIn;
}
