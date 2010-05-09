#include <cstdio>
#include "entity.hh"
#include "skeleton.hh"
#include "clipdb.hh"
#include "mesh.hh"
#include "lbfloader.hh"
#include "motiongraph.hh"
#include "mogedevents.hh"
#include "sql/sqlite3.h"

Entity::Entity(	Events::EventSystem* evsys)
	: m_evsys(evsys)
	, m_db(0)
	, m_skeleton(0)
	, m_clips(0)
	, m_mesh(0)
	, m_mg(0)
{
}

Entity::~Entity()
{
	delete m_skeleton;
	delete m_clips;
	delete m_mesh;
	delete m_mg;

	sqlite3_close(m_db);	
}

void Entity::SetFilename( const char* filename )
{
	Reset();
	sqlite3_close(m_db); m_db = 0;

	if( sqlite3_open(filename, &m_db) == SQLITE_OK )
	{
		Query enable_fk(m_db, "PRAGMA foreign_keys = ON");
		enable_fk.Step();
		
		sqlite3_busy_timeout(m_db, 1000);

		CreateMissingTables();

		sqlite3_int64 skelid;
		if(FindFirstSkeleton(&skelid)) {
			SetCurrentSkeleton(skelid);
		}
	}
}

void Entity::Reset()
{
	delete m_skeleton;m_skeleton=0;
	delete m_clips;m_clips=0;
	delete m_mesh;m_mesh=0;
	delete m_mg;m_mg=0;

	Events::EntitySkeletonChangedEvent ev;
	m_evsys->Send(&ev);
}

sqlite3_int64 Entity::GetCurrentSkeleton() const 
{
	if(m_skeleton) return m_skeleton->GetID();
	else return 0;
}

void Entity::SetCurrentSkeleton(sqlite3_int64 id)
{
	Reset();
	
	Skeleton* newSkel = new Skeleton(m_db, id);
	if(newSkel->Valid())
	{
		m_skeleton = newSkel;
		m_clips = new ClipDB(m_db, id);

		sqlite3_int64 mg_id;
		if(FindFirstMotionGraph(id, &mg_id))
			m_mg = new MotionGraph(m_db, id, mg_id);

		sqlite3_int64 meshid;
		if(FindFirstMesh(id, &meshid)) {
			m_mesh = new Mesh(m_db, id, meshid);
		}
	} else {
		delete newSkel; 
	}
		
	Events::EntitySkeletonChangedEvent ev;
	m_evsys->Send(&ev);		
}

void Entity::SetCurrentMesh(sqlite3_int64 id)
{
	delete m_mesh; m_mesh = 0;
	if(m_skeleton) {
		m_mesh = new Mesh(m_db, m_skeleton->GetID(), id);
		if(!m_mesh->Valid()) {
			delete m_mesh; m_mesh = 0;
		}			
	}
	Events::EntitySkeletonChangedEvent ev;
	m_evsys->Send(&ev);		
}

void Entity::SetCurrentMotionGraph(sqlite3_int64 id)
{
	delete m_mg; m_mg = 0;
	if(m_skeleton) {
		m_mg = new MotionGraph(m_db, m_skeleton->GetID(), id);
		if(!m_mg->Valid()) {
			delete m_mg; m_mg = 0;
		}
	}
}
bool Entity::FindFirstSkeleton(sqlite3_int64 *skel_id)
{
	if(skel_id == 0) return false;
	*skel_id = 0;

	Query find(m_db, "SELECT id FROM skeleton ORDER BY id ASC LIMIT 1");
	if( find.Step() )
		*skel_id = find.ColInt64(0);
	return *skel_id != 0;
}

bool Entity::FindFirstMesh (sqlite3_int64 skel_id, sqlite3_int64* mesh_id)
{
	if(mesh_id == 0) return false;
	*mesh_id = 0;

	Query find(m_db, "SELECT id FROM meshes WHERE skel_id=? ORDER BY id ASC LIMIT 1");
	find.BindInt64(1, skel_id);
	if( find.Step()) 
		*mesh_id = find.ColInt64(0);
	return *mesh_id != 0;
}

