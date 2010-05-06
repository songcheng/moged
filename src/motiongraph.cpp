#include <ostream>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <cstdio>
#include "sql/sqlite3.h"
#include "motiongraph.hh"
#include "assert.hh"
#include "clip.hh"
#include "clipdb.hh"
#include "mesh.hh"
#include "Vector.hh"
#include "anim/clipcontroller.hh"
#include "anim/pose.hh"
#include "Mat4.hh"
#include "skeleton.hh"
#include "assert.hh"

using namespace std;

sqlite3_int64 NewMotionGraph( sqlite3* db, sqlite3_int64 skel )
{
	Query new_mg(db, "INSERT INTO motion_graphs (skel_id) VALUES(?)");
	new_mg.BindInt64(1, skel);
	new_mg.Step();
	return new_mg.LastRowID();
}

MotionGraph::MotionGraph(sqlite3* db, sqlite3_int64 skel_id, sqlite3_int64 id)
	: m_db(db)
	, m_skel_id(skel_id)
	, m_id(id)
	, m_stmt_count_edges (db)
	, m_stmt_count_nodes (db)
	, m_stmt_insert_edge (db)
	, m_stmt_insert_node (db)
	, m_stmt_get_edges (db)
	, m_stmt_get_edge(db)
{

	Query check_id(m_db, "SELECT id FROM motion_graphs WHERE id = ? and skel_id = ?");
	check_id.BindInt64(1, id);
	check_id.BindInt64(2, skel_id);
	m_id = 0;
	if( check_id.Step()) {
		m_id = check_id.ColInt64(0);
	}
	if(Valid())
		PrepareStatements();
}

void MotionGraph::PrepareStatements()
{
	m_stmt_count_edges.Init("SELECT count(*) FROM motion_graph_edges WHERE motion_graph_id = ?");
	m_stmt_count_edges.BindInt64(1, m_id);

	m_stmt_count_nodes.Init("SELECT count(*) FROM motion_graph_nodes WHERE motion_graph_id = ?");
	m_stmt_count_nodes.BindInt64(1, m_id);
	
	m_stmt_insert_edge.Init("INSERT INTO motion_graph_edges "
							"(motion_graph_id, clip_id, start_id, finish_id) "
							"VALUES (?,?,?,?)");
	m_stmt_insert_edge.BindInt64(1, m_id);

	m_stmt_insert_node.Init("INSERT INTO motion_graph_nodes (motion_graph_id) VALUES ( ? )");
	m_stmt_insert_node.BindInt64(1, m_id);

	m_stmt_get_edges.Init("SELECT id FROM motion_graph_edges WHERE motion_graph_id = ?");
	m_stmt_get_edges.BindInt64(1, m_id);

	m_stmt_get_edge.Init("SELECT clip_id,start_id,finish_id FROM motion_graph_edges WHERE motion_graph_id = ? AND id = ?");
	m_stmt_get_edge.BindInt64(1, m_id);
}

MotionGraph::~MotionGraph()
{
}

sqlite3_int64 MotionGraph::AddEdge(sqlite3_int64 start, sqlite3_int64 finish, sqlite3_int64 clip_id)
{
	m_stmt_insert_edge.Reset();
	m_stmt_insert_edge.BindInt64(2, clip_id);
	m_stmt_insert_edge.BindInt64(3, start);
	m_stmt_insert_edge.BindInt64(4, finish);
	m_stmt_insert_edge.Step();
	return m_stmt_insert_edge.LastRowID();
}

sqlite3_int64 MotionGraph::AddNode()
{
	m_stmt_insert_node.Reset();
	m_stmt_insert_node.Step();
	return m_stmt_insert_node.LastRowID();
}

void MotionGraph::GetEdgeIDs(std::vector<sqlite3_int64>& out) const
{
	out.clear();
	m_stmt_get_edges.Reset();
	while(m_stmt_get_edges.Step()) {
		out.push_back( m_stmt_get_edges.ColInt64(0) );
	}
}

