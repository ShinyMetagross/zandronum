#pragma once
#include "model.h"
#include "filesystem.h"

#define SMD_HEADER	"version 1"

class FSMDModel;
extern TDeletingArray<FSMDModel*> animationClips;

class FSMDModel : public FModel
{
	struct SMDNode
	{
		unsigned int ID;
		FString Name;
		int parentID;
	};

	struct SMDSkeletonNode
	{
		unsigned int boneID;
		float posX, posY, posZ, rotX, rotY, rotZ;
	};

	struct SMDSkeletonFrame
	{
		unsigned int time;
		TArray<SMDSkeletonNode> skeletonNodes;
	};

	struct SMDTriangle
	{
		FString surface;
		unsigned int parentBone[3];
		float posX[3], posY[3], posZ[3], normX[3], normY[3], normZ[3], uvX[3], uvY[3];
		unsigned int boneID[3];
	};

	//SMD doesn't have groups. But we need to create artifical ones.
	struct SMDGroup
	{
		FTextureID surface;
		TArray<SMDTriangle> groupTriangles;
	};

	TArray<SMDGroup>	groups;
	TArray<SMDTriangle> SMDTriangles;
	TArray<SMDNode>		SMDNodes;
	TArray<FString>		surfaceSkins;
	TArray<SMDSkeletonFrame> SMDSkeleton;
	int mLumpNum;
	int Triangles = 0;
	int Frames = 0;
	FString mPath;
	bool mLoaded = false;
	TArray<Matrix3x4> boneArray;

public:
	FSMDModel() = default;
	virtual bool Load(const char* fn, int lumpnum, const char* buffer, int length);
	virtual int FindFrame(const char* name);
	virtual void RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frame, int frame2, double inter, int translation = 0);
	void LoadGeometry();
	void BuildVertexBuffer(FModelRenderer* renderer);
	virtual void AddSkins(uint8_t* hitlist);
	void LoadAnimations();
};