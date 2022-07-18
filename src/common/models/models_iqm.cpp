
#include "filesystem.h"
#include "cmdlib.h"
#include "model_iqm.h"
#include "texturemanager.h"
#include "modelrenderer.h"

struct IQMReadErrorException { };

class IQMFileReader
{
public:
	IQMFileReader(const void* buffer, int length) : buffer((const char*)buffer), length(length) { }

	uint8_t ReadUByte()
	{
		uint8_t value;
		Read(&value, sizeof(uint8_t));
		return value;
	}

	int32_t ReadInt32()
	{
		int32_t value;
		Read(&value, sizeof(int32_t));
		value = LittleLong(value);
		return value;
	}

	int16_t ReadInt16()
	{
		int16_t value;
		Read(&value, sizeof(int16_t));
		value = LittleShort(value);
		return value;
	}

	uint32_t ReadUInt32()
	{
		return ReadInt32();
	}

	uint16_t ReadUInt16()
	{
		return ReadInt16();
	}

	float ReadFloat()
	{
		float value;
		Read(&value, sizeof(float));
		return value;
	}

	FString ReadName(const TArray<char>& textBuffer)
	{
		uint32_t nameOffset = ReadUInt32();
		if (nameOffset >= textBuffer.Size())
			throw IQMReadErrorException();
		return textBuffer.Data() + nameOffset;
	}

	void Read(void* data, int size)
	{
		if (pos + size > length || size < 0 || size > 0x0fffffff)
			throw IQMReadErrorException();
		memcpy(data, buffer + pos, size);
		pos += size;
	}

	void SeekTo(int newPos)
	{
		if (newPos < 0 || newPos > length)
			throw IQMReadErrorException();
		pos = newPos;
	}

private:
	const char* buffer = nullptr;
	int length = 0;
	int pos = 0;
};

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
		Frames.Resize(num_frames);
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
			joint.Translate[0] = reader.ReadFloat();
			joint.Translate[1] = reader.ReadFloat();
			joint.Translate[2] = reader.ReadFloat();
			joint.Quaternion[0] = reader.ReadFloat();
			joint.Quaternion[1] = reader.ReadFloat();
			joint.Quaternion[2] = reader.ReadFloat();
			joint.Quaternion[3] = reader.ReadFloat();
			joint.Scale[0] = reader.ReadFloat();
			joint.Scale[1] = reader.ReadFloat();
			joint.Scale[2] = reader.ReadFloat();
		}

		reader.SeekTo(ofs_poses);
		for (IQMPose& pose : Poses)
		{
			pose.Parent = reader.ReadInt32();
			pose.Channelmask = reader.ReadUInt32();
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

		reader.SeekTo(ofs_frames);
		for (uint16_t& frame : Frames)
		{
			frame = reader.ReadUInt16();
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
			if (vertexArray.Type == IQM_POSITION && vertexArray.Format == IQM_FLOAT && vertexArray.Size == 3)
			{
				for (FModelVertex& v : Vertices)
				{
					v.x = reader.ReadFloat();
					v.y = reader.ReadFloat();
					v.z = reader.ReadFloat();
					v.lu = 0.0f;
					v.lv = 0.0f;
					v.lindex = -1.0f;
				}
			}
			else if (vertexArray.Type == IQM_TEXCOORD && vertexArray.Format == IQM_FLOAT && vertexArray.Size == 2)
			{
				for (FModelVertex& v : Vertices)
				{
					v.u = reader.ReadFloat();
					v.v = reader.ReadFloat();
				}
			}
			else if (vertexArray.Type == IQM_NORMAL && vertexArray.Format == IQM_FLOAT && vertexArray.Size == 3)
			{
				for (FModelVertex& v : Vertices)
				{
					v.SetNormal(reader.ReadFloat(), reader.ReadFloat(), reader.ReadFloat());
				}
			}
			else if (vertexArray.Type == IQM_BLENDINDEXES && vertexArray.Format == IQM_UBYTE && vertexArray.Size == 4)
			{
				for (FModelVertex& v : Vertices)
				{
					v.SetBoneSelector(reader.ReadUByte(), reader.ReadUByte(), reader.ReadUByte(), reader.ReadUByte());
				}
			}
			else if (vertexArray.Type == IQM_BLENDWEIGHTS && vertexArray.Format == IQM_UBYTE && vertexArray.Size == 4)
			{
				for (FModelVertex& v : Vertices)
				{
					v.SetBoneWeight(reader.ReadUByte(), reader.ReadUByte(), reader.ReadUByte(), reader.ReadUByte());
				}
			}
		}
	}
	catch (IQMReadErrorException)
	{
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

void IQMModel::RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frame, int frame2, double inter, int translation)
{
	// To do: is the frame a specific mesh? a specific animation? should it always render all the meshes?

	if ((unsigned)frame >= Anims.Size() || (unsigned)frame2 >= Anims.Size()) return;

	renderer->SetInterpolation(inter);

	TArray<VSMatrix> bones(Joints.Size());
	for (IQMJoint& join : Joints)
	{
		VSMatrix bone;
		bone.loadIdentity();

		// To do: calculate the bone position here

		bones.Push(bone);
	}

	renderer->SetupFrame(this, 0, 0, NumVertices, bones);

	for (IQMMesh& mesh : Meshes)
	{
		FGameTexture* meshSkin = skin;
		if (!meshSkin)
		{
			if (!mesh.Skin.isValid()) continue;
			meshSkin = TexMan.GetGameTexture(mesh.Skin, true);
			if (!meshSkin) continue;
		}

		renderer->SetMaterial(meshSkin, false, translation);
		renderer->DrawElements(mesh.NumTriangles, mesh.FirstTriangle);
	}

	renderer->SetInterpolation(0.f);
}

void IQMModel::BuildVertexBuffer(FModelRenderer* renderer)
{
	if (!GetVertexBuffer(renderer->GetType()))
	{
		LoadGeometry();

		auto vbuf = renderer->CreateVertexBuffer(false, true);
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
