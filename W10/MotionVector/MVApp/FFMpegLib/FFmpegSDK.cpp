#include "pch.h"
#include "FFmpegSDK.h"
#include <shcore.h>
#include "FFmpegReader.h"

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

using namespace concurrency;
using namespace Platform;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::MediaProperties;
using namespace FFmpegInterop;
using namespace FFmpegLib;
using namespace Windows::System::Threading;


// Size of the buffer when reading a stream
const int FILESTREAMBUFFERSZ = 16384;

// Static functions passed to FFmpeg for stream interop
static int FileStreamRead(void* ptr, uint8_t* buf, int bufSize);
static int64_t FileStreamSeek(void* ptr, int64_t pos, int whence);

FFmpegSDK::FFmpegSDK()
	: avIOCtx(nullptr)
	, avFormatCtx(nullptr)
	, avVideoCodecCtx(nullptr)
	, videoStreamIndex(AVERROR_STREAM_NOT_FOUND)
	, m_fileStreamData(nullptr)
	, m_fileStreamBuffer(nullptr)
{
	av_register_all();
}

void FFmpegSDK::ReadPackets(IRandomAccessStream^ stream, StorageFile ^file)
{
	HRESULT hr = CreateMediaStreamSource(stream);

	if (SUCCEEDED(hr))
	{
		auto workItem = ref new WorkItemHandler(
			[this, file](IAsyncAction^ workItem)
		{
			while (videoSampleProvider->ProcessNextSample() != false)
			{
				videoSampleProvider->writeFrameSideData(file);
				framesProcessed++;
			}
		});

		auto asyncAction = ThreadPool::RunAsync(workItem);
	}
}

FFmpegSDK^ FFmpegSDK::Initialize()
{
	auto sdk = ref new FFmpegSDK();
	return sdk;
}

HRESULT FFmpegSDK::CreateMediaStreamSource(IRandomAccessStream^ stream)
{
	HRESULT hr = S_OK;
	if (!stream)
	{
		hr = E_INVALIDARG;
	}

	if (SUCCEEDED(hr))
	{
		// Convert asynchronous IRandomAccessStream to synchronous IStream. This API requires shcore.h and shcore.lib
		hr = CreateStreamOverRandomAccessStream(reinterpret_cast<IUnknown*>(stream), IID_PPV_ARGS(&m_fileStreamData));
	}

	if (SUCCEEDED(hr))
	{
		m_fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
		if (m_fileStreamBuffer == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		avIOCtx = avio_alloc_context(m_fileStreamBuffer, FILESTREAMBUFFERSZ, 0, m_fileStreamData, FileStreamRead, 0, FileStreamSeek);
		if (avIOCtx == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		avFormatCtx = avformat_alloc_context();
		if (avFormatCtx == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		avFormatCtx->pb = avIOCtx;
		avFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

		// Open media file using custom IO setup above instead of using file name. Opening a file using file name will invoke fopen C API call that only have
		// access within the app installation directory and appdata folder. Custom IO allows access to file selected using FilePicker dialog.
		if (avformat_open_input(&avFormatCtx, "", NULL, NULL) < 0)
		{
			hr = E_FAIL; // Error opening file
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = InitFFmpegContext();
	}

	return hr;
}

HRESULT FFmpegSDK::CreateVideoStreamDescriptor()
{
	VideoEncodingProperties^ videoProperties;

	videoProperties = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Nv12, avVideoCodecCtx->width, avVideoCodecCtx->height);
	videoSampleProvider = ref new MediaSampleProvider(m_pReader, avFormatCtx, avVideoCodecCtx);

	// Detect the correct framerate
	if (avVideoCodecCtx->framerate.num != 0 || avVideoCodecCtx->framerate.den != 1)
	{
		videoProperties->FrameRate->Numerator = avVideoCodecCtx->framerate.num;
		videoProperties->FrameRate->Denominator = avVideoCodecCtx->framerate.den;
	}
	else if (avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.num != 0 || avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.den != 0)
	{
		videoProperties->FrameRate->Numerator = avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.num;
		videoProperties->FrameRate->Denominator = avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.den;
	}

	videoProperties->Bitrate = avVideoCodecCtx->bit_rate;
	videoStreamDescriptor = ref new VideoStreamDescriptor(videoProperties);

	return (videoStreamDescriptor != nullptr && videoSampleProvider != nullptr) ? S_OK : E_OUTOFMEMORY;
}

HRESULT FFmpegSDK::InitFFmpegContext()
{
	HRESULT hr = S_OK;

	if (SUCCEEDED(hr))
	{
		if (avformat_find_stream_info(avFormatCtx, NULL) < 0)
		{
			hr = E_FAIL; // Error finding info
		}
	}

	if (SUCCEEDED(hr))
	{
		m_pReader = ref new FFmpegReader(avFormatCtx);
		if (m_pReader == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}


	if (SUCCEEDED(hr))
	{
		// Find the video stream and its decoder
		AVCodec* avVideoCodec = nullptr;
		videoStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &avVideoCodec, 0);
		if (videoStreamIndex != AVERROR_STREAM_NOT_FOUND && avVideoCodec)
		{
			// FFmpeg identifies album/cover art from a music file as a video stream
			// Avoid creating unnecessarily video stream from this album/cover art
			if (avFormatCtx->streams[videoStreamIndex]->disposition == AV_DISPOSITION_ATTACHED_PIC)
			{
				videoStreamIndex = AVERROR_STREAM_NOT_FOUND;
				avVideoCodec = nullptr;
			}
			else
			{
				AVDictionary *opts = NULL;
				av_dict_set(&opts, "flags2", "+export_mvs", 0);

				avVideoCodecCtx = avFormatCtx->streams[videoStreamIndex]->codec;
				if (avcodec_open2(avVideoCodecCtx, avVideoCodec, &opts) < 0)
				{
					avVideoCodecCtx = nullptr;
					hr = E_FAIL; // Cannot open the video codec
				}
				else
				{
					// Detect video format and create video stream descriptor accordingly
					hr = CreateVideoStreamDescriptor();
					if (SUCCEEDED(hr))
					{
						hr = videoSampleProvider->AllocateResources();
						if (SUCCEEDED(hr))
						{
							m_pReader->SetVideoStream(videoStreamIndex, videoSampleProvider);
						}

					}
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		// Convert media duration from AV_TIME_BASE to TimeSpan unit
		mediaDuration = { LONGLONG(avFormatCtx->duration * 10000000 / double(AV_TIME_BASE)) };
	}
	return hr;
}

static int FileStreamRead(void* ptr, uint8_t* buf, int bufSize)
{
	IStream* pStream = reinterpret_cast<IStream*>(ptr);
	ULONG bytesRead = 0;
	HRESULT hr = pStream->Read(buf, bufSize, &bytesRead);

	if (FAILED(hr))
	{
		return -1;
	}

	// If we succeed but don't have any bytes, assume end of file
	if (bytesRead == 0)
	{
		return AVERROR_EOF;  // Let FFmpeg know that we have reached eof
	}

	return bytesRead;
}

// Static function to seek in file stream. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
static int64_t FileStreamSeek(void* ptr, int64_t pos, int whence)
{
	IStream* pStream = reinterpret_cast<IStream*>(ptr);
	LARGE_INTEGER in;
	in.QuadPart = pos;
	ULARGE_INTEGER out = { 0 };

	if (FAILED(pStream->Seek(in, whence, &out)))
	{
		return -1;
	}

	return out.QuadPart; // Return the new position:
}
