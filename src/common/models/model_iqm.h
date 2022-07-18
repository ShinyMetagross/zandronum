#pragma once

#include <stdint.h>
#include "model.h"
#include "vectors.h"
#include "common/rendering/i_modelvertexbuffer.h"

struct IQMMesh
{
	FString Name;
	FString Material;
	uint32_t FirstVertex;
	uint32_t NumVertices;
	uint32_t FirstTriangle;
	uint32_t NumTriangles;
	FTextureID Skin;
};

enum IQMVertexArrayType
{
	IQM_POSITION = 0,     // float, 3
	IQM_TEXCOORD = 1,     // float, 2
	IQM_NORMAL = 2,       // float, 3
	IQM_TANGENT = 3,      // float, 4
	IQM_BLENDINDEXES = 4, // ubyte, 4
	IQM_BLENDWEIGHTS = 5, // ubyte, 4
	IQM_COLOR = 6,        // ubyte, 4
	IQM_CUSTOM = 0x10
};

enum IQMVertexArrayFormat
{
	IQM_BYTE = 0,
	IQM_UBYTE = 1,
	IQM_SHORT = 2,
	IQM_USHORT = 3,
	IQM_INT = 4,
	IQM_UINT = 5,
	IQM_HALF = 6,
	IQM_FLOAT = 7,
	IQM_DOUBLE = 8,
};

struct IQMVertexArray
{
	IQMVertexArrayType Type;
	uint32_t Flags;
	IQMVertexArrayFormat Format;
	uint32_t Size;
	uint32_t Offset;
};

struct IQMTriangle
{
	uint32_t Vertex[3];
};

struct IQMAdjacency
{
	uint32_t Triangle[3];
};

struct IQMJoint
{
	FString Name;
	int32_t Parent; // parent < 0 means this is a root bone
	float Translate[3];
	float Quaternion[4];
	float Scale[3];
};

struct IQMPose
{
	int32_t Parent; // parent < 0 means this is a root bone
	uint32_t Channelmask; // mask of which 10 channels are present for this joint pose
	float ChannelOffset[10];
	float ChannelScale[10];
	// channels 0..2 are translation <Tx, Ty, Tz> and channels 3..6 are quaternion rotation <Qx, Qy, Qz, Qw>
	// rotation is in relative/parent local space
	// channels 7..9 are scale <Sx, Sy, Sz>
	// output = (input*scale)*rotation + translation
};

struct IQMAnim
{
	FString Name;
	uint32_t FirstFrame;
	uint32_t NumFrames;
	float Framerate;
	bool Loop;
};

struct IQMBounds
{
	float BBMins[3];
	float BBMaxs[3];
	float XYRadius;
	float Radius;
};

class IQMModel : public FModel
{
public:
	IQMModel();
	~IQMModel();

	bool Load(const char* fn, int lumpnum, const char* buffer, int length) override;
	int FindFrame(const char* name) override;
	void RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frame, int frame2, double inter, int translation = 0) override;
	void BuildVertexBuffer(FModelRenderer* renderer) override;
	void AddSkins(uint8_t* hitlist) override;

	void LoadGeometry();
	void UnloadGeometry();

	int mLumpNum = -1;
	TArray<IQMMesh> Meshes;
	TArray<IQMTriangle> Triangles;
	TArray<IQMAdjacency> Adjacency;
	TArray<IQMJoint> Joints;
	TArray<IQMPose> Poses;
	TArray<IQMAnim> Anims;
	TArray<uint16_t> Frames;
	TArray<IQMBounds> Bounds;
	TArray<IQMVertexArray> VertexArrays;
	uint32_t NumVertices = 0;

	TArray<FModelVertex> Vertices;
};
