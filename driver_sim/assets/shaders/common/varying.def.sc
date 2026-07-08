vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
vec3 v_viewPosition  : TEXCOORD1 = vec3(0.0, 0.0, 0.0);
vec3 v_viewNormal  : NORMAL = vec3(0.0, 0.0, 0.0);
vec4 v_currentPosition : TEXCOORD2 = vec4(0.0, 0.0, 0.0, 1.0);
vec4 v_previousPosition : TEXCOORD3 = vec4(0.0, 0.0, 0.0, 1.0);
vec3 v_worldPosition  : TEXCOORD4 = vec3(0.0, 0.0, 0.0);

vec4 i_data0 : TEXCOORD7;
vec4 i_data1 : TEXCOORD6;
vec4 i_data2 : TEXCOORD5;
vec4 i_data3 : TEXCOORD4;
vec4 i_data4 : TEXCOORD3;
vec4 i_data5 : TEXCOORD2;

vec3 a_position  : POSITION;
vec4 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;

uint gl_FragData[2];