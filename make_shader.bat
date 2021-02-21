call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
fxc /E NullVertexShader /Fh NullVertexShader.h /T vs_5_0 /O3 NullVertexShader.hlsl
fxc /E RGBXPixelShader /Fh RGBXPixelShader.h /T ps_5_0 /O3 RGBXPixelShader.hlsl
fxc /E YUVJ444PPixelShader /Fh YUVJ444PPixelShader.h /T ps_5_0 /O3 YUVJ444PPixelShader.hlsl
fxc /E NV12PixelShader /Fh NV12PixelShader.h /T ps_5_0 /O3 NV12PixelShader.hlsl
pause