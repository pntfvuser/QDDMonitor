uniform sampler2D texture0;
uniform sampler2D texture1;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;
varying highp vec2 texCoord;

void main()
{
    mediump float Y = texture2D(texture0, texCoord).r;
    mediump vec2 UV = texture2D(texture1, texCoord).rg;
    mediump vec4 colorYUV = vec4(Y, UV, 1.0);
    mediump vec4 colorRGB = colorMatrix * colorYUV;
    gl_FragColor = vec4(colorRGB.rgb, 1.0) * opacity;
}
