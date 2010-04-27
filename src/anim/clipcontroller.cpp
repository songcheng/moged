#include <cmath>
#include <cstdio>
#include "clipcontroller.hh"
#include "pose.hh"
#include "clip.hh"
#include "skeleton.hh"
#include "MathUtil.hh"

ClipController::ClipController()
	: m_skel(0)
	, m_clip(0)
	, m_frame(0.f)
{
}

void ClipController::ComputePose( Pose* out ) 
{
	if(m_skel)
	{   // TODO: this needs another step - pose should be local (so we can do procedural stuff)
		// and then do a localToModel and ModelToWorld transformation at the end
		if(m_clip) 
		{
			float frame_low = floor(m_frame);
			float frame_hi = ceil(m_frame);
			float fraction = m_frame - frame_low;
			float one_minus_fraction = 1.0 - fraction;

			int iframe_low = frame_low;
			int iframe_hi = frame_hi;

			const Quaternion *rotations_low = m_clip->GetFrameRotations(iframe_low);
			const Quaternion *rotations_hi = m_clip->GetFrameRotations(iframe_hi);
			const Quaternion *skel_rest_rotations = m_skel->GetJointRotations();
			const Vec3 *skel_rest_offsets = m_skel->GetJointTranslations();
			const int* parents = m_skel->GetParents();

			Vec3 root_pos = one_minus_fraction * m_clip->GetFrameRootOffset(iframe_low) 
				+ fraction * m_clip->GetFrameRootOffset(iframe_hi);
			Quaternion root_rot;
			slerp( root_rot, m_clip->GetFrameRootOrientation(iframe_low),
				   m_clip->GetFrameRootOrientation(iframe_hi), fraction);

			Quaternion* out_rotations = out->GetRotations();
			Vec3* out_offsets = out->GetOffsets();

			int num_joints = m_skel->GetNumJoints();
			Quaternion anim_rot ;
			Quaternion local_rot ;
			for(int i = 0; i < num_joints; ++i) {
				slerp(anim_rot, rotations_low[i], rotations_hi[i], fraction);
				local_rot = skel_rest_rotations[i] * anim_rot * conjugate(skel_rest_rotations[i]) /* * bindPoseToRestRotation */;
				
				int parent = parents[i];
				if(parent == -1) {
					out_rotations[i] = root_rot * local_rot;
					out_offsets[i] = root_pos + rotate(skel_rest_offsets[i], out_rotations[i]);//root_rot );
				} else {
					out_rotations[i] = out_rotations[parent] * local_rot;
					out_offsets[i] = out_offsets[parent] + rotate(skel_rest_offsets[i], out_rotations[i]);//out_rotations[parent] * local_rot);
				}
			}			
			// std::printf("lo %f hi %f frac %f\n", frame_low, frame_hi, fraction);
			
//			printf("%f %f %f\n", root_pos.x, root_pos.y, root_pos.z);
			out->SetRootOffset(root_pos);
			out->SetRootRotation(root_rot);
		}
		else
			out->RestPose(m_skel);
	}
}

void ClipController::SetSkeleton( const Skeleton* skel ) 
{
	if(m_skel != skel) {
		m_skel = skel;
		SetClip(0);
	}
}

void ClipController::SetClip( const Clip* clip ) 
{
	if(m_clip != clip) {
		m_clip = clip;
		m_frame = 0.f;
	}
}

void ClipController::UpdateTime( float dt )
{
	if(m_clip) {
		float newTime = m_frame + dt * m_clip->GetClipFPS();
		ClampSetFrame(newTime);
	} 
}

void ClipController::SetFrame( float frame )
{
	ClampSetFrame(frame);
}

bool ClipController::IsAtEnd() const 
{
	if(m_clip)
		return m_frame >= (float)(m_clip->GetNumFrames() - 1);
	else
		return true;
}

void ClipController::ClampSetFrame(float t)
{
	if(m_clip) {
		m_frame = Clamp(t, 0.f, float(m_clip->GetNumFrames() - 1) );
	} else 
		m_frame = 0.f;	
}

void ClipController::SetToLastFrame() 
{
	if(m_clip) {
		m_frame = float(m_clip->GetNumFrames() - 1);
	} else 
		m_frame = 0.f;
}
