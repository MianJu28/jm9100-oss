#version 300 es
precision highp float;
in vec4 dbg;
out vec4 oFragColor;
void main(){ oFragColor = dbg * 0.5 + 0.5; }
