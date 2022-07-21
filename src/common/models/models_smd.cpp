// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2022 Andrew Clarke
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "filesystem.h"
#include "cmdlib.h"
#include "model_smd.h"
#include "texturemanager.h"
#include "modelrenderer.h"
#include "sc_man.h"

bool FSMDModel::Load(const char* path, int lumpnum, const char* buffer, int length)
{
	FString fileName = fileSystem.GetFileFullName(lumpnum);
	mLumpNum = lumpnum;
	mPath = path;

	return true;
}

void FSMDModel::LoadGeometry()
{
	for (unsigned lumps = 0; lumps < animLookups.Size() + 1; lumps++)
	{
		int lumpID = lumps == 0 ? mLumpNum : fileSystem.CheckNumForFullName(Models[animLookups[lumps-1]]->mFileName);
		FScanner sc(lumpID);

		int nodeCounter = -1;
		int framesCounter = -1;
		int groupsCounter = -1;
		int boneCounter = 0;
		bool addGroup = true;
		while (sc.GetString())
		{
			if (sc.Compare("nodes") && lumps == 0)
			{
				while (!sc.Compare("end"))
				{
					sc.MustGetNumber();
					SMDNodes.Push(SMDNode());
					nodeCounter++;
					SMDNodes[nodeCounter].ID = sc.Number;
					sc.MustGetString();
					SMDNodes[nodeCounter].Name = sc.String;
					//SMDNodes[nodeCounter].Name = SMDNodes[nodeCounter].Name.Left(1);
					//SMDNodes[nodeCounter].Name = SMDNodes[nodeCounter].Name.Right(SMDNodes[nodeCounter].Name.Len()-1);
					sc.MustGetNumber();
					SMDNodes[nodeCounter].parentID = sc.Number;
					sc.GetString();
					if (!sc.Compare("end"))
						sc.UnGet();
				}
			}
			if (sc.Compare("skeleton"))
			{
				sc.MustGetString();
				animations.Push(SMDAnimation());
				while (sc.Compare("time"))
				{
					sc.MustGetNumber();
					animations[animations.Size() - 1].SMDSkeleton.Push(SMDSkeletonFrame());

					framesCounter = animations[animations.Size() - 1].SMDSkeleton.Size() - 1;
					animations[animations.Size() - 1].SMDSkeleton[framesCounter].time = sc.Number;
					while (!sc.Compare("end") && !sc.Compare("time"))
					{
						animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes.Push(SMDSkeletonNode());
						boneCounter = animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes.Size() - 1;
						sc.MustGetNumber(); animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes[boneCounter].boneID = sc.Number;
						sc.MustGetFloat();	animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes[boneCounter].posX = (float)sc.Float;
						sc.MustGetFloat();	animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes[boneCounter].posY = (float)sc.Float;
						sc.MustGetFloat();	animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes[boneCounter].posZ = (float)sc.Float;
						sc.MustGetFloat();	animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes[boneCounter].rotX = (float)sc.Float;
						sc.MustGetFloat();	animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes[boneCounter].rotY = (float)sc.Float;
						sc.MustGetFloat();	animations[animations.Size() - 1].SMDSkeleton[framesCounter].skeletonNodes[boneCounter].rotZ = (float)sc.Float;
						sc.GetString();
						if (!(sc.Compare("end") || sc.Compare("time")))
							sc.UnGet();
					}
				}
			}
			if (sc.Compare("triangles"))
			{
				sc.GetString();
				while (!sc.Compare("end"))
				{
					SMDTriangles.Push(SMDTriangle());

					for (unsigned i = 0; i < surfaceSkins.Size(); i++)
					{
						if (!surfaceSkins[i].CompareNoCase(sc.String))
						{
							addGroup = false;
							break;
						}
					}
					if (addGroup)
					{
						surfaceSkins.Push(sc.String);
						groups.Push(SMDGroup());
						groupsCounter++;
						groups[groupsCounter].surface = LoadSkin(mPath, sc.String);
					}

					addGroup = true;

					SMDTriangles[Triangles].surface = sc.String;

					for (int i = 0; i < 3; i++)
					{
						sc.MustGetNumber();
						SMDTriangles[Triangles].parentBone[i] = sc.Number;
						sc.MustGetFloat();
						SMDTriangles[Triangles].posX[i] = (float)sc.Float;
						sc.MustGetFloat();
						SMDTriangles[Triangles].posY[i] = (float)sc.Float;
						sc.MustGetFloat();
						SMDTriangles[Triangles].posZ[i] = (float)sc.Float;
						sc.MustGetFloat();
						SMDTriangles[Triangles].normX[i] = (float)sc.Float;
						sc.MustGetFloat();
						SMDTriangles[Triangles].normY[i] = (float)sc.Float;
						sc.MustGetFloat();
						SMDTriangles[Triangles].normZ[i] = (float)sc.Float;
						sc.MustGetFloat();
						SMDTriangles[Triangles].uvX[i] = (float)sc.Float;
						sc.MustGetFloat();
						SMDTriangles[Triangles].uvY[i] = (float)sc.Float;

						int oldLine = sc.Line;
						sc.GetString();
						if (sc.Line == oldLine)
						{
							sc.UnGet();
							sc.MustGetNumber();
							int links = sc.Number;
							for (unsigned j = 0; j < links; j++)
							{
								sc.GetString();
								if (sc.Line == oldLine)
								{
									sc.UnGet();
									sc.MustGetNumber();
									if(j < 4)
										SMDTriangles[Triangles].boneID[j][i] = sc.Number;
									sc.GetString();
									if (sc.Line == oldLine)
									{
										sc.UnGet();
										sc.MustGetFloat();
										if (j < 4)
											SMDTriangles[Triangles].weight[j+1][i] = sc.Float;
									}
									else { sc.UnGet(); break; }
								}
								else { sc.UnGet(); break; }
							}
						}
						else sc.UnGet();
					}
					if (groupsCounter >= 0 && groupsCounter < (int)groups.Size())
						groups[groupsCounter].groupTriangles.Push(SMDTriangles[Triangles]);
					Triangles++;
					sc.GetString();
				}
			}
		}
	}

	int amountofFrames = 0;
	for (unsigned i = 0; i < animations.Size(); i++)
		amountofFrames += animations[i].SMDSkeleton.Size();

	TArray<VSMatrix> baseframe(SMDNodes.Size(), true);
	TArray<VSMatrix> inversebaseframe(SMDNodes.Size(), true);
	for (uint32_t i = 0; i < SMDNodes.Size(); i++)
	{
		const SMDSkeletonNode& n = animations[0].SMDSkeleton[0].skeletonNodes[i];

		VSMatrix m, invm;
		m.loadIdentity();
		m.translate(n.posX, n.posY, n.posZ);

		FVector3 eulerRotation;
		eulerRotation.X = n.rotX;
		eulerRotation.Y = n.rotY;
		eulerRotation.Z = n.rotZ;

		FVector4 quaternion;
		quaternion.ToQuaternion(eulerRotation);
		m.multQuaternion(quaternion);

		m.inverseMatrix(invm);
		if (SMDNodes[i].parentID >= 0)
		{
			baseframe[i] = baseframe[SMDNodes[i].parentID];
			baseframe[i].multMatrix(m);

			inversebaseframe[i] = invm;
			inversebaseframe[i].multMatrix(inversebaseframe[SMDNodes[i].parentID]);
		}
		else
		{
			baseframe[i] = m;
			inversebaseframe[i] = invm;
		}
	}

	int frameCounter = 0;
	
	frameMatrices.Resize(amountofFrames*SMDNodes.Size());
	for (unsigned i = 0; i < animations.Size(); i++)
	{
		for (unsigned j = 0; j < animations[i].SMDSkeleton.Size(); j++)
		{
			for (unsigned k = 0; k < animations[i].SMDSkeleton[j].skeletonNodes.Size(); k++)
			{
				FVector3 translate;
				translate.X = animations[0].SMDSkeleton[0].skeletonNodes[k].posX; translate.X += animations[i].SMDSkeleton[j].skeletonNodes[k].posX;
				translate.Y = animations[0].SMDSkeleton[0].skeletonNodes[k].posY; translate.Y += animations[i].SMDSkeleton[j].skeletonNodes[k].posY;
				translate.Z = animations[0].SMDSkeleton[0].skeletonNodes[k].posZ; translate.Z += animations[i].SMDSkeleton[j].skeletonNodes[k].posZ;

				FVector3 eulerRotation;
				eulerRotation.X = animations[0].SMDSkeleton[0].skeletonNodes[k].rotX; eulerRotation.X += animations[i].SMDSkeleton[j].skeletonNodes[k].rotX;
				eulerRotation.Y = animations[0].SMDSkeleton[0].skeletonNodes[k].rotY; eulerRotation.Y += animations[i].SMDSkeleton[j].skeletonNodes[k].rotY;
				eulerRotation.Z = animations[0].SMDSkeleton[0].skeletonNodes[k].rotZ; eulerRotation.Z += animations[i].SMDSkeleton[j].skeletonNodes[k].rotZ;

				FVector4 quaternion;
				quaternion.ToQuaternion(eulerRotation);
				quaternion.MakeUnit();

				VSMatrix m;
				m.loadIdentity();
				m.translate(translate.X, translate.Y, translate.Z);
				m.multQuaternion(quaternion);

				VSMatrix& result = frameMatrices[frameCounter + k];

				if (SMDNodes[k].parentID >= 0)
				{
					result = baseframe[SMDNodes[k].parentID];
					result.multMatrix(m);
					result.multMatrix(inversebaseframe[k]);
				}
				else
				{
					result = m;
					result.multMatrix(inversebaseframe[k]);
				}
			}
			frameCounter++;
		}
	}

	mLoaded = true;
}

