#include <cstdio>
#include <GL/gl.h>
#include <GL/glu.h>
#include "mogedevents.hh"
#include "gui/skeleton_ctrl.hh"
#include "render/gridhelper.hh"
#include "render/drawskeleton.hh"
#include "appcontext.hh"
#include "entity.hh"

SkeletonCanvasController::SkeletonCanvasController(Events::EventSystem *evsys, AppContext* appContext) 
	: CanvasController(_("Skeleton"))
	, m_evsys(evsys)
	, m_grid(20.f, 0.25f)
	, m_appctx(appContext)
	, m_selected_flags(0)
{}

SkeletonCanvasController::~SkeletonCanvasController()
{
	delete[] m_selected_flags;
}

void SkeletonCanvasController::Enter() 
{
	m_drawskel.Init();
}

void SkeletonCanvasController::Render(int width, int height)
{
	glViewport(0,0,width,height);
	glClearColor(0.1f,0.1f,0.1f,1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	m_camera.Draw();
	m_grid.Draw();

	const Skeleton* skel = m_appctx->GetEntity()->GetSkeleton();
	if(skel) 
	{
		m_drawskel.SetSkeleton(skel);
		m_drawskel.Draw();
	}
}

void SkeletonCanvasController::HandleEvent(Events::Event* ev)
{
	using namespace Events;
	if(ev->GetType() == EventID_SelectBoneEvent) {
		SelectBoneEvent* bse = static_cast<SelectBoneEvent*>(ev);
		m_drawskel.SetSelected(bse->BoneIndex, bse->Selected == 1);
	} 
}


	
