#ifndef __SCENE_H__
#define __SCENE_H__

#include <stdint.h>

//forward declaration
namespace rapidjson
{
	template<typename CharType = char>
	struct UTF8;

	class CrtAllocator;

	template <typename BaseAllocator = CrtAllocator>
	class MemoryPoolAllocator;

	template <typename Encoding, typename Allocator = MemoryPoolAllocator<> >
	class GenericValue;

	typedef GenericValue<UTF8<> > Value;
}


/**
* This a class representing the familiar scene known in different other engines.
* The only different is that I am using the semantic field of plays.
*/
class Scene
{
public:
	bool enabled = false;

	/*
	* Imports all the data user can change, saved from each use.
	*/
	virtual void Import(const rapidjson::Value& AppSettings) = 0;

	/*
	* Exports all the data user can change in the document, to save it for next use.
	*/
	virtual void Export(rapidjson::Value& AppSettings, rapidjson::MemoryPoolAllocator<>& Allocator) = 0;

	/*
	* Prepares once all the unmovable resources needed (such as pipelines in recent Graphics APIs)
	*/
	virtual void Prepare(class GraphicsAPIManager& GAPI) = 0;

	/*
	* Allocates the resources associated with window (which can change size during runtime), and deallocate previously allocated resources if needed.
	* This is expected to be called multiple time, do not put resources that are supposed to be created once in this method.
	*/
	virtual void Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames) = 0;

	/*
	* The name of this scene. This can be hardcoded or a member, but it should never fail.
	*/
	virtual const char* Name()const noexcept = 0;

	/*
	* called once per frame.
	* Changes data inside the scene depending on time or user input that can be found in the AppContext (or with ImGUI)
	* UI should also be updated here.
	*/
	virtual void Act(struct AppWideContext& AppContext) = 0;

	/*
	* called once per frame.
	* Uses the data changed in Act and the resources to display on screen the scene's current context.
	* Should create and send the render command to the GPU.
	*/
	virtual void Show(struct GAPIHandle& GAPIHandle) = 0;

	/*
	* Releases all the resources allocated in Prepare
	*/
	virtual void Close(class GraphicsAPIManager& GAPI) = 0;
};

#endif //__SCENE_H__