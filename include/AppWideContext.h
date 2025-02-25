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
	//the previous mouse pos in pixel on the monitor
	vec2		prev_mouse_pos;
	//the window's pos in pixel on screen. x and y for first window, z and w for second window.
	vec4		window_pos;
	//the mouse pos in window space. x and y for mouse from first window, z and w from second window
	vec4		window_mouse_pos;
	//the application's delta time
	float		delta_time{0.0f};
	
	
	//the projection matrix used for the camera in every scene
	mat4		proj_mat;
	//the near plane value for the camera
	float		near_plane{ 0.01f };
	//the far plane value for the camera
	float		far_plane{ 10000.0f };
	//the field of view of the camera
	float		fov{ 90.0f * DEGREES_TO_RADIANS };
	//is the user in the camera settings menu ?
	bool		in_camera_menu{ false };
	//is the user controlling a 3D camera or using the UI
	bool		in_camera_mode{ false };
	//the camera's position
	vec3		camera_pos{0.0f, 0.0f, -5.0f};
	// the camera's translate speed
	float		camera_translate_speed{ 10.0f };
	//the camera's euler rotation (it does have rotation lock). each component of the vector is the angle to rotate on said axis (roll will not exist).
	vec2		camera_rot{ 0.0f,0.0f };
	//the camera's rotational speed
	float		camera_rotational_speed{ 2.0f };
	//the camera's view matrix
	mat4		view_mat;

	//a threadPool that can be used throughout the program
	ThreadPool threadPool{};

};


__forceinline bool SceneCanShowUI(const AppWideContext& ctx) 
{
	return ctx.ui_visible && !ctx.in_camera_menu;
}

#endif //__APP_WIDE_CONTEXT_H__