MGEdgeHandle MotionGraph::GetEdge(sqlite3_int64 id) 
{
	m_stmt_get_edge.Reset();
	m_stmt_get_edge.BindInt64(2, id);

	if(m_stmt_get_edge.Step()) 
	{
		sqlite3_int64 clip_id = m_stmt_get_edge.ColInt64(0);
		sqlite3_int64 start_id = m_stmt_get_edge.ColInt64(1);
		sqlite3_int64 finish_id = m_stmt_get_edge.ColInt64(2);

		MGEdgeHandle result = new MGEdge(m_db, id, clip_id, start_id, finish_id);
		return result;		
	} else 
		return MGEdgeHandle();
}

const MGEdgeHandle MotionGraph::GetEdge(sqlite3_int64 id) const
{
	m_stmt_get_edge.Reset();
	m_stmt_get_edge.BindInt64(2, id);

	if(m_stmt_get_edge.Step()) 
	{
		sqlite3_int64 clip_id = m_stmt_get_edge.ColInt64(0);
		sqlite3_int64 start_id = m_stmt_get_edge.ColInt64(1);
		sqlite3_int64 finish_id = m_stmt_get_edge.ColInt64(2);

		MGEdgeHandle result = new MGEdge(m_db, id, clip_id, start_id, finish_id);
		return result;		
	} else 
		return MGEdgeHandle();
}


int MotionGraph::GetNumEdges() const 
{
	m_stmt_count_edges.Reset();
	if(m_stmt_count_edges.Step()) {
		return m_stmt_count_edges.ColInt(0);
	}
	return 0;
}