bool Entity::FindFirstMotionGraph(sqlite3_int64 skel_id, sqlite3_int64* mg_id)
{
	if(mg_id == 0) return false;
	*mg_id = 0;
   
	Query find(m_db, "SELECT id FROM motion_graphs WHERE skel_id = ? ORDER BY id ASC LIMIT 1");
	find.BindInt64(1, skel_id);
	if(find.Step())
		*mg_id = find.ColInt64(0);
	return *mg_id != 0;
}

void Entity::GetSkeletonInfos(std::vector<SkeletonInfo>& out) const
{
	out.clear();
	Query query(m_db, "SELECT id,name FROM skeleton");
	while(query.Step()) {
		SkeletonInfo info;
		info.id = query.ColInt64(0);
		info.name = query.ColText(1);
		out.push_back(info);
	}
}

void Entity::GetMotionGraphInfos(std::vector<MotionGraphInfo>& out, sqlite3_int64 skel_id) const
{
	out.clear();
	Query query(m_db, "SELECT id,name FROM motion_graphs WHERE skel_id = ?");
	query.BindInt64(1, skel_id);
	while(query.Step()) {
		MotionGraphInfo info;
		info.id = query.ColInt64(0);
		info.name = query.ColText(1);
		out.push_back(info);
	}
}

void Entity::GetMeshInfos(std::vector<MeshInfo>& out, sqlite3_int64 skel_id) const
{
	out.clear();
	Query query(m_db, "SELECT id,name FROM meshes WHERE skel_id = ?");
	query.BindInt64(1, skel_id);
	while(query.Step()) {
		MeshInfo info;
		info.id = query.ColInt64(0);
		info.name = query.ColText(1);
		out.push_back(info);
	}
}

void Entity::DeleteSkeleton(sqlite3_int64 skel_id)
{
	sql_begin_transaction(m_db);
	// delete everything using a skeleton (mesh, motion_graphs)
	Query del_meshes(m_db, "DELETE FROM meshes WHERE skel_id = ?");
	del_meshes.BindInt64(1, skel_id);
	del_meshes.Step();

	Query del_mg(m_db, "DELETE FROM motion_graphs WHERE skel_id = ?");
	del_mg.BindInt64(1, skel_id);
	del_mg.Step();
	
	Query del_clips(m_db, "DELETE FROM clips WHERE skel_id = ?");
	del_clips.BindInt64(1, skel_id);
	del_clips.Step();

	Query del_skel(m_db, "DELETE FROM skeleton WHERE id = ?");
	del_skel.BindInt64(1, skel_id);
	del_skel.Step();

	sql_end_transaction(m_db);
}

void Entity::DeleteMesh(sqlite3_int64 mesh_id)
{
	sql_begin_transaction(m_db);
	Query del_meshes(m_db, "DELETE FROM meshes WHERE id = ?");
	del_meshes.BindInt64(1, mesh_id);
	del_meshes.Step();
	sql_end_transaction(m_db);
}

void Entity::DeleteMotionGraph(sqlite3_int64 mg_id)
{
	Transaction t(m_db);
	std::vector<sqlite3_int64> transitions ;

	// need to delete these after the graph is deleted due to fk constraints
	Query get_transitions(m_db, 
						  "SELECT id FROM clips WHERE is_transition = 1 AND "
						  "id IN (SELECT clip_id FROM motion_graph_edges WHERE motion_graph_id = ?)");
	get_transitions.BindInt64(1, mg_id);
	while(get_transitions.Step()) {
		transitions.push_back(get_transitions.ColInt64(0));
	}

	Query del(m_db, "DELETE FROM motion_graphs WHERE id = ?");
	del.BindInt64(1, mg_id);
	del.Step();
	
	Query del_transition(m_db, "DELETE FROM clips WHERE id = ? AND is_transition = 1");
	for(int i = 0; i < (int)transitions.size(); ++i) {
		del_transition.Reset();
		del_transition.BindInt64(1, transitions[i] );
		del_transition.Step();
	}

	Events::MassClipRemoveEvent ev;
	m_evsys->Send(&ev);
}

