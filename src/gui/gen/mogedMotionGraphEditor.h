#ifndef __mogedMotionGraphEditor__
#define __mogedMotionGraphEditor__

/**
@file
Subclass of MotionGraphEditor, which is generated by wxFormBuilder.
*/

#include <list>
#include <vector>
#include "MogedFrames.h"
#include "Vector.hh"
#include "clipdb.hh"
#include "motiongraph.hh"

class AppContext;
class MGEdge;
class ClipDB;
class MotionGraph;
class Pose;
class ClipController;
class Vec3;
class Skeleton;
class Mesh;
class SkeletonWeights;

/** Implementing MotionGraphEditor */
class mogedMotionGraphEditor : public MotionGraphEditor
{
	enum { TIMING_SAMPLES = 30 };

	AppContext *m_ctx;
	int m_current_state;

	typedef std::pair<int,int> EdgePair;
	std::list< EdgePair > m_edge_pairs;

	bool m_stepping;

	void ReadSettings();
	void CreateWorkListAndStart(const MotionGraph* graph);
	void CreateTransitionWorkListAndStart(std::ostream& out);
	bool ProcessNextTransition();
	void UpdateTiming(float num_per_sec);
	void PublishCloudData(bool do_align, Vec3_arg align_translation, float align_rotation, 
						  int from_offset = 0, int from_len = -1, int to_offset = 0, int to_len = -1);
	void InitJointWeights(const SkeletonWeights& weights, const Mesh* mesh);

	void RestoreSavedSettings();
	void SaveSettings();
	void ExtractTransitionCandidates();

	struct TransitionCandidate {
		int from_edge_idx; 
		int to_edge_idx;

		int from_frame;
		int to_frame;

		float error;
		Vec3 align_translation;
		float align_rotation;
	};

	struct TransitionWorkingData
	{
		std::vector< int > sample_verts; // vert indices to sample
		int num_clouds;
		Vec3 **clouds;
		int *cloud_lengths;

		float processed_per_second[TIMING_SAMPLES];
		int next_sample_idx;

		float *joint_weights; // len = m_settings.num_samples * sample_verts.size() // 1 weight per point per transition cloud
		float inv_sum_weights;

		std::vector< TransitionCandidate > transition_candidates;

		std::vector< MGEdgeHandle > working_set; // clips we are considering
		
		TransitionWorkingData();
		~TransitionWorkingData() { clear(); }
		void clear();
	};
	
	struct TransitionFindingData
	{
		int from_idx;
		int to_idx;
		MGEdgeHandle from;
		MGEdgeHandle to;
		int from_frame;
		int from_max;
		int to_frame;
		int to_max;

		float* error_function_values;
		Vec3* alignment_translations;
		float* alignment_angles;
		std::vector<int> minima_indices;

		TransitionFindingData();
		~TransitionFindingData() { clear(); }
		void clear();		
	};

	struct Settings {
		int num_threads;
		float error_threshold;
		float point_cloud_rate;
		float transition_length; // in seconds
		float sample_rate; // fps to sample at
		float weight_falloff;

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
	void OnScrollFalloff( wxScrollEvent& event );
	void OnEditFalloff( wxCommandEvent& event );
	void OnCreate( wxCommandEvent& event );
	void OnCancel( wxCommandEvent& event );
	void OnPause( wxCommandEvent& event );
	void OnNext( wxCommandEvent& event );
	void OnContinue( wxCommandEvent& event );
	void OnNextStage( wxCommandEvent& event );
	void OnViewDistanceFunction( wxCommandEvent& event );
	void OnTransitionLengthChanged( wxCommandEvent& event ) ;
	void OnClose( wxCloseEvent& event ) ;

public:
	/** Constructor */
	mogedMotionGraphEditor( wxWindow* parent, AppContext* ctx );
	~mogedMotionGraphEditor() ;
};

#endif // __mogedMotionGraphEditor__