int MotionGraph::GetNumNodes() const 
{
	m_stmt_count_nodes.Reset();
	if (m_stmt_count_nodes.Step()) {
		return m_stmt_count_nodes.ColInt(0);
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
MGEdge::MGEdge( sqlite3* db, sqlite3_int64 id, sqlite3_int64 clip, sqlite3_int64 start, sqlite3_int64 finish )
	: m_db(db)
	, m_id(id)
	, m_clip_id(clip)
	, m_start_id(start)
	, m_finish_id(finish)
	, m_error_mode(false)
{
}

ClipHandle MGEdge::GetClip()
{
	CacheHandle();
	return m_cached_clip;
}

const ClipHandle MGEdge::GetClip() const
{
	CacheHandle();
	return m_cached_clip;
}

void MGEdge::CacheHandle() const
{
	if(!m_error_mode && !m_cached_clip) {
		m_cached_clip = new Clip(m_db, m_clip_id);
		if(!m_cached_clip->Valid())
		{
			m_error_mode = true;
			m_cached_clip = 0;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void populateInitialMotionGraph(MotionGraph* graph, 
								const ClipDB* clips,
								std::ostream& out)
{
	std::vector<sqlite3_int64> clip_ids;
	clips->GetClipIDs(clip_ids);

	const int num_clips = clip_ids.size();
	for(int i = 0; i < num_clips; ++i)
	{
		sqlite3_int64 start = graph->AddNode();
		sqlite3_int64 end = graph->AddNode();
		graph->AddEdge( start, end, clip_ids[i] );
	}
	
	out << "Using " << num_clips << " clips." << endl <<
		"Created graph with " << graph->GetNumEdges() << " edges and " << graph->GetNumNodes() << " nodes." << endl;
}

// rand index over vertex buffer. 
// This may not give the best distribution over the character necessarily since
// it doesn't take into account density of mesh (where we probably want number of points
// in an area to be roughly the same).
// can try a stratified sampling approach if this is bad.
void selectMotionGraphSampleVerts(const Mesh* mesh, int num, std::vector<int> &out)
{
	std::vector<int> candidates ;
	const int num_verts = mesh->GetNumVerts();
	candidates.reserve(num_verts);
	for(int i = 0; i < num_verts; ++i) 
		candidates.push_back(i);
	
	out.clear();
	out.reserve(num);

	for(int i = 0; i < num; ++i) 
	{
		int rand_index = rand() % candidates.size();

		out.push_back(candidates[rand_index]);
		candidates.erase( candidates.begin() + rand_index );
	}

	// sort the list so we at least access data in a predictable way
	std::sort( out.begin(), out.end());
}

// Get point from a clip controller. 
// 'samples' must be of at least num_samples * sample_indicies.size() in length.
// It begins by starting at a particular frame, and then sampling every sample_interval for num_samples iterations.
// This allows for clips that are running at variable FPS to be used.
// Pose must be initialized with the same skeleton as the clip_controller.

static void poseSamples(Vec3* out, int num_samples, const std::vector<int>& sample_indices, 
						const Mesh* mesh, const Pose* pose)
{
	const float *vert_data = mesh->GetPositionPtr();
	const char *mat_indices = mesh->GetSkinMatricesPtr();
	const float *weights = mesh->GetSkinWeightsPtr();
	
	const Mat4* mats = pose->GetMatricesPtr();

	Vec3 v0,v1,v2,v3;
	int i = 0;

	for(i = 0; i < num_samples; ++i) {
		int sample = sample_indices[i];
		int vert_sample = sample*3;
		int mat_sample = sample*4;
		Vec3 mesh_vert(vert_data[vert_sample],vert_data[vert_sample+1],vert_data[vert_sample+2]);
		const float *w = &weights[mat_sample];
		const char* indices = &mat_indices[mat_sample];
		ASSERT(indices[0] < pose->GetNumJoints() && 
			   indices[1] < pose->GetNumJoints() && 
			   indices[2] < pose->GetNumJoints() && 
			   indices[3] < pose->GetNumJoints());
		const Mat4& m1 = mats[ int(indices[0]) ];
		const Mat4& m2 = mats[ int(indices[1]) ];
		const Mat4& m3 = mats[ int(indices[2]) ];
		const Mat4& m4 = mats[ int(indices[3]) ];
	
		v0 = transform_point( m1, mesh_vert );
		v1 = transform_point( m2, mesh_vert );
		v2 = transform_point( m3, mesh_vert );
		v3 = transform_point( m4, mesh_vert );

		(void)w;
		out[i] = w[0] * v0 + 
			w[1] * v1 + 
			w[2] * v2 + 
			w[3] * v3;
	}
}

void getPointCloudSamples(Vec3* samples, 
						  const Mesh* mesh,
						  const Skeleton* skel, 
						  const std::vector<int>& sample_indices, 
						  const Clip* clip,
						  int num_samples, 
						  float sample_interval,
						  int numThreads)
{
	ASSERT(samples);
	ASSERT(numThreads > 0);
	int i;
	int frame_offset, tid;
	// init
	const int frame_length_in_samples = sample_indices.size();
	Pose** poses = new Pose*[numThreads];
	for(i = 0; i < numThreads; ++i) {
		poses[i] = new Pose(skel);
	}
	ClipController *controllers = new ClipController[numThreads];
	for(int i = 0; i < numThreads; ++i) {
		controllers[i].SetSkeleton(skel);
		controllers[i].SetClip(clip);
	}

	memset(samples, 0, sizeof(Vec3)*num_samples*frame_length_in_samples);

	omp_set_num_threads(numThreads);

	// processing
#pragma omp parallel for												\
	shared(poses,controllers,mesh,skel,sample_indices,samples,sample_interval,num_samples) \
	private(i,tid,frame_offset)
	for(i = 0; i < num_samples; ++i)
	{
		tid = omp_get_thread_num();
		ASSERT(tid < numThreads);
		frame_offset = i * frame_length_in_samples;

		controllers[tid].SetTime( i * sample_interval );
		controllers[tid].ComputePose(poses[tid]);

		poses[tid]->ComputeMatrices(skel, mesh->GetTransform());

		poseSamples(&samples[frame_offset],
					frame_length_in_samples,
					sample_indices, 
					mesh, 
					poses[tid]);
	}

	// clean up
	for(int i = 0; i < numThreads; ++i) {
		delete poses[i];
	}
	delete[] poses;
	delete[] controllers;
}

void computeCloudAlignment(const Vec3* from_cloud,
						   const Vec3* to_cloud,
						   int points_per_frame,
						   int num_frames,
						   const float *weights,
						   float inv_total_weights,
						   Vec3& align_translation,
						   float& align_rotation,
						   int numThreads)
{
	// compute according to minimization solution in kovar paper
	omp_set_num_threads(numThreads);

	const int total_num_samples = num_frames * points_per_frame;
	double total_from_x = 0.f, total_from_z = 0.f;
	double total_to_x = 0.f, total_to_z = 0.f;
	int i = 0;
	// a = w * (x * z' - x' * z) sum over i
	double a = 0.f;
	// b = w * (x * x' + z * z') sum over i
	double b = 0.f;
	double w = 0.f;

#pragma omp parallel for private(i,w) shared(from_cloud,to_cloud,weights) reduction(+:total_from_x,total_from_z,total_to_x,total_to_z,a,b)
	for(i = 0; i < total_num_samples; ++i)
	{
		w = weights[i];
		total_from_x += from_cloud[i].x * w;
		total_from_z += from_cloud[i].z * w;
		total_to_x += to_cloud[i].x * w;
		total_to_z += to_cloud[i].z * w;		
		a += w * (from_cloud[i].x * to_cloud[i].z - to_cloud[i].x * from_cloud[i].z);
		b += w * (from_cloud[i].x * to_cloud[i].x + from_cloud[i].z * to_cloud[i].z);
	}

	double angle = atan2( a - double(inv_total_weights) * (total_from_x * total_to_z - total_to_x * total_from_z),
						  b - double(inv_total_weights) * (total_from_x * total_to_x + total_from_z * total_to_z) );

	double cos_a = cos(angle);
	double sin_a = sin(angle);

	float x = double(inv_total_weights) * (total_from_x - total_to_x * cos_a - total_to_z * sin_a);
	float z = double(inv_total_weights) * (total_from_z + total_to_x * sin_a - total_to_z * cos_a);
	
	align_translation.set(x,0,z);
	align_rotation = float(angle);
}


float computeCloudDifference(const Vec3* from_cloud,
							 const Vec3* to_cloud,
							 const float* weights,
							 int points_per_frame,
							 int num_frames,
							 Vec3_arg align_translation,
							 float align_rotation,
							 int numThreads)
{
	omp_set_num_threads(numThreads);

	Mat4 transform = translation(align_translation) * rotation_y(align_rotation);
	const int total_num_samples = points_per_frame * num_frames;
	int i = 0;

	float diff = 0.f;

#pragma omp parallel for private(i) shared(transform, weights, from_cloud, to_cloud) reduction(+:diff)
	for(i = 0; i < total_num_samples; ++i)
	{
		float w = weights[i];
		Vec3 from = from_cloud[i];
		Vec3 to = transform_point(transform,to_cloud[i]);
		float magsq = w * magnitude_squared(from-to);
		diff += magsq;
	}

	return diff;
}

void findErrorFunctionMinima(const float* error_values, int width, int height, std::vector<int>& out_minima_indices)
{
	out_minima_indices.clear();

	const int window_size = 3;
	const int total = width*height;
	int i = 0;

	int* is_minima = new int[total];
	for(i = 0; i < total; ++i) is_minima[i] = 1;
	
#pragma omp parallel for private(i) shared(is_minima, width, height, error_values)
	for(i = 0; i < total; ++i) 
	{
		int x = i % width;
		int y = i / width;

		float middle = error_values[i];
		
		int min_x = Max(0, x - window_size);
		int max_x = Min(width, x + window_size);
		int min_y = Max(0, y - window_size);
		int max_y = Min(height, y + window_size);
		
		for(int cur_y = min_y; cur_y < max_y; ++cur_y)
		{			
			int offset = cur_y * width + min_x;
			for(int cur_x = min_x; cur_x < max_x; ++cur_x)
			{
				if(offset != i) {
					float val = error_values[offset];
					if(val < middle) {
						is_minima[i] = 0;
						break;
					}				
				}
				++offset;
			}
		}
	}

	for(i = 0; i < total; ++i) 
		if(is_minima[i]) 
			out_minima_indices.push_back(i);

	delete[] is_minima;
}

