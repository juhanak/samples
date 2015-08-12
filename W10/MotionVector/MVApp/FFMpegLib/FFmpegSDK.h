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
#include "MediaSampleProvider.h"
#include "FFmpegReader.h"
#include "MediaSampleProvider.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Media::Core;
using namespace Windows::Storage::Streams;
using namespace FFmpegInterop;
using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;

extern "C"
{
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
}

namespace FFmpegLib
{
	public ref class FFmpegSDK sealed
	{
	public:
		static FFmpegSDK^ Initialize();
		void ReadPackets(IRandomAccessStream^ stream, StorageFile ^file);

		property int FramesProcessed {
			int get() {
				return framesProcessed;
			}
		}

	private:
		FFmpegSDK();
		HRESULT CreateMediaStreamSource(IRandomAccessStream^ stream);
		HRESULT InitFFmpegContext();
		HRESULT CreateVideoStreamDescriptor();
		
	internal:
		FFmpegReader^ m_pReader;
		AVFormatContext* m_pAvFormatCtx;
		AVCodecContext* m_pAvCodecCtx;

	private:
		IStream* m_fileStreamData;
		unsigned char* m_fileStreamBuffer;
		TimeSpan mediaDuration;

		AVIOContext* avIOCtx;
		AVFormatContext* avFormatCtx;
		AVCodecContext* avVideoCodecCtx;
		int videoStreamIndex;
		MediaSampleProvider^ videoSampleProvider;
		VideoStreamDescriptor^ videoStreamDescriptor;
		int framesProcessed;

	};
}
