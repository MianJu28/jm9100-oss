#version 300 es
in vec2 aPosition;
uniform highp sampler2D sGpuCache;
out vec4 dbg;
void main(){
  dbg = texelFetch(sGpuCache, ivec2(0,0), 0);
  gl_Position = vec4(aPosition.xy*2.0-vec2(1.0), 0.0, 1.0);
}
