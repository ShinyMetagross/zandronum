
#include "filesystem.h"
#include "cmdlib.h"
#include "model_iqm.h"
#include "texturemanager.h"
#include "modelrenderer.h"
#include "engineerrors.h"
#include "r_utility.h"
#include <math.h>


IQMModel::IQMModel()
{
}

IQMModel::~IQMModel()
{
}

bool IQMModel::Load(const char* path, int lumpnum, const char* buffer, int length)
{
	mLumpNum = lumpnum;

	try
	{
		IQMFileReader reader(buffer, length);

		char magic[16];
		reader.Read(magic, 16);
		if (memcmp(magic, "INTERQUAKEMODEL\0", 16) != 0)
			return false;

		uint32_t version = reader.ReadUInt32();
		if (version != 2)
			return false;

		uint32_t filesize = reader.ReadUInt32();
		uint32_t flags = reader.ReadUInt32();
		uint32_t num_text = reader.ReadUInt32();
		uint32_t ofs_text = reader.ReadUInt32();
		uint32_t num_meshes = reader.ReadUInt32();
		uint32_t ofs_meshes = reader.ReadUInt32();
		uint32_t num_vertexarrays = reader.ReadUInt32();
		uint32_t num_vertices = reader.ReadUInt32();
		uint32_t ofs_vertexarrays = reader.ReadUInt32();
		uint32_t num_triangles = reader.ReadUInt32();
		uint32_t ofs_triangles = reader.ReadUInt32();
		uint32_t ofs_adjacency = reader.ReadUInt32();
		uint32_t num_joints = reader.ReadUInt32();
		uint32_t ofs_joints = reader.ReadUInt32();
		uint32_t num_poses = reader.ReadUInt32();
		uint32_t ofs_poses = reader.ReadUInt32();
		uint32_t num_anims = reader.ReadUInt32();
		uint32_t ofs_anims = reader.ReadUInt32();
		uint32_t num_frames = reader.ReadUInt32();
		uint32_t num_framechannels = reader.ReadUInt32();
		uint32_t ofs_frames = reader.ReadUInt32();
		uint32_t ofs_bounds = reader.ReadUInt32();
		uint32_t num_comment = reader.ReadUInt32();
		uint32_t ofs_comment = reader.ReadUInt32();
		uint32_t num_extensions = reader.ReadUInt32();
		uint32_t ofs_extensions = reader.ReadUInt32();

		if (num_text == 0)
			return false;

		TArray<char> text(num_text, true);
		reader.SeekTo(ofs_text);
		reader.Read(text.Data(), text.Size());
		text[text.Size() - 1] = 0;

		Meshes.Resize(num_meshes);
		Triangles.Resize(num_triangles);
		Adjacency.Resize(num_triangles);
		Joints.Resize(num_joints);
		Poses.Resize(num_poses);
		Anims.Resize(num_anims);
		Bounds.Resize(num_frames);
		VertexArrays.Resize(num_vertexarrays);
		NumVertices = num_vertices;

		reader.SeekTo(ofs_meshes);
		for (IQMMesh& mesh : Meshes)
		{
			mesh.Name = reader.ReadName(text);
			mesh.Material = reader.ReadName(text);
			mesh.FirstVertex = reader.ReadUInt32();
			mesh.NumVertices = reader.ReadUInt32();
			mesh.FirstTriangle = reader.ReadUInt32();
			mesh.NumTriangles = reader.ReadUInt32();
			mesh.Skin = LoadSkin(path, mesh.Material.GetChars());
		}

		reader.SeekTo(ofs_triangles);
		for (IQMTriangle& triangle : Triangles)
		{
			triangle.Vertex[0] = reader.ReadUInt32();
			triangle.Vertex[1] = reader.ReadUInt32();
			triangle.Vertex[2] = reader.ReadUInt32();
		}

		reader.SeekTo(ofs_adjacency);
		for (IQMAdjacency& adj : Adjacency)
		{
			adj.Triangle[0] = reader.ReadUInt32();
			adj.Triangle[1] = reader.ReadUInt32();
			adj.Triangle[2] = reader.ReadUInt32();
		}

		reader.SeekTo(ofs_joints);
		for (IQMJoint& joint : Joints)
		{
			joint.Name = reader.ReadName(text);
			joint.Parent = reader.ReadInt32();
			joint.Translate.X = reader.ReadFloat();
			joint.Translate.Y = reader.ReadFloat();
			joint.Translate.Z = reader.ReadFloat();
			joint.Quaternion.X = reader.ReadFloat();
			joint.Quaternion.Y = reader.ReadFloat();
			joint.Quaternion.Z = reader.ReadFloat();
			joint.Quaternion.W = reader.ReadFloat();
			joint.Quaternion.MakeUnit();
			joint.Scale.X = reader.ReadFloat();
			joint.Scale.Y = reader.ReadFloat();
			joint.Scale.Z = reader.ReadFloat();
		}

		reader.SeekTo(ofs_poses);
		for (IQMPose& pose : Poses)
		{
			pose.Parent = reader.ReadInt32();
			pose.ChannelMask = reader.ReadUInt32();
			for (int i = 0; i < 10; i++) pose.ChannelOffset[i] = reader.ReadFloat();
			for (int i = 0; i < 10; i++) pose.ChannelScale[i] = reader.ReadFloat();
		}

		reader.SeekTo(ofs_anims);
		for (IQMAnim& anim : Anims)
		{
			anim.Name = reader.ReadName(text);
			anim.FirstFrame = reader.ReadUInt32();
			anim.NumFrames = reader.ReadUInt32();
			anim.Framerate = reader.ReadFloat();
			anim.Loop = !!(reader.ReadUInt32() & 1);
		}

		baseframe.Resize(num_joints);
		inversebaseframe.Resize(num_joints);
		for (uint32_t i = 0; i < num_joints; i++)
		{
			const IQMJoint& j = Joints[i];

			VSMatrix m, invm;
			m.loadIdentity();
			m.translate(j.Translate.X, j.Translate.Y, j.Translate.Z);
			m.multQuaternion(j.Quaternion);
			m.scale(j.Scale.X, j.Scale.Y, j.Scale.Z);
			m.inverseMatrix(invm);
			if (j.Parent >= 0)
			{
				baseframe[i] = baseframe[j.Parent];
				baseframe[i].multMatrix(m);
				inversebaseframe[i] = invm;
				inversebaseframe[i].multMatrix(inversebaseframe[j.Parent]);
			}
			else
			{
				baseframe[i] = m;
				inversebaseframe[i] = invm;
			}
		}

		rawTransforms.Resize(num_frames * num_poses);
		FrameTransforms.Resize(num_frames * num_poses);
		reader.SeekTo(ofs_frames);
		for (uint32_t i = 0; i < num_frames; i++)
		{
			for (uint32_t j = 0; j < num_poses; j++)
			{
				const IQMPose& p = Poses[j];

				FVector3 translate;
				translate.X = p.ChannelOffset[0]; if (p.ChannelMask & 0x01) translate.X += reader.ReadUInt16() * p.ChannelScale[0];
				translate.Y = p.ChannelOffset[1]; if (p.ChannelMask & 0x02) translate.Y += reader.ReadUInt16() * p.ChannelScale[1];
				translate.Z = p.ChannelOffset[2]; if (p.ChannelMask & 0x04) translate.Z += reader.ReadUInt16() * p.ChannelScale[2];

				FVector4 quaternion;
				quaternion.X = p.ChannelOffset[3]; if (p.ChannelMask & 0x08) quaternion.X += reader.ReadUInt16() * p.ChannelScale[3];
				quaternion.Y = p.ChannelOffset[4]; if (p.ChannelMask & 0x10) quaternion.Y += reader.ReadUInt16() * p.ChannelScale[4];
				quaternion.Z = p.ChannelOffset[5]; if (p.ChannelMask & 0x20) quaternion.Z += reader.ReadUInt16() * p.ChannelScale[5];
				quaternion.W = p.ChannelOffset[6]; if (p.ChannelMask & 0x40) quaternion.W += reader.ReadUInt16() * p.ChannelScale[6];
				quaternion.MakeUnit();

				FVector3 scale;
				scale.X = p.ChannelOffset[7]; if (p.ChannelMask & 0x80) scale.X += reader.ReadUInt16() * p.ChannelScale[7];
				scale.Y = p.ChannelOffset[8]; if (p.ChannelMask & 0x100) scale.Y += reader.ReadUInt16() * p.ChannelScale[8];
				scale.Z = p.ChannelOffset[9]; if (p.ChannelMask & 0x200) scale.Z += reader.ReadUInt16() * p.ChannelScale[9];

				VSMatrix m;
				m.loadIdentity();
				m.translate(translate.X, translate.Y, translate.Z);
				m.multQuaternion(quaternion);
				m.scale(scale.X, scale.Y, scale.Z);

				//We probably need to concatenate once we have all our transformations we want, so let's store raw values here if needed.
				rawTransforms[i * num_poses + j] = m;

				// Concatenate each pose with the inverse base pose to avoid doing this at animation time.
				// If the joint has a parent, then it needs to be pre-concatenated with its parent's base pose.
				// Thus it all negates at animation time like so: 
				//   (parentPose * parentInverseBasePose) * (parentBasePose * childPose * childInverseBasePose) =>
				//   parentPose * (parentInverseBasePose * parentBasePose) * childPose * childInverseBasePose =>
				//   parentPose * childPose * childInverseBasePose
				VSMatrix& result = FrameTransforms[i * num_poses + j];
				if (p.Parent >= 0)
				{
					result = baseframe[p.Parent];
					result.multMatrix(m);
					result.multMatrix(inversebaseframe[j]);
				}
				else
				{
					result = m;
					result.multMatrix(inversebaseframe[j]);
				}
			}
		}

		reader.SeekTo(ofs_bounds);
		for (IQMBounds& bound : Bounds)
		{
			bound.BBMins[0] = reader.ReadFloat();
			bound.BBMins[1] = reader.ReadFloat();
			bound.BBMins[2] = reader.ReadFloat();
			bound.BBMaxs[0] = reader.ReadFloat();
			bound.BBMaxs[1] = reader.ReadFloat();
			bound.BBMaxs[2] = reader.ReadFloat();
			bound.XYRadius = reader.ReadFloat();
			bound.Radius = reader.ReadFloat();
		}

		reader.SeekTo(ofs_vertexarrays);
		for (IQMVertexArray& vertexArray : VertexArrays)
		{
			vertexArray.Type = (IQMVertexArrayType)reader.ReadUInt32();
			vertexArray.Flags = reader.ReadUInt32();
			vertexArray.Format = (IQMVertexArrayFormat)reader.ReadUInt32();
			vertexArray.Size = reader.ReadUInt32();
			vertexArray.Offset = reader.ReadUInt32();
		}

		return true;
	}
	catch (IQMReadErrorException)
	{
		return false;
	}
}

