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

#pragma once
#include <queue>

extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

using namespace Windows::Storage::Streams;
using namespace Windows::Media::Core;
using namespace Platform;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;

namespace FFmpegInterop
{
	ref class FFmpegInteropMSS;
	ref class FFmpegReader;

	ref class MediaSampleProvider
	{
	public:

		virtual ~MediaSampleProvider();
		virtual bool ProcessNextSample();
		virtual void SetCurrentStreamIndex(int streamIndex);
		void writeFrameSideData(StorageFile ^file);
		
	internal:
		void PushPacket(AVPacket packet);
		AVPacket PopPacket();
		AVFrameSideData *_frameSideData;

	private:
		void storeVecToFile(StorageFile ^file, Windows::Foundation::Collections::IIterable<Platform::String^>^ lines);
		std::queue<AVPacket> m_packetQueue;
		int m_streamIndex;
		AVFrame* m_pAvFrame;
		SwsContext* m_pSwsCtx;
		int m_rgVideoBufferLineSize[4];
		uint8_t* m_rgVideoBufferData[4];
		int video_frame_count = 0;
		volatile bool asyncOperationCompleted = false;

	internal:
		// The FFmpeg context. Because they are complex types
		// we declare them as internal so they don't get exposed
		// externally
		FFmpegReader^ m_pReader;
		AVFormatContext* m_pAvFormatCtx;
		AVCodecContext* m_pAvCodecCtx;

	internal:
		MediaSampleProvider(
			FFmpegReader^ reader,
			AVFormatContext* avFormatCtx,
			AVCodecContext* avCodecCtx);
		virtual HRESULT AllocateResources();

		virtual HRESULT DecodeAVPacket(AVPacket* avPacket);
	};
}