void FSMDModel::BuildVertexBuffer(FModelRenderer* renderer)
{	
	if (mLoaded == false)
		LoadGeometry();

	if (!GetVertexBuffer(renderer->GetType()))
	{
		auto vbuf = renderer->CreateVertexBuffer(false, animations.Size() == 1);
		SetVertexBuffer(renderer->GetType(), vbuf);
		FModelVertex* vptr = vbuf->LockVertexBuffer(Triangles * 3);

		int vidx = 0;
		for (int i = 0; i < Triangles; i++)
		{
			SMDTriangle tri = SMDTriangles[i];
			for (int j = 0; j < 3; j++)
			{
				FModelVertex* vert = &vptr[vidx++];
				vert->Set(tri.posX[j], tri.posZ[j], tri.posY[j], tri.uvX[j], tri.uvY[j]);
				vert->SetNormal(tri.normX[j], tri.normZ[j], tri.normY[j]);
				vert->SetBoneSelector(tri.parentBone[j], tri.boneID[0][j], tri.boneID[1][j], tri.boneID[2][j]);
				vert->SetBoneWeight((int)clamp((1.0 - tri.weight[1][j] - tri.weight[2][j] - tri.weight[3][j]) * 255.0, 0.0, 255.0),
					(int)clamp(tri.weight[1][j] * 255.0, 0.0, 255.0),
					(int)clamp(tri.weight[2][j] * 255.0, 0.0, 255.0),
					(int)clamp(tri.weight[3][j] * 255.0, 0.0, 255.0));
			}
		}
		vbuf->UnlockVertexBuffer();
	}
}

