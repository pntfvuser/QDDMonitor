uniform sampler2D texture0;
uniform lowp float opacity;
varying highp vec2 texCoord;

void main(void)
{
    gl_FragColor = vec4(texture2D(texture0, texCoord).rgb, opacity);
}
