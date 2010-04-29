#!BPY
"""
Name: 'Luke/Lab Binary File Format (.lbf)...'
Blender: 249
Group: 'Export'
Tooltip: 'Export LBF file format (*.lbf)'
"""

__author__ = "Lucas Panian"
__url__ = ("Author's Homepage, http://panian.net")
__version__ = "1.0"

import Blender
from Blender import Armature,Modifier
from Blender.Mathutils import *
import Blender.Draw
import lbf
import lbf.structures
import os
import os.path

def to_lbf_quat( quat ):
    return (quat[1],quat[2],quat[3],quat[0])

def _export_skeleton(ob, node):
    data = ob.getData()
    obMat = ob.getMatrix()
    
    skel = lbf.structures.Skeleton()
    skel.name = data.name
    skel.root_offset = obMat.translationPart()
    skel.root_rotation = to_lbf_quat(obMat.rotationPart().toQuat())
    
    working_bones = []
    for bone in data.bones.values():
        if bone.parent == None:
            working_bones.append((bone,-1))

    working_bones = sorted(working_bones, key=lambda bone: bone[0].name, reverse=True)
    parents = []
    while len(working_bones) > 0:
        cur,p_index = working_bones.pop()
        my_index = len(parents)
        parents.append(p_index)        

        for child in sorted(cur.children, key=lambda bone: bone.name, reverse=True):
            working_bones.append((child,my_index))
        offset = cur.tail['ARMATURESPACE'] - cur.head['ARMATURESPACE']
        skel.offsets.append(list(offset))
        skel.names.append( cur.name )

    skel.parents = parents
    skel.num_joints = len(parents)
    node.add_child( skel.toNode() )

def _export_face(face, mesh, tgtBuffer):
    for i,v in enumerate(face):
        for fmt in mesh.vertex_format:
            if fmt == 'POSITIONS':
                tgtBuffer.append(v.index) # index of position == vert index
            elif fmt == 'NORMALS':
                tgtBuffer.append(v.index) # same with normals
            elif fmt == 'TEXCOORDS':
                idx = len(mesh.texcoords)
                mesh.texcoords.append( face.uv[i][0:2] )
                tgtBuffer.append(idx)
            elif fmt == 'WEIGHTS':
                tgtBuffer.append(v.index) # etc... 
            elif fmt == 'SKINMATS':
                tgtBuffer.append(v.index)
            else:
                raise lbf.LBFError("Don't know how to export %s in mesh" % (fmt))            

# return map of bones and their indices
def _get_skeleton_bone_export_indices(skelData):
    # TODO: check if mesh has Armature modifier
    working = []
    for bone in skelData.bones.values():
        if bone.parent==None:
            working.append(bone)
    
    idx = 0
    result = {}
    working = sorted(working, key=lambda bone: bone.name, reverse=True)
    while len(working)>0:
        cur = working.pop()
        for child in sorted(cur.children, key=lambda bone: bone.name, reverse=True):
            working.append(child)
        result[cur.name] = idx
        idx += 1        
    return result

def _compute_vertex_weights(influences, bone_indices):
    # biggest weight to smallest weight.
    influences = sorted(influences, key=lambda inf: inf[1], reverse=True)
    influences = influences[0:4] # only 4 supported    
    totalW = reduce( lambda lhs,rhs: lhs + rhs, map(lambda x:x[1],influences), 0.0)
    influences = map (lambda inf: (bone_indices[inf[0]],inf[1]/totalW), influences)
    indices,weights = ([],[])
    if len(influences) > 0:
        indices,weights = map(list,zip(*influences))
    
    if len(indices) < 4:
        indices.extend( (4-len(indices)) * [0] )
    if len(weights) < 4:
        weights.extend( (4-len(weights)) * [0.0] )

    return (weights,indices)

def _export_mesh(ob, node, meshNum):
    data = ob.getData(False, True)
    mesh = lbf.structures.Mesh()
    mesh.name = data.name
    mesh.vertex_format = ['POSITIONS', 'NORMALS']    

    # basic vert stuff
    for vert in data.verts:
        mesh.positions.append( vert.co )
        mesh.normals.append( vert.no )

    # weighting stuff
    if data.getVertGroupNames() > 0:
        # find skeleton we're attached to
        skelData = None
        for mod in ob.modifiers:
            if mod.type == Blender.Modifier.Types.ARMATURE:
                if mod[Modifier.Settings.VGROUPS]:
                    skelObj = mod[Modifier.Settings.OBJECT]
                    if skelObj:
                        skelData = skelObj.getData(False,True)
                        break
        if skelData:
            bone_indices = _get_skeleton_bone_export_indices(skelData)
            mesh.vertex_format.extend(['WEIGHTS','SKINMATS'])
            for vert in data.verts:
                influences = data.getVertexInfluences(vert.index)
                weights,indices = _compute_vertex_weights(influences, bone_indices)
                mesh.weights.append(weights)
                mesh.skinmats.append(indices)
    
    # optional tex coords
    if len(data.getUVLayerNames()) > 0:
        mesh.vertex_format.append('TEXCOORDS')
        data.activeUVLayer = data.getUVLayerNames()[0]
    
    # face stuff
    for face in data.faces:
        if len(face) == 3:
            _export_face(face, mesh, mesh.trimesh_indices)
        elif len(face) == 4:
            _export_face(face, mesh, mesh.quadmesh_indices)

    meshNode = mesh.toNode()
    meshNode.id = meshNum
    node.add_child( meshNode ) 

def export_lbf(fname):
    if not fname.lower().endswith('.lbf'):
        fname += '.lbf'

    # are you SUUUURE?? 
    if os.path.exists(fname):
        result = Blender.Draw.PupMenu("Save Over?%t|Yes|No")
        if result != 1:
            return

    Blender.Window.WaitCursor(1)
    sce = Blender.Scene.GetCurrent()

    outlbf = lbf.LBFFile()
    objSection = outlbf.add_node( lbf.LBFNode('OBJECT_SECTION') )
    animSection = outlbf.add_node( lbf.LBFNode('ANIM_SECTION') )

    meshNum = 0
    for ob in sce.objects:
        if ob.type == 'Armature':
            _export_skeleton(ob, animSection)
        elif ob.type == 'Mesh':
            _export_mesh(ob, objSection, meshNum)
            meshNum += 1

    lbf.writeLBF(outlbf, fname)

    Blender.Window.WaitCursor(0)
if __name__ == '__main__':
    Blender.Window.FileSelector(export_lbf, 'Export LBF', '*.lbf')