void Entity::CreateMissingTables()
{
	ASSERT(m_db);

	// TODO unique things
	static const char* beginTransaction = 
		"BEGIN EXCLUSIVE TRANSACTION";
	static const char* endTransaction = 
		"END TRANSACTION";

	static const char* createSkelStmt = 
		"CREATE TABLE IF NOT EXISTS skeleton ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"name TEXT,"
		"root_offset_x REAL,"
		"root_offset_y REAL,"
		"root_offset_z REAL,"
		"root_rotation_a REAL,"
		"root_rotation_b REAL,"
		"root_rotation_c REAL,"
		"root_rotation_r REAL)";

	static const char* indexSkel = 
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_skeleton ON skeleton (id)";
	   
	static const char* createJointsStmt = 
		"CREATE TABLE IF NOT EXISTS skeleton_joints ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"skel_id INTEGER NOT NULL,"
		"parent_id INTEGER NOT NULL,"
		"offset INTEGER NOT NULL," // index in skeleton
		"name TEXT,"
		"t_x REAL,"		
		"t_y REAL,"
		"t_z REAL,"
		"weight REAL,"
		"UNIQUE(skel_id,offset),"
		"CONSTRAINT joints_parent_u FOREIGN KEY(skel_id) REFERENCES skeleton(id) ON UPDATE CASCADE,"
		"CONSTRAINT joints_parent_d FOREIGN KEY(skel_id) REFERENCES skeleton(id) ON DELETE CASCADE)";

	static const char *indexJoint1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_skeleton_joints ON skeleton_joints (id)";

	static const char *indexJoint2 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_skeleton_joints2 ON skeleton_joints (skel_id,offset)";	

	static const char* createClipsStmt =
		"CREATE TABLE IF NOT EXISTS clips ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"skel_id INTEGER NOT NULL,"
		"name TEXT,"
		"fps REAL,"
		"is_transition INTEGER DEFAULT 0,"
		"CONSTRAINT clips_parent_u FOREIGN KEY(skel_id) REFERENCES skeleton(id) ON UPDATE RESTRICT,"
		"CONSTRAINT clips_parent_d FOREIGN KEY(skel_id) REFERENCES skeleton(id) ON DELETE RESTRICT)";

	static const char *indexClips =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_clips ON clips (id)";	

	static const char* createFramesStmt = 
		"CREATE TABLE IF NOT EXISTS frames ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"clip_id INTEGER NOT NULL,"
		"num INTEGER NOT NULL," // 'index' of frame in an array
		"root_offset_x REAL,"
		"root_offset_y REAL,"
		"root_offset_z REAL,"
		"root_rotation_a REAL,"
		"root_rotation_b REAL,"
		"root_rotation_c REAL,"
		"root_rotation_r REAL,"
		"UNIQUE(clip_id, num),"
		"CONSTRAINT frames_parent_u FOREIGN KEY(clip_id) REFERENCES clips(id) ON UPDATE CASCADE,"
		"CONSTRAINT frames_parent_d FOREIGN KEY(clip_id) REFERENCES clips(id) ON DELETE CASCADE)";
	
	static const char *indexFrames1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_frames ON frames (id)";	

	static const char *indexFrames2 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_frames2 ON frames (clip_id,num)";	

	static const char *indexFrames3 =
		"CREATE INDEX IF NOT EXISTS idx_frames3 ON frames (clip_id)";	

	static const char* createFrameRotationsStmt =
		"CREATE TABLE IF NOT EXISTS frame_rotations ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"frame_id INTEGER NOT NULL,"
		"skel_id INTEGER NOT NULL,"
		"joint_offset INTEGER NOT NULL," 
		"q_a REAL,"
		"q_b REAL,"
		"q_c REAL,"
		"q_r REAL,"
		"UNIQUE(frame_id,joint_offset),"
		"CONSTRAINT rots_parent_u FOREIGN KEY (skel_id, joint_offset) REFERENCES skeleton_joints(skel_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT rots_parent_d FOREIGN KEY (skel_id, joint_offset) REFERENCES skeleton_joints(skel_id, offset) ON DELETE RESTRICT,"
		"CONSTRAINT rots_parent_u2 FOREIGN KEY (frame_id) REFERENCES frames(id) ON UPDATE CASCADE,"
		"CONSTRAINT rots_parent_d2 FOREIGN KEY (frame_id) REFERENCES frames(id) ON DELETE CASCADE)";

	static const char *indexFrameRots1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_frame_rots ON frame_rotations (id)";	

	static const char *indexFrameRots2 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_frame_rots2 ON frame_rotations (frame_id,joint_offset)";	

	static const char* createAnnotationsStmt = 
		"CREATE TABLE IF NOT EXISTS annotations ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"name TEXT,"
		"fidelity REAL,"
		"UNIQUE(name))";

	static const char *indexAnno =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_annotations ON annotations (id)";	

	static const char* createClipAnnotationsStmt = 
		"CREATE TABLE IF NOT EXISTS clip_annotations ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"annotation_id INTEGER NOT NULL,"
		"clip_id INTEGER NOT NULL,"
		"UNIQUE(annotation_id,clip_id),"
		"CONSTRAINT anno_parent_d FOREIGN KEY (annotation_id) REFERENCES annotations(id) ON DELETE CASCADE,"
		"CONSTRAINT anno_clip_parent_d FOREIGN KEY (clip_id) REFERENCES clips(id) ON DELETE CASCADE)";

	static const char *indexClipAnno1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_clip_annotations ON clip_annotations (id)";	

	static const char *indexClipAnno2 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_clip_annotations2 ON clip_annotations (annotation_id,clip_id)";	

	static const char* createMeshStmt =
		"CREATE TABLE IF NOT EXISTS meshes ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"skel_id INTEGER NOT NULL,"
		"name TEXT,"
		"transform BLOB,"
		"CONSTRAINT mesh_owner_u FOREIGN KEY (skel_id) REFERENCES skeleton(id) ON UPDATE CASCADE,"
		"CONSTRAINT mesh_owner_d FOREIGN KEY (skel_id) REFERENCES skeleton(id) ON DELETE RESTRICT)";

	static const char *indexMeshes =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_meshes ON meshes (id)";	

	static const char *createVerticesStmt =
		"CREATE TABLE IF NOT EXISTS mesh_verts ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"mesh_id INTEGER NOT NULL,"
		"offset INTEGER NOT NULL,"
		"x REAL, y REAL, z REAL,"
		"UNIQUE(mesh_id,offset),"
		"CONSTRAINT vert_parent_u FOREIGN KEY (mesh_id) REFERENCES mesh_verts(id) ON UPDATE CASCADE,"
		"CONSTRAINT vert_parent_d FOREIGN KEY (mesh_id) REFERENCES mesh_verts(id) ON DELETE CASCADE)";
	
	static const char *indexVerts1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_verts ON mesh_verts (id)";	

	static const char *indexVerts2 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_verts2 ON mesh_verts (mesh_id,offset)";	

	static const char* createQuadIndicesStmt = 
		"CREATE TABLE IF NOT EXISTS mesh_quads ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"mesh_id INTEGER NOT NULL,"
		"idx0 INTEGER NOT NULL, idx1 INTEGER NOT NULL, idx2 INTEGER NOT NULL, idx3 INTEGER NOT NULL,"
		"CONSTRAINT quad_idx_owner_1 FOREIGN KEY (mesh_id) REFERENCES meshes(id) ON DELETE CASCADE,"
		"CONSTRAINT quad_idx_owner_2 FOREIGN KEY (mesh_id) REFERENCES meshes(id) ON UPDATE CASCADE,"
		"CONSTRAINT quad_idx_owner_3 FOREIGN KEY (mesh_id, idx0) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT quad_idx_owner_4 FOREIGN KEY (mesh_id, idx1) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT quad_idx_owner_5 FOREIGN KEY (mesh_id, idx2) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT quad_idx_owner_6 FOREIGN KEY (mesh_id, idx3) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT quad_idx_owner_7 FOREIGN KEY (mesh_id, idx0) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE,"
		"CONSTRAINT quad_idx_owner_8 FOREIGN KEY (mesh_id, idx1) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE,"
		"CONSTRAINT quad_idx_owner_9 FOREIGN KEY (mesh_id, idx2) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE,"
		"CONSTRAINT quad_idx_owner_10 FOREIGN KEY (mesh_id, idx3) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE)";

	static const char *indexQuads =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_quads ON mesh_quads (id)";	

	static const char* createTriIndicesStmt = 
		"CREATE TABLE IF NOT EXISTS mesh_tris ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"mesh_id INTEGER NOT NULL,"
		"idx0 INTEGER NOT NULL, idx1 INTEGER NOT NULL, idx2 INTEGER NOT NULL,"
		"CONSTRAINT tri_idx_owner_1 FOREIGN KEY (mesh_id) REFERENCES meshes(id) ON DELETE CASCADE,"
		"CONSTRAINT tri_idx_owner_2 FOREIGN KEY (mesh_id) REFERENCES meshes(id) ON UPDATE CASCADE,"
		"CONSTRAINT tri_idx_owner_3 FOREIGN KEY (mesh_id,idx0) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT tri_idx_owner_4 FOREIGN KEY (mesh_id,idx1) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT tri_idx_owner_5 FOREIGN KEY (mesh_id,idx2) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT tri_idx_owner_6 FOREIGN KEY (mesh_id,idx0) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE,"
		"CONSTRAINT tri_idx_owner_7 FOREIGN KEY (mesh_id,idx1) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE,"
		"CONSTRAINT tri_idx_owner_8 FOREIGN KEY (mesh_id,idx2) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE)";
		
	static const char *indexTris =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_tris ON mesh_tris (id)";	

	static const char *createNormalsStmt = 
		"CREATE TABLE IF NOT EXISTS mesh_normals ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"mesh_id INTEGER NOT NULL,"
		"offset INTEGER NOT NULL,"
		"nx REAL, ny REAL, nz REAL,"
		"UNIQUE (mesh_id, offset),"
		"CONSTRAINT normal_owner_1 FOREIGN KEY (mesh_id, offset) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT normal_owner_2 FOREIGN KEY (mesh_id, offset) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE)";
	
	static const char *indexNormals1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_normals ON mesh_normals (id)";	

	static const char *indexNormals2 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_normals2 ON mesh_normals (mesh_id,offset)";	

	static const char* createTexcoordsStmt = 
		"CREATE TABLE IF NOT EXISTS mesh_texcoords ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"mesh_id INTEGER NOT NULL,"
		"offset INTEGER NOT NULL,"
		"u REAL, v REAL,"
		"UNIQUE(mesh_id, offset),"
		"CONSTRAINT texc_owner_1 FOREIGN KEY (mesh_id, offset) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT texc_owner_2 FOREIGN KEY (mesh_id, offset) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE)";

	static const char *indexTexcoords1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_texcoords ON mesh_texcoords (id)";	

	static const char *indexTexcoorsd2 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_texcoords2 ON mesh_texcoords (mesh_id,offset)";	

	static const char* createSkinMatsStmt = 
		"CREATE TABLE IF NOT EXISTS mesh_skin ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"mesh_id INTEGER NOT NULL,"
		"offset INTEGER NOT NULL,"
		"skel_id INTEGER NOT NULL,"
		"joint_offset INTEGER NOT NULL,"
		"weight REAL,"
		"CONSTRAINT skin_owner_1 FOREIGN KEY (skel_id, joint_offset) REFERENCES skeleton_joints(skel_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT skin_owner_2 FOREIGN KEY (skel_id, joint_offset) REFERENCES skeleton_joints(skel_id, offset) ON DELETE CASCADE,"
		"CONSTRAINT skin_owner_3 FOREIGN KEY (mesh_id, offset) REFERENCES mesh_verts(mesh_id, offset) ON UPDATE CASCADE,"
		"CONSTRAINT skin_owner_4 FOREIGN KEY (mesh_id, offset) REFERENCES mesh_verts(mesh_id, offset) ON DELETE CASCADE)";

	static const char *indexSkinMats1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mesh_skinmats ON mesh_skin (id)";	

	static const char *indexSkinMats2 =
		"CREATE INDEX IF NOT EXISTS idx_mesh_skinmats2 ON mesh_skin (mesh_id,offset)";	

	static const char* createMotionGraphContainerStmt = 
		"CREATE TABLE IF NOT EXISTS motion_graphs ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"skel_id INTEGER NOT NULL,"
		"name TEXT,"
		"CONSTRAINT mg_owner_1 FOREIGN KEY (skel_id) REFERENCES skeleton(id) ON UPDATE CASCADE,"
		"CONSTRAINT mg_owner_2 FOREIGN KEY (skel_id) REFERENCES skeleton(id) ON DELETE RESTRICT)";

	static const char *indexMg =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mg ON motion_graphs (id)";		

	static const char* createMotionGraphStmt =
		"CREATE TABLE IF NOT EXISTS motion_graph_edges ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"motion_graph_id INTEGER NOT NULL,"
		"clip_id INTEGER NOT NULL,"
		"start_id INTEGER NOT NULL,"
		"finish_id INTEGER NOT NULL,"
//		"num_frames INTEGER NOT NULL," // don't think I need this - can get it from clip_id
		"align_t_x REAL DEFAULT 0.0,"
		"align_t_y REAL DEFAULT 0.0,"
		"align_t_z REAL DEFAULT 0.0,"
		"align_q_a REAL DEFAULT 0.0,"
		"align_q_b REAL DEFAULT 0.0,"
		"align_q_c REAL DEFAULT 0.0,"
		"align_q_r REAL DEFAULT 1.0,"
		"CONSTRAINT edge_owner_1 FOREIGN KEY (motion_graph_id) REFERENCES motion_graphs(id) ON DELETE CASCADE,"
		"CONSTRAINT edge_owner_2 FOREIGN KEY (motion_graph_id) REFERENCES motion_graphs(id) ON UPDATE CASCADE,"
		"CONSTRAINT edge_owner_3 FOREIGN KEY (clip_id) REFERENCES clips(id) ON DELETE RESTRICT,"
		"CONSTRAINT edge_owner_4 FOREIGN KEY (clip_id) REFERENCES clips(id) ON UPDATE CASCADE,"
		"CONSTRAINT edge_owner_5 FOREIGN KEY (start_id) REFERENCES motion_graph_nodes(id) ON DELETE CASCADE,"
		"CONSTRAINT edge_owner_6 FOREIGN KEY (finish_id) REFERENCES motion_graph_nodes(id) ON DELETE CASCADE)";

	static const char *indexMgEdges1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mg_edges ON motion_graph_edges (id)";		

//	static const char *indexMgEdges2 =
//		"CREATE INDEX IF NOT EXISTS idx_mg_edges2 ON motion_graph_edges (start_id)";		

	static const char* createMotionGraphNodesStmt =
		"CREATE TABLE IF NOT EXISTS motion_graph_nodes ("
		"id INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
		"motion_graph_id INTEGER NOT NULL,"
		"clip_id INTEGER NOT NULL,"
		"frame_num INTEGER NOT NULL,"
		"CONSTRAINT node_clip_d FOREIGN KEY (clip_id,frame_num) REFERENCES frames(clip_id,num) ON DELETE RESTRICT,"
		"CONSTRAINT node_clip_u FOREIGN KEY (clip_id,frame_num) REFERENCES frames(clip_id,num) ON UPDATE CASCADE,"
		"CONSTRAINT node_owner_1 FOREIGN KEY (motion_graph_id) REFERENCES motion_graphs(id) ON UPDATE CASCADE,"
		"CONSTRAINT node_owner_2 FOREIGN KEY (motion_graph_id) REFERENCES motion_graphs(id) ON DELETE CASCADE)" ;

	static const char *indexMgNodes1 =
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_mg_nodes ON motion_graph_nodes (id)";		


	const char* toCreate[] = 
	{
		beginTransaction,
		createSkelStmt,
		indexSkel,
		createJointsStmt,
		indexJoint1,
		indexJoint2,
		createClipsStmt,
		indexClips,
		createFramesStmt,
		indexFrames1,
		indexFrames2,
		indexFrames3,
		createFrameRotationsStmt,
		indexFrameRots1,
		indexFrameRots2,
		createAnnotationsStmt,
		indexAnno,
		createClipAnnotationsStmt,
		indexClipAnno1,
		indexClipAnno2,
		createMeshStmt,
		indexMeshes,
		createVerticesStmt,
		indexVerts1,
		indexVerts2,
		createQuadIndicesStmt,
		indexQuads,
		createTriIndicesStmt,
		indexTris,
		createNormalsStmt,
		indexNormals1,
		indexNormals2,
		createTexcoordsStmt,
		indexTexcoords1,
		indexTexcoorsd2,
		createSkinMatsStmt, 
		indexSkinMats1,
		indexSkinMats2,
		createMotionGraphContainerStmt,
		indexMg,
		createMotionGraphStmt,
		indexMgEdges1,
		createMotionGraphNodesStmt,
		indexMgNodes1,
		endTransaction,
	};
		
	const int count = sizeof(toCreate)/sizeof(const char*);
	for(int i = 0; i < count; ++i) {
		Query create(m_db, toCreate[i]);
		create.Step();
	}
}

