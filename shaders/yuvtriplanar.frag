uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;
varying highp vec2 texCoord;

void main()
{
    mediump float Y = texture2D(texture0, texCoord).r;
    mediump float U = texture2D(texture1, texCoord).r;
    mediump float V = texture2D(texture2, texCoord).r;
    mediump vec4 colorYUV = vec4(Y, U, V, 1.0);
    mediump vec4 colorRGB = colorMatrix * colorYUV;
    gl_FragColor = vec4(colorRGB.rgb, 1.0) * opacity;
}