void IQMModel::LoadGeometry()
{
	try
	{
		FileData lumpdata = fileSystem.ReadFile(mLumpNum);
		IQMFileReader reader(lumpdata.GetMem(), (int)lumpdata.GetSize());

		Vertices.Resize(NumVertices);
		for (IQMVertexArray& vertexArray : VertexArrays)
		{
			reader.SeekTo(vertexArray.Offset);
			if (vertexArray.Type == IQM_POSITION)
			{
				LoadPosition(reader, vertexArray);
			}
			else if (vertexArray.Type == IQM_TEXCOORD)
			{
				LoadTexcoord(reader, vertexArray);
			}
			else if (vertexArray.Type == IQM_NORMAL)
			{
				LoadNormal(reader, vertexArray);
			}
			else if (vertexArray.Type == IQM_BLENDINDEXES)
			{
				LoadBlendIndexes(reader, vertexArray);
			}
			else if (vertexArray.Type == IQM_BLENDWEIGHTS)
			{
				LoadBlendWeights(reader, vertexArray);
			}
		}
	}
	catch (IQMReadErrorException)
	{
	}
}

void IQMModel::LoadPosition(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	float lu = 0.0f, lv = 0.0f, lindex = -1.0f;
	if (vertexArray.Format == IQM_FLOAT && vertexArray.Size == 3)
	{
		for (FModelVertex& v : Vertices)
		{
			v.x = reader.ReadFloat();
			v.y = reader.ReadFloat();
			v.z = reader.ReadFloat();
			v.lu = lu;
			v.lv = lv;
			v.lindex = lindex;
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_POSITION vertex format");
	}
}

void IQMModel::LoadTexcoord(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	if (vertexArray.Format == IQM_FLOAT && vertexArray.Size == 2)
	{
		for (FModelVertex& v : Vertices)
		{
			v.u = reader.ReadFloat();
			v.v = reader.ReadFloat();
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_TEXCOORD vertex format");
	}
}

void IQMModel::LoadNormal(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	if (vertexArray.Format == IQM_FLOAT && vertexArray.Size == 3)
	{
		for (FModelVertex& v : Vertices)
		{
			v.SetNormal(reader.ReadFloat(), reader.ReadFloat(), reader.ReadFloat());
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_NORMAL vertex format");
	}
}

void IQMModel::LoadBlendIndexes(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	if (vertexArray.Format == IQM_UBYTE && vertexArray.Size == 4)
	{
		for (FModelVertex& v : Vertices)
		{
			v.SetBoneSelector(reader.ReadUByte(), reader.ReadUByte(), reader.ReadUByte(), reader.ReadUByte());
		}
	}
	else if (vertexArray.Format == IQM_INT && vertexArray.Size == 4)
	{
		for (FModelVertex& v : Vertices)
		{
			v.SetBoneSelector(reader.ReadInt32(), reader.ReadInt32(), reader.ReadInt32(), reader.ReadInt32());
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_BLENDINDEXES vertex format");
	}
}

void IQMModel::LoadBlendWeights(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	if (vertexArray.Format == IQM_UBYTE && vertexArray.Size == 4)
	{
		for (FModelVertex& v : Vertices)
		{
			v.SetBoneWeight(reader.ReadUByte(), reader.ReadUByte(), reader.ReadUByte(), reader.ReadUByte());
		}
	}
	else if (vertexArray.Format == IQM_FLOAT && vertexArray.Size == 4)
	{
		for (FModelVertex& v : Vertices)
		{
			uint8_t x = (int)clamp(reader.ReadFloat() * 255.0f, 0.0f, 255.0f);
			uint8_t y = (int)clamp(reader.ReadFloat() * 255.0f, 0.0f, 255.0f);
			uint8_t z = (int)clamp(reader.ReadFloat() * 255.0f, 0.0f, 255.0f);
			uint8_t w = (int)clamp(reader.ReadFloat() * 255.0f, 0.0f, 255.0f);
			v.SetBoneWeight(x, y, z, w);
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_BLENDWEIGHTS vertex format");
	}
}

void IQMModel::UnloadGeometry()
{
	Vertices.Reset();
}

int IQMModel::FindFrame(const char* name)
{
	// To do: how does this map to anything in an iqm model? the animation perhaps?

	for (unsigned i = 0; i < Anims.Size(); i++)
	{
		if (!stricmp(name, Anims[i].Name.GetChars())) return i;
	}
	return -1;
}

void IQMModel::RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frame1, int frame2, double inter, DActorSkeletalData* skeleton, int translation)
{
	frame1 = clamp(frame1, 0, (int)FrameTransforms.Size() - 1);
	frame2 = clamp(frame2, 0, (int)FrameTransforms.Size() - 1);

	int numbones = Joints.Size();
	int offset1 = frame1 * numbones;
	int offset2 = frame2 * numbones;
	float t = (float)inter;
	float invt = 1.0f - t;

	TArray<VSMatrix> bones(numbones, true);
	for (int i = 0; i < numbones; i++)
	{
		VSMatrix framePosition1 = FrameTransforms[offset1 + i];
		VSMatrix framePosition2 = FrameTransforms[offset2 + i];

		if (skeleton != nullptr)
		{
			if (skeleton->transform.Size() > i && skeleton->moddedBone[i])
			{
				VSMatrix pos1;
				VSMatrix pos2;
				pos1.loadIdentity();
				pos2.loadIdentity();

				VSMatrix pos1mod = rawTransforms[offset1 + i];
				VSMatrix pos2mod = rawTransforms[offset2 + i];

				pos1mod.multMatrix(skeleton->oldTransform[i]);
				pos2mod.multMatrix(skeleton->transform[i]);

				if (Joints[i].Parent >= 0)
				{
					pos1 = baseframe[Joints[i].Parent];		
					pos1.multMatrix(pos1mod);	
					pos1.multMatrix(inversebaseframe[i]);

					pos2 = baseframe[Joints[i].Parent];
					pos2.multMatrix(pos2mod);
					pos2.multMatrix(inversebaseframe[i]);
				}
				else
				{
					pos1 = pos1mod;
					pos1.multMatrix(inversebaseframe[i]);

					pos2 = pos2mod;
					pos2.multMatrix(inversebaseframe[i]);
				}
				framePosition1 = pos1;
				framePosition2 = pos2;
			}
		}

		const float* from = framePosition1.get();
		const float* to = framePosition2.get();

		// Interpolate bone between the two frames
		float bone[16];
		for (int i = 0; i < 16; i++)
		{
			bone[i] = from[i] * invt + to[i] * t;
		}

		// Apply parent bone
		if (Joints[i].Parent >= 0)
		{
			bones[i] = bones[Joints[i].Parent];
			bones[i].multMatrix(bone);
		}
		else
		{
			bones[i].loadMatrix(bone);
		}
	}

	renderer->SetupFrame(this, 0, 0, NumVertices, bones);

	FGameTexture* lastSkin = nullptr;
	for (IQMMesh& mesh : Meshes)
	{
		FGameTexture* meshSkin = skin;
		if (!meshSkin)
		{
			if (!mesh.Skin.isValid()) continue;
			meshSkin = TexMan.GetGameTexture(mesh.Skin, true);
			if (!meshSkin) continue;
		}

		if (meshSkin != lastSkin)
		{
			renderer->SetMaterial(meshSkin, false, translation);
			lastSkin = meshSkin;
		}

		renderer->DrawElements(mesh.NumTriangles * 3, mesh.FirstTriangle * 3 * sizeof(unsigned int));
	}
}

void IQMModel::BuildVertexBuffer(FModelRenderer* renderer)
{
	if (!GetVertexBuffer(renderer->GetType()))
	{
		LoadGeometry();

		auto vbuf = renderer->CreateVertexBuffer(true, true);
		SetVertexBuffer(renderer->GetType(), vbuf);

		FModelVertex* vertptr = vbuf->LockVertexBuffer(Vertices.Size());
		memcpy(vertptr, Vertices.Data(), Vertices.Size() * sizeof(FModelVertex));
		vbuf->UnlockVertexBuffer();

		unsigned int* indxptr = vbuf->LockIndexBuffer(Triangles.Size() * 3);
		memcpy(indxptr, Triangles.Data(), Triangles.Size() * sizeof(unsigned int) * 3);
		vbuf->UnlockIndexBuffer();

		UnloadGeometry();
	}
}

void IQMModel::AddSkins(uint8_t* hitlist)
{
	for (IQMMesh& mesh : Meshes)
	{
		if (mesh.Skin.isValid())
		{
			hitlist[mesh.Skin.GetIndex()] |= FTextureManager::HIT_Flat;
		}
	}
}

bool IQMModel::AttachAnimations(int id)
{
	return false;
}

DVector3 IQMModel::ReturnBoneTransform(int index, int bone, int alias, DActorSkeletalData* skeleton)
{
	DVector3 result = DVector3(0, 0, 0);
	int frame = curSpriteMDLFrame->modelframes[0];
	int numbones = Joints.Size();

	VSMatrix framePosition = rawTransforms[curSpriteMDLFrame->modelframes[0] * numbones + bone];

	if (skeleton != nullptr)
		framePosition.multMatrix(skeleton->transform[bone]);

	double scalarX1 = framePosition.get()[0];
	double scalarX2 = framePosition.get()[4];
	double scalarX3 = framePosition.get()[8];

	double scalarY1 = framePosition.get()[1];
	double scalarY2 = framePosition.get()[5];
	double scalarY3 = framePosition.get()[9];

	double scalarZ1 = framePosition.get()[2];
	double scalarZ2 = framePosition.get()[6];
	double scalarZ3 = framePosition.get()[10];

	double scalingFactorX = sqrt(scalarX1 * scalarX1 + scalarX2 * scalarX2 + scalarX3 * scalarX3);
	double scalingFactorY = sqrt(scalarY1 * scalarY1 + scalarY2 * scalarY2 + scalarY3 * scalarY3);
	double scalingFactorZ = sqrt(scalarZ1 * scalarZ1 + scalarZ2 * scalarZ2 + scalarZ3 * scalarZ3);

	switch (alias)
	{
		default: 
			return DVector3(0, 0, 0); 
		case 1:
			result.X = framePosition.get()[12];
			result.Y = framePosition.get()[13];
			result.Z = framePosition.get()[14];
			break;
		case 2:
			DMatrix3x3 rotationSub;
			rotationSub[0][0] = framePosition.get()[0];
			rotationSub[0][1] = framePosition.get()[1];
			rotationSub[0][2] = framePosition.get()[2];

			rotationSub[1][0] = framePosition.get()[4];
			rotationSub[1][1] = framePosition.get()[5];
			rotationSub[1][2] = framePosition.get()[6];

			rotationSub[2][0] = framePosition.get()[8];
			rotationSub[2][1] = framePosition.get()[9];
			rotationSub[2][2] = framePosition.get()[10];

			DMatrix3x3 rotationMatrix_X = (1.0 / scalingFactorX) * rotationSub;
			DMatrix3x3 rotationMatrix_Y = (1.0 / scalingFactorY) * rotationSub;
			DMatrix3x3 rotationMatrix_Z = (1.0 / scalingFactorZ) * rotationSub;

			result = framePosition.eulerAngles(rotationMatrix_X) * (180 / 3.14159265);

			if (result.X < 0)
				result.X += 360.0f;
			if (result.Y < 0)
				result.Y += 360.0f;
			if (result.Z < 0)
				result.Z += 360.0f;

			break;
		case 3:
			result.X = scalingFactorX;
			result.Y = scalingFactorY;
			result.Z = scalingFactorZ;
			break;
	}

	//Printf("[ %f, %f, %f, %f,\n %f, %f, %f, %f,\n %f, %f, %f, %f,\n %f, %f, %f, 1.0 ]\n",
		//framePosition.get()[0], framePosition.get()[1], framePosition.get()[2], framePosition.get()[3], framePosition.get()[4], framePosition.get()[5], framePosition.get()[6], framePosition.get()[7], framePosition.get()[8], framePosition.get()[9], framePosition.get()[10], framePosition.get()[11], framePosition.get()[13], framePosition.get()[14]);
	return result;
}
