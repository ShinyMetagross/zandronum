// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2014-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
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

#include "hw_bonebuffer.h"
#include "hw_dynlightdata.h"
#include "shaderuniforms.h"

static const int BONE_SIZE = (16*sizeof(float));

BoneBuffer::BoneBuffer(int pipelineNbr) : mPipelineNbr(pipelineNbr)
{
	int maxNumberOfBones = 80000;

	mBufferSize = maxNumberOfBones;
	mByteSize = mBufferSize * BONE_SIZE;

	// Hack alert: On Intel's GL driver SSBO's perform quite worse than UBOs.
	// We only want to disable using SSBOs for bones but not disable the feature entirely.
	// Note that using an uniform buffer here will limit the number of bones per model so it isn't done for NVidia and AMD.
	if (screen->IsVulkan() || screen->IsPoly() || ((screen->hwcaps & RFL_SHADER_STORAGE_BUFFER) && screen->allowSSBO() && !strstr(screen->vendorstring, "Intel")))
	{
		mBufferType = true;
		mBlockAlign = 0;
		mBlockSize = mBufferSize;
		mMaxUploadSize = mBlockSize;
	}
	else
	{
		mBufferType = false;
		mBlockSize = screen->maxuniformblock / BONE_SIZE;
		mBlockAlign = screen->uniformblockalignment / BONE_SIZE;
		mMaxUploadSize = (mBlockSize - mBlockAlign);
	}

	for (int n = 0; n < mPipelineNbr; n++)
	{
		mBufferPipeline[n] = screen->CreateDataBuffer(BONEBUF_BINDINGPOINT, mBufferType, false);
		mBufferPipeline[n]->SetData(mByteSize, nullptr, BufferUsageType::Persistent);
	}

	Clear();
}

BoneBuffer::~BoneBuffer()
{
	delete mBuffer;
}

void BoneBuffer::Clear()
{
	mIndex = 0;

	mPipelinePos++;
	mPipelinePos %= mPipelineNbr;

	mBuffer = mBufferPipeline[mPipelinePos];
}

int BoneBuffer::UploadBones(const TArray<VSMatrix>& bones)
{
	int totalsize = bones.Size();
	if (totalsize > (int)mMaxUploadSize)
	{
		totalsize = mMaxUploadSize;
	}

	float *mBufferPointer = (float*)mBuffer->Memory();
	assert(mBufferPointer != nullptr);
	if (mBufferPointer == nullptr) return -1;
	if (totalsize <= 0) return -1;	// there are no bones

	unsigned thisindex = mIndex.fetch_add(totalsize);

	if (thisindex + totalsize <= mBufferSize)
	{
		memcpy(mBufferPointer + thisindex * BONE_SIZE, bones.Data(), totalsize * BONE_SIZE);
		return thisindex;
	}
	else
	{
		return -1;	// Buffer is full. Since it is being used live at the point of the upload we cannot do much here but to abort.
	}
}

int BoneBuffer::GetBinding(unsigned int index, size_t* pOffset, size_t* pSize)
{
	// this function will only get called if a uniform buffer is used. For a shader storage buffer we only need to bind the buffer once at the start.
	unsigned int offset = (index / mBlockAlign) * mBlockAlign;

	*pOffset = offset * BONE_SIZE;
	*pSize = mBlockSize * BONE_SIZE;
	return (index - offset);
}
