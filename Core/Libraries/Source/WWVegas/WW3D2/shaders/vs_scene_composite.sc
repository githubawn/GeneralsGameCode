$input  a_position
$output v_texcoord0

// TheSuperHackers @feature bobtista 27/04/2026 Fullscreen scene
// composite vertex shader. The scene color target is sampled back to
// the swapchain before UI draws, creating the post-processing hook.

#include <bgfx_shader.sh>

// TheSuperHackers @bugfix githubawn 21/06/2026 u_postTexelSize.z carries a V-flip
// flag (1.0 on bottom-left-origin renderers like GLES, 0.0 on DX11/Metal) so the
// scene render target is sampled the right way up when the composite pass is used
// (e.g. the downscaled render-resolution path on Android).
uniform vec4 u_postTexelSize;

void main()
{
	gl_Position = vec4(a_position, 1.0);
	float u = a_position.x * 0.5 + 0.5;
	float v = 0.5 - a_position.y * 0.5;
	if (u_postTexelSize.z > 0.5)
	{
		v = 1.0 - v;
	}
	v_texcoord0 = vec2(u, v);
}