////////////////////////////////////////////////////////////////////////////////
// serialization funcs
////////////////////////////////////////////////////////////////////////////////

bool exportEntityLBF(const Entity* entity, const char* filename)
{
	LBF::WriteNode* objSectionNode = new LBF::WriteNode(LBF::OBJECT_SECTION, 0, 0);
	LBF::WriteNode* animSectionNode = new LBF::WriteNode(LBF::ANIM_SECTION, 0, 0);
	
	objSectionNode->AddSibling(animSectionNode);
	if(entity->GetMesh())
	{
		LBF::WriteNode* geom3d = entity->GetMesh()->CreateMeshWriteNode();
		if(geom3d) {
			objSectionNode->AddChild(geom3d);
		}
	}

	if(entity->GetSkeleton())
	{
		LBF::WriteNode* skelNode = entity->GetSkeleton()->CreateSkeletonWriteNode();
		if(skelNode) {
			LBF::WriteNode* skelWeightsNode = entity->GetSkeleton()->GetSkeletonWeights().CreateWriteNode();
			if(skelWeightsNode) {
				skelNode->AddChild(skelWeightsNode);
			}
			animSectionNode->AddChild(skelNode);
		}
	}

	if(entity->GetClips())
	{
		LBF::WriteNode* clipsNode = createClipsWriteNode( entity->GetClips() );
		if(clipsNode) {
			animSectionNode->AddChild(clipsNode);
		}
	}

	int err = LBF::saveLBF( filename, objSectionNode, true );
	delete objSectionNode;
	if(err == 0) {
		return true;
	} else {
		fprintf(stderr, "Failed with error code %d\n", err);
		// error report?
		return false;
	}
}

