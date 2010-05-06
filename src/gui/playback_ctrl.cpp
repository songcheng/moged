#include <cstdio>
#include <GL/gl.h>
#include <GL/glu.h>
#include "mogedevents.hh"
#include "gui/playback_ctrl.hh"
#include "render/gridhelper.hh"
#include "render/posehelper.hh"
#include "appcontext.hh"
#include "entity.hh"
#include "MathUtil.hh"
#include "assert.hh"
#include "playback_enum.hh"
#include "clip.hh"
#include "anim/pose.hh"
#include "anim/clipcontroller.hh"
#include "mesh.hh"

PlaybackCanvasController::PlaybackCanvasController(Events::EventSystem *evsys, AppContext* appContext) 
	: CanvasController(_("Playback"))
	, m_evsys(evsys)
	, m_grid(20.f, 0.25f)
	, m_appctx(appContext)
	, m_playing(false)
	, m_accum_time(0.f)
	, m_current_pose(0)
	, m_anim_controller(0)
{
	m_watch.Pause();
	m_anim_controller = new ClipController;
}

PlaybackCanvasController::~PlaybackCanvasController()
{
	delete m_current_pose;
	delete m_anim_controller;
}

void PlaybackCanvasController::Enter()
{
	m_drawmesh.Init();
}

void PlaybackCanvasController::Render(int width, int height)
{
	glViewport(0,0,width,height);
	glClearColor(0.1f,0.1f,0.1f,1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); 
	glEnable(GL_DEPTH_TEST);

	m_camera.Draw();
	m_grid.Draw(); 

	long newTime = m_watch.Time();
	m_watch.Start();

	if(m_playing)
		m_accum_time += (newTime) / 1000.f;

	if(m_accum_time > (1/30.f)) {
		float dt = m_accum_time;
		m_accum_time = 0.f;

		if(m_playing) {
			m_anim_controller->UpdateTime( dt );
			if(m_anim_controller->IsAtEnd()) 
				m_playing = false;
		}
	}
	m_anim_controller->ComputePose(m_current_pose);

	const Skeleton* skel = m_appctx->GetEntity()->GetSkeleton();
	if(skel) {
		const Mesh* mesh = m_appctx->GetEntity()->GetMesh();
		if(mesh)
		{
			m_current_pose->ComputeMatrices(skel, mesh->GetTransform());
			drawPose(skel, m_current_pose);
			m_drawmesh.Draw(mesh, m_current_pose);			
		}
	}

	// Update the world
	Events::PlaybackFrameInfoEvent ev;
	ev.Frame = m_anim_controller->GetFrame();
	ev.Playing = m_playing;
	m_appctx->GetEventSystem()->Send(&ev);
}

void PlaybackCanvasController::HandleEvent(Events::Event* ev)
{
	using namespace Events;
	if(ev->GetType() == EventID_ClipPlaybackEvent) {
		ClipPlaybackEvent* cpe = static_cast<ClipPlaybackEvent*>(ev);
		HandlePlaybackCommand(cpe->PlaybackType);
	}
	else if(ev->GetType() == EventID_ClipPlaybackTimeEvent) {
		ClipPlaybackTimeEvent* cpte = static_cast<ClipPlaybackTimeEvent*>(ev);
		m_playing = false;
		m_anim_controller->SetFrame( cpte->Time );
	}
	else if(ev->GetType() == EventID_ActiveClipEvent) {
		ActiveClipEvent* ace = static_cast<ActiveClipEvent*>(ev);
		SetClip(ace->ClipID);
		m_playing = true;
	} 
	else if(ev->GetType() == EventID_EntitySkeletonChangedEvent) {
		SetClip(0);
		ResetPose();
	} 
}

void PlaybackCanvasController::SetClip(sqlite3_int64 id)
{
	m_anim_controller->SetSkeleton( m_appctx->GetEntity()->GetSkeleton() );	

	const ClipDB* db = m_appctx->GetEntity()->GetClips();
	if(db == 0) {
		m_current_clip = ClipHandle();
	} else if(id == 0) {
		m_current_clip = ClipHandle();
	} else {
		m_current_clip = db->GetClip(id);
	}
		
	m_anim_controller->SetClip(m_current_clip.RawPtr());
	m_anim_controller->SetFrame(0.f);
}

void PlaybackCanvasController::ResetPose()
{
	if(m_current_pose) {
		delete m_current_pose;
		m_current_pose = 0;
	}

	const Skeleton* skel = m_appctx->GetEntity()->GetSkeleton();
	if( skel ) {
		m_current_pose = new Pose( skel );
		m_current_pose->RestPose( skel );
	}
}
	
void PlaybackCanvasController::HandlePlaybackCommand(int type)
{
	switch(type)
	{
	case PlaybackEventType_Play:
		m_playing = true;
		break;
	case PlaybackEventType_Stop:
		m_playing = false;
		break;
	case PlaybackEventType_Fwd:
	{
		float newTime = floor(m_anim_controller->GetFrame()+1.f);
		m_anim_controller->SetFrame(newTime);
		m_playing = false;
		break;
	}
	case PlaybackEventType_FwdAll:
	{
		m_anim_controller->SetToLastFrame();
		break;
	}
	case PlaybackEventType_Rewind:
	{
		float newTime = floor(m_anim_controller->GetFrame()-1.f);
		m_anim_controller->SetFrame(newTime);
		m_playing = false;
		break;
	}
	case PlaybackEventType_RewindAll:
		m_anim_controller->SetFrame(0.f);
		break;
	default: ASSERT(false); break;
	}
}
