//*****************************************************************************
//
//	Copyright 2015 Microsoft Corporation
//
//	Licensed under the Apache License, Version 2.0 (the "License");
//	you may not use this file except in compliance with the License.
//	You may obtain a copy of the License at
//
//	http ://www.apache.org/licenses/LICENSE-2.0
//
//	Unless required by applicable law or agreed to in writing, software
//	distributed under the License is distributed on an "AS IS" BASIS,
//	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//	See the License for the specific language governing permissions and
//	limitations under the License.
//
//*****************************************************************************

// This is a modified version of the sample code which is published here 
// https://github.com/Microsoft/FFmpegInterop
// Modified work Copyright Juhana Koski.


#include "pch.h"
#include "FFmpegReader.h"

using namespace FFmpegInterop;

FFmpegReader::FFmpegReader(AVFormatContext* avFormatCtx)
	: m_pAvFormatCtx(avFormatCtx)
	, m_videoStreamIndex(AVERROR_STREAM_NOT_FOUND)
{

}

FFmpegReader::~FFmpegReader()
{

}

// Read the next packet from the stream and push it into the appropriate
// sample provider
int FFmpegReader::ReadPacket()
{
	int ret;
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	ret = av_read_frame(m_pAvFormatCtx, &avPacket);
	if (ret < 0)
	{
		return ret;
	}

	if (avPacket.stream_index == m_videoStreamIndex && m_videoSampleProvider != nullptr)
	{
		m_videoSampleProvider->PushPacket(avPacket);
	}
	else
	{
		av_free_packet(&avPacket);
	}

	return ret;
}


void FFmpegReader::SetVideoStream(int videoStreamIndex, MediaSampleProvider^ videoSampleProvider)
{
	m_videoStreamIndex = videoStreamIndex;
	m_videoSampleProvider = videoSampleProvider;
	if (videoSampleProvider != nullptr)
	{
		videoSampleProvider->SetCurrentStreamIndex(m_videoStreamIndex);
	}
}