bool importEntityLBF(Entity *target, const char* filename)
{
	if(!target->HasDB()) return false;

	LBF::LBFData* file = 0;
	int err = LBF::openLBF( filename, file );
	if(err != 0) {
		fprintf(stderr, "Failed with error code %d\n", err);
		return false;
	}

	LBF::ReadNode rnObj = file->GetFirstNode(LBF::OBJECT_SECTION);
	LBF::ReadNode rnAnim = file->GetFirstNode(LBF::ANIM_SECTION);

	if(rnAnim.Valid()) {
		LBF::ReadNode rnSkel = rnAnim.GetFirstChild(LBF::SKELETON);
		LBF::ReadNode rnSkelWeights = rnSkel.GetFirstChild(LBF::SKELETON_JOINT_WEIGHTS);		
		LBF::ReadNode rnFirstClip = rnAnim.GetFirstChild(LBF::ANIMATION);
		
		sqlite3_int64 skelid = Skeleton::ImportFromReadNode(target->GetDB(), rnSkel);
		if(skelid > 0)
		{
			SkeletonWeights weights(target->GetDB(), skelid);
			weights.ImportFromReadNode(rnSkelWeights);

			importClipsFromReadNode(target->GetDB(), skelid, rnFirstClip);	

			target->SetCurrentSkeleton(skelid);
		}
	}

	if(rnObj.Valid() ) {
		LBF::ReadNode rnGeom = rnObj.GetFirstChild(LBF::GEOM3D);
		if(rnGeom.Valid()) {
			sqlite3_int64 mesh_id =  Mesh::ImportFromReadNode( target->GetDB(), target->GetCurrentSkeleton(), rnGeom );
			if(mesh_id > 0) 
				target->SetCurrentMesh(mesh_id);
		}
	}
	delete file;
	return true;
}

