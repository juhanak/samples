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
#include "MediaSampleProvider.h"
#include "FFmpegReader.h"

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/motion_vector.h>
}

#include <collection.h>
using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace FFmpegInterop;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::MediaProperties;
using namespace Windows::System::Threading;
using namespace Windows::Foundation;
using namespace Windows::Media::Core;
using namespace Windows::UI::Core;
using namespace Windows::Foundation::Collections;
using namespace Platform::Collections;

MediaSampleProvider::MediaSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx)
	: m_pReader(reader)
	, m_pAvFormatCtx(avFormatCtx)
	, m_pAvCodecCtx(avCodecCtx)
	, m_streamIndex(AVERROR_STREAM_NOT_FOUND)
	, m_pAvFrame(nullptr)
	, m_pSwsCtx(nullptr)
{
	for (int i = 0; i < 4; i++)
	{
		m_rgVideoBufferLineSize[i] = 0;
		m_rgVideoBufferData[i] = nullptr;
	}
	video_frame_count = 0;
}



HRESULT MediaSampleProvider::AllocateResources()
{
	HRESULT hr = S_OK;

	m_pAvFrame = av_frame_alloc();
	if (m_pAvFrame == nullptr)
	{
		hr = E_OUTOFMEMORY;
	}
	
	return hr;
}

MediaSampleProvider::~MediaSampleProvider()
{
	if (m_pAvFrame)
	{
		av_freep(m_pAvFrame);
	}
}

void MediaSampleProvider::SetCurrentStreamIndex(int streamIndex)
{
	if (m_pAvCodecCtx != nullptr && m_pAvFormatCtx->nb_streams > (unsigned int)streamIndex)
	{
		m_streamIndex = streamIndex;
	}
	else
	{
		m_streamIndex = AVERROR_STREAM_NOT_FOUND;
	}
}

void MediaSampleProvider::writeFrameSideData(StorageFile ^file)
{
	wchar_t szBuff[200];
	Vector<String^>^ vec = ref new Vector<String^>();

	if (_frameSideData != NULL) {
		const AVMotionVector *mvs = (const AVMotionVector *)_frameSideData->data;
		for (int i = 0; i < _frameSideData->size / sizeof(*mvs); i++) {
			const AVMotionVector *mv = &mvs[i];
			
			swprintf(szBuff, 200, L"%d,%2d,%2d,%2d,%4d,%4d,%4d,%4d,0x%",
				video_frame_count, mv->source,
				mv->w, mv->h, mv->src_x, mv->src_y,
				mv->dst_x, mv->dst_y, mv->flags);

			String ^systemstring = ref new String(szBuff);
			
			if(mv->src_x != mv->dst_x || mv->src_y != mv->dst_y)
				vec->Append(systemstring);

			if (vec->Size > 1000)
			{
				storeVecToFile(file, vec);
				vec->Clear();
			}
		}
		
		if (vec->Size > 0)
		{
			storeVecToFile(file, vec);
			vec->Clear();
		}
	}
}


void MediaSampleProvider::storeVecToFile(StorageFile ^file, Windows::Foundation::Collections::IIterable<Platform::String^>^ lines)
{
	asyncOperationCompleted = false;
	IAsyncAction^ Action = Windows::Storage::FileIO::AppendLinesAsync(file, lines);
	auto t = create_task(Action);

	t.then([this](void) {
		this->asyncOperationCompleted = true;
	}).wait();

	while (false == asyncOperationCompleted)
		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneIfPresent);
}


bool MediaSampleProvider::ProcessNextSample()
{
	HRESULT hr = S_OK;

	MediaStreamSample^ sample;
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;
	DataWriter^ dataWriter = ref new DataWriter();

	bool frameComplete = false;
	bool decodeSuccess = true;

	while (SUCCEEDED(hr) && !frameComplete)
	{
		// Continue reading until there is an appropriate packet in the stream
		while (m_packetQueue.empty())
		{
			if (m_pReader->ReadPacket() < 0)
			{
				DebugMessage(L"GetNextSample reaching EOF\n");
				hr = E_FAIL;
				break;
			}
		}

		if (!m_packetQueue.empty())
		{
			// Pick the packets from the queue one at a time
			avPacket = PopPacket();

			// Decode the packet if necessary
			hr = DecodeAVPacket( &avPacket);
			frameComplete = (hr == S_OK);
		}
	}

	return SUCCEEDED(hr) ? true : false;
}



HRESULT MediaSampleProvider::DecodeAVPacket( AVPacket* avPacket)
{
	int frameComplete = 0;
	if (avcodec_decode_video2(m_pAvCodecCtx, m_pAvFrame, &frameComplete, avPacket) < 0)
	{
		DebugMessage(L"GetNextSample reaching EOF\n");
		frameComplete = 1;
	}

	int i;
	AVFrameSideData *sd;
	video_frame_count++;
	wchar_t szBuff[200];
	sd = av_frame_get_side_data(m_pAvFrame, AV_FRAME_DATA_MOTION_VECTORS);
	if (sd) {
		_frameSideData = sd;
		}
	
	else
	{
		_frameSideData = NULL;
	}

	return frameComplete ? S_OK : S_FALSE;
}

void MediaSampleProvider::PushPacket(AVPacket packet)
{
	DebugMessage(L" - PushPacket\n");

	m_packetQueue.push(packet);
}

AVPacket MediaSampleProvider::PopPacket()
{
	DebugMessage(L" - PopPacket\n");

	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	if (!m_packetQueue.empty())
	{
		avPacket = m_packetQueue.front();
		m_packetQueue.pop();
	}

	return avPacket;
}