void FSMDModel::AddSkins(uint8_t* hitlist)
{
	for (unsigned i = 0; i < groups.Size(); i++)
	{
		int ssIndex = i + curMDLIndex * MD3_MAX_SURFACES;
		if (curSpriteMDLFrame && curSpriteMDLFrame->surfaceskinIDs[ssIndex].isValid())
		{
			hitlist[curSpriteMDLFrame->surfaceskinIDs[ssIndex].GetIndex()] |= FTextureManager::HIT_Flat;
		}
		if (groups[i].surface.isValid())
		{
			hitlist[groups[i].surface.GetIndex()] |= FTextureManager::HIT_Flat;
		}
	}
}

int FSMDModel::FindFrame(const char* name)
{
	return -1;
}

void FSMDModel::RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frameno, int frameno2, double inter, DActorSkeletalData* skeleton, int translation)
{
	frameno = clamp(frameno, 0, (int)frameMatrices.Size() - 1);
	frameno2 = clamp(frameno2, 0, (int)frameMatrices.Size() - 1);

	int animationNum = curSpriteMDLFrame->animationID;
	int numbones = SMDNodes.Size();

	int offset1 = frameno * numbones;
	int offset2 = frameno2 * numbones;

	for (unsigned i = 0; i < animationNum; i++)
	{
		offset1 += animations[i].SMDSkeleton.Size() * numbones;
		offset2 += animations[i].SMDSkeleton.Size() * numbones;
	}

	float t = (float)inter;
	float invt = 1.0f - t;

	TArray<VSMatrix> bones(numbones, true);
	for (int i = 0; i < numbones; i++)
	{
		const float* from = frameMatrices[offset1 + i].get();
		const float* to = frameMatrices[offset2 + i].get();

		// Interpolate bone between the two frames
		float bone[16];
		for (int i = 0; i < 16; i++)
		{
			bone[i] = from[i] * invt + to[i] * t;
		}

		// Apply parent bone
		if (SMDNodes[i].parentID >= 0)
		{
			bones[i] = bones[SMDNodes[i].parentID];
			bones[i].multMatrix(bone);
		}
		else
		{
			bones[i].loadMatrix(bone);
		}
	}

	renderer->SetupFrame(this, 0, 0, Triangles * 3, bones);

	int voffset = 0;
	for (unsigned int i = 0; i < groups.Size(); i++)
	{
		int groupTriangles = groups[i].groupTriangles.Size() * 3;
		FGameTexture* surfaceSkin = skin;
		if (!surfaceSkin)
		{
			if (curSpriteMDLFrame)
			{
				int ssIndex = i + curMDLIndex * MD3_MAX_SURFACES;
				if (curSpriteMDLFrame->surfaceskinIDs[ssIndex].isValid())
				{
					surfaceSkin = TexMan.GetGameTexture(curSpriteMDLFrame->surfaceskinIDs[ssIndex], true);
				}
				else if (groups[i].surface.isValid())
				{
					surfaceSkin = TexMan.GetGameTexture(groups[i].surface, true);
				}
			}
			if (!surfaceSkin)
			{
				voffset += groupTriangles;
				continue;
			}
		}

		renderer->SetMaterial(surfaceSkin, false, translation);
		renderer->DrawArrays(voffset, groupTriangles);
		voffset += groupTriangles;
	}
}

bool FSMDModel::AttachAnimations(int id)
{
	animLookups.Push(id);
	return true;
}

bool FSMDModel::ManipulateBones(float moveX, float moveY, float moveZ, float rotX, float rotY, float rotZ, float scaleX, float scaleY, float scaleZ)
{
	return false;
}


