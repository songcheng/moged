#ifndef INCLUDED_lbf_loader_HH
#define INCLUDED_lbf_loader_HH

////////////////////////////////////////////////////////////////////////////////
// "Lab Binary File" or "Luke's Binary File"
////////////////////////////////////////////////////////////////////////////////

#define LBF_VERSION_MAJOR 1
#define LBF_VERSION_MINOR 0

namespace LBF
{
	// TODO: memory allocation hooks 

	// TODO: move these to common place
	typedef unsigned char u8;
	typedef unsigned short u16;
	typedef unsigned int u32;

	struct FileHeader {
		u8 tag[4]; // "LBF_"
		u16 version_major;
		u16 version_minor;
		u8 reserved[8];
	};

	struct ChunkHeader{ 
		u32 type;
		u32 length; 
		u32 child_offset;
		u8 reserved[4];
	};

	enum TypeID {
		DONTCARE = -1, // used for finding functions

		////////////////////////////////////////////////////////////////////////////////
		// geometry container
		GEOM3D = 0x1000, 

		GEOM3D_NAME = 0x1001, 
		
		VTXFMT = 0x1050,
 
		// index buffer types
		TRIMESH_INDICES = 0x1100,
		QUADMESH_INDICES = 0x1101,

		// various geometry data
		POSITIONS = 0x1200, 
		NORMALS = 0x1201,
		WEIGHTS = 0x1202,
		SKINMATS = 0x1203,

		// bind pose matrices (in joint local space)
		BIND_ROTATIONS = 0x1204,

		////////////////////////////////////////////////////////////////////////////////
		// animation container
		ANIMATION = 0x2000,
		ANIMATION_INFO = 0x2001,

		// per frame animation data
		FRAME = 0x2002,
		FRAME_ROTATIONS = 0x2003, // joint local space

		////////////////////////////////////////////////////////////////////////////////
		// skeleton container
		SKELETON = 0x3000,
		SKELETON_INFO = 0x3001,
		SKELETON_TRANSLATIONS = 0x3002,
		SKELETON_ROTATIONS = 0x3003,
		SKELETON_NAMES = 0x3004,		
	};

	////////////////////////////////////////////////////////////////////////////////
	// Structure for getting results from LBFData find functions. These are tied to
	// a particular LBFData file.
	class ReadNode {
		char* m_data;
		long m_size;
	public:
		ReadNode() ;
		ReadNode(char *chunk_start, long size);

		bool Valid() const { return m_data != 0; }
		int GetType() const ;

		const char* GetNodeData() const ;
		int GetNodeDataLength() const ;

		ReadNode GetNext(int type = DONTCARE) const;
		ReadNode GetFirstChild(int type = DONTCARE) const ;
	};

	class WriteNode {
		int m_type;
		char *m_data;
		long m_data_length;
		WriteNode* m_first_child;
		WriteNode* m_next;
	public:
		explicit WriteNode(int type, int length);
		~WriteNode() ;

		int GetType() const { return m_type; }
		void AddChild(WriteNode* node) ; 
		void AddSibling(WriteNode* node) ;

		const WriteNode* GetNext( ) const ;
		const WriteNode* GetFirstChild( ) const ;

		char* GetData() ;
		const char* GetData() const;
		long GetDataLength() const ;

	}; 

	class LBFData {
		char *m_file_data;
		long m_file_size;
		bool m_owner;
	public:		
		explicit LBFData(char* top_ptr, long file_size, bool owner = false);
		~LBFData();

		ReadNode GetFirstNode(int type = DONTCARE);		
	};

	////////////////////////////////////////////////////////////////////////////////
	// Interpret 'buffer' of buffer_size as an lbf file. 
	// if takeOwnership is specified, the resulting LBFData object owns the buffer that was pssed in, and will be deleted with delete[].
	// if the call fails and takeOwnership == true, the buffer will also be delete[]'d.
	int parseLBF( char* buffer, long buffer_size, LBFData*& result, bool takeOwnership = false );
	// same as parse, but opens file, allocates data and passes ownership to the result.
	int openLBF( const char* filename, LBFData*& result);

	////////////////////////////////////////////////////////////////////////////////
	// using the existing LBF file (currently loaded), or null, with hierarchy
	// specified by the Node*. Returns a NEW character buffer for writing
	int mergeLBF( const WriteNode* newData, const LBFData* existing, char *& outBuffer, long& outLen );
	// save newData as file, optionally merging 
	int saveLBF( const char* filename, WriteNode* newData, bool doMerge );

	enum ReturnCodes {
		OK = 0,
		ERR_OPEN_R,
		ERR_OPEN_W,
		ERR_MEM,
		ERR_READ,
		ERR_WRITE,
		ERR_PARSE,
		ERR_MERGE,
		ERR_BAD_VERSION,
		ERR_SIZE,
	};
}

#endif
