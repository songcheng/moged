#ifndef __mogedMotionGraphEditor__
#define __mogedMotionGraphEditor__

/**
@file
Subclass of MotionGraphEditor, which is generated by wxFormBuilder.
*/

#include <list>
#include <vector>
#include "MogedFrames.h"

class AppContext;
class MGEdge;
class ClipDB;
class MotionGraph;
class Pose;
class ClipController;
class Vec3;
class Skeleton;
class Mesh;

/** Implementing MotionGraphEditor */
class mogedMotionGraphEditor : public MotionGraphEditor
{
	enum { TIMING_SAMPLES = 30 };

	AppContext *m_ctx;
	int m_current_state;
	
	typedef std::pair<const MGEdge*,const MGEdge*> EdgePair;
	std::list< EdgePair > m_edge_pairs;

	void ReadSettings();
	void CreateWorkListAndStart(const Skeleton* skel, const Mesh* mesh, const MotionGraph* graph);
	void CreateTransitionWorkListAndStart(const MotionGraph* graph);
	bool ProcessNextTransition(std::ostream& out);
	void UpdateTiming(float num_per_sec);
	struct TransitionWorkingData
	{
		std::vector< int > sample_verts; // vert indices to sample
		int num_clouds;
		Vec3 **clouds;
		int *cloud_lengths;

		float processed_per_second[TIMING_SAMPLES];
		int next_sample_idx;
		TransitionWorkingData();
		~TransitionWorkingData() { clear(); }
		void clear();
	};
	
	struct TransitionFindingData
	{
		int from_idx;
		int to_idx;
		const MGEdge* from;
		const MGEdge* to;
		int from_frame;
		int from_max;
		int to_frame;
		int to_max;
		TransitionFindingData() { clear(); }
		void clear();		
	};

	struct Settings {
		int num_threads;
		float error_threshold;
		float point_cloud_rate;
		float transition_length; // in seconds
		float sample_rate; // fps to sample at

		int num_samples; // number of samples to gather given the above
		float sample_interval; // time per sample.
		Settings() { clear(); }
		void clear();
	};

	TransitionFindingData m_transition_finding;
	Settings m_settings;
	TransitionWorkingData m_working;

protected:
	// Handlers for MotionGraphEditor events.
	void OnIdle( wxIdleEvent& event );
	void OnPageChanged( wxListbookEvent& event );
	void OnPageChanging( wxListbookEvent& event );
	void OnScrollErrorThreshold( wxScrollEvent& event );
	void OnEditErrorThreshold( wxCommandEvent& event );
	void OnScrollCloudSampleRate( wxScrollEvent& event );
	void OnEditCloudSampleRate( wxCommandEvent& event );
	void OnCreate( wxCommandEvent& event );
	void OnCancel( wxCommandEvent& event );
	void OnPause( wxCommandEvent& event );
	void OnNext( wxCommandEvent& event );
	void OnContinue( wxCommandEvent& event );
	void OnNextStage( wxCommandEvent& event );
	void OnTransitionLengthChanged( wxCommandEvent& event ) ;

public:
	/** Constructor */
	mogedMotionGraphEditor( wxWindow* parent, AppContext* ctx );
};

#endif // __mogedMotionGraphEditor__