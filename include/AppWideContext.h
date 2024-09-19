#ifndef __APP_WIDE_CONTEXT_H__
#define __APP_WIDE_CONTEXT_H__

#include "Maths.h"

struct AppWideContext
{
	//is ui window opened and visible
	bool		ui_visible{true};
	//a scene can be viewed one at a time, this is the index in the array
	uint32_t	scene_index{0};
	//the mouse pos in pixel on the monitor
	vec2		mouse_pos;
	//the window's pos in pixel on screen. x and y for first window, z and w for second window.
	vec4		window_pos;
	//the mouse pos in window space. x and y for mouse from first window, z and w from second window
	vec4		window_mouse_pos;
	//the application's delta time
	float		delta_time{0.0f};

};

#endif //__APP_WIDE_CONTEXT_H__