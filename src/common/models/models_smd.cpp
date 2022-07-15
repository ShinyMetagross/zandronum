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
#include "r_utility.h"

TDeletingArray<FSMDModel*> animationClips;

bool FSMDModel::Load(const char* path, int lumpnum, const char* buffer, int length)
{
	FString fileName = fileSystem.GetFileFullName(lumpnum);
	mLumpNum = lumpnum;
	mPath = path;

	return true;
}

void FSMDModel::LoadGeometry()
{
	FScanner sc(mLumpNum);
	FString hdr = "";
	sc.GetString();
	hdr.AppendCharacter(' ');
	sc.GetString();
	int nodeCounter = -1;
	int framesCounter = -1;
	int groupsCounter = -1;
	int boneCounter = 0;
	bool addGroup = true;
	if (hdr.Compare("version 1"))
	{
		while (sc.GetString())
		{
			if (sc.Compare("nodes"))
			{
				while (!sc.Compare("end"))
				{
					sc.MustGetNumber();
					SMDNodes.Push(*new SMDNode());
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
				while(sc.Compare("time"))
				{
					sc.MustGetNumber();
					SMDSkeleton.Push(*new SMDSkeletonFrame());
					framesCounter = SMDSkeleton.Size() - 1;
					SMDSkeleton[framesCounter].time = sc.Number;
					while (!sc.Compare("end") && !sc.Compare("time"))
					{			
						SMDSkeleton[framesCounter].skeletonNodes.Push(*new SMDSkeletonNode());
						boneCounter = SMDSkeleton[framesCounter].skeletonNodes.Size() - 1;
						sc.MustGetNumber(); SMDSkeleton[framesCounter].skeletonNodes[boneCounter].boneID = sc.Number;
						sc.MustGetFloat();	SMDSkeleton[framesCounter].skeletonNodes[boneCounter].posX = (float)sc.Float;
						sc.MustGetFloat();	SMDSkeleton[framesCounter].skeletonNodes[boneCounter].posY = (float)sc.Float;
						sc.MustGetFloat();	SMDSkeleton[framesCounter].skeletonNodes[boneCounter].posZ = (float)sc.Float;
						sc.MustGetFloat();	SMDSkeleton[framesCounter].skeletonNodes[boneCounter].rotX = (float)sc.Float;
						sc.MustGetFloat();	SMDSkeleton[framesCounter].skeletonNodes[boneCounter].rotY = (float)sc.Float;
						sc.MustGetFloat();	SMDSkeleton[framesCounter].skeletonNodes[boneCounter].rotZ = (float)sc.Float;
						sc.GetString();
						if (!(sc.Compare("end") || sc.Compare("time")))
							sc.UnGet();
					}
				}
			}
			if (sc.Compare("triangles"))
			{
				sc.GetString();
				while(!sc.Compare("end"))
				{
					SMDTriangles.Push(*new SMDTriangle());

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
						groups.Push(*new SMDGroup());
						groupsCounter++;
						groups[groupsCounter].surface = LoadSkin(mPath, sc.String);
					}
					
					addGroup = true;

					SMDTriangles[Triangles].surface = sc.String;

					int oldLine = sc.Line;
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
						
						oldLine = sc.Line;
						sc.GetString();
						if (sc.Line == oldLine)
						{
							sc.UnGet();
							sc.MustGetNumber();
							if (sc.Line == oldLine)
							{
								sc.MustGetNumber();
								if (sc.Line == oldLine)
								{
									SMDTriangles[Triangles].parentBone[i] = sc.Number;
									sc.MustGetFloat();
								}
								else sc.UnGet();
							}
							else sc.UnGet();	
						}
						else sc.UnGet();
					}
					if(groupsCounter >= 0 && groupsCounter < (int)groups.Size())
						groups[groupsCounter].groupTriangles.Push(SMDTriangles[Triangles]);
					Triangles++;
					sc.GetString();
				}
			}
		}
		mLoaded = true;
	}
}

void FSMDModel::BuildVertexBuffer(FModelRenderer* renderer)
{	
	if(mLoaded == false) 
		LoadGeometry();

	if (!GetVertexBuffer(renderer->GetType()))
	{
		auto vbuf = renderer->CreateVertexBuffer(false, SMDSkeleton.Size() == 1);
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
				vert->SetBone(tri.parentBone[j], 0);
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

void FSMDModel::RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frameno, int frameno2, double inter, int translation)
{
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

		if (curSpriteMDLFrame->animationID != -1)
		{
			FSMDModel* newSMDModel = animationClips[curSpriteMDLFrame->animationID];
			newSMDModel->BuildVertexBuffer(renderer);

			for (unsigned i = 0; i < newSMDModel->SMDSkeleton[frameno].skeletonNodes.Size(); i++)
			{
				float rotX = cos(newSMDModel->SMDSkeleton[frameno].skeletonNodes[i].rotX / 2);
				float rotY = cos(newSMDModel->SMDSkeleton[frameno].skeletonNodes[i].rotY / 2);
				float rotZ = cos(newSMDModel->SMDSkeleton[frameno].skeletonNodes[i].rotZ / 2);
				float rotW = cos(newSMDModel->SMDSkeleton[frameno].skeletonNodes[i].rotX / 2);

				float cy = cos(rotY * 0.5);
				float sy = sin(rotY * 0.5);
				float cp = cos(rotX * 0.5);
				float sp = sin(rotX * 0.5);
				float cr = cos(rotZ * 0.5);
				float sr = sin(rotZ * 0.5);

				boneArray[i]->Translate(newSMDModel->SMDSkeleton[frameno].skeletonNodes[i].posX, newSMDModel->SMDSkeleton[frameno].skeletonNodes[i].posY, newSMDModel->SMDSkeleton[frameno].skeletonNodes[i].posZ);
				boneArray[i]->Rotate(sr * cp * cy - cr * sp * sy, 
					cr * sp * cy + sr * cp * sy,
					cr * cp * sy - sr * sp * cy,
					cr * cp * cy + sr * sp * sy);
			}
			renderer->SetupBones(this, frameno, frameno2, groupTriangles, boneArray);
		}
	
		renderer->SetMaterial(surfaceSkin, false, translation);
		renderer->SetupFrame(this, voffset, voffset, groupTriangles);
		renderer->DrawArrays(0, groupTriangles);
		voffset += groupTriangles;
	}
}



