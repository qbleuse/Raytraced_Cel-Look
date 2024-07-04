#ifndef __SCENE_H__
#define __SCENE_H__

/**
* This a class representing the familiar scene known in different other engines.
* The only different is that I am using the lexical field of plays.
*/
class Scene
{
public:
	virtual void Prepare(class GraphicsAPIManager& GAPI) = 0;

	virtual void Act(struct AppWideContext& AppContext) = 0;

	virtual void Show(class GraphicsAPIManager& GAPI) = 0;

	virtual void Close(class GraphicsAPIManager& GAPI) = 0;
};

#endif //__SCENE_H__