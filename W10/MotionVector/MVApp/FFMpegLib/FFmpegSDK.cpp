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
	: m_avIOCtx(nullptr)
	, m_avFormatCtx(nullptr)
	, m_avVideoCodecCtx(nullptr)
	, m_videoStreamIndex(AVERROR_STREAM_NOT_FOUND)
	, m_fileStreamData(nullptr)
	, m_fileStreamBuffer(nullptr)
{
	av_register_all();
}

void FFmpegSDK::ReadMotionFrames(IRandomAccessStream^ stream, StorageFile ^file)
{
	HRESULT hr = CreateMediaStreamSource(stream);

	if (SUCCEEDED(hr))
	{
		auto workItem = ref new WorkItemHandler(
			[this, file](IAsyncAction^ workItem)
		{
			while (m_videoSampleProvider->ProcessNextSample() != false)
			{
				m_videoSampleProvider->writeFrameSideData(file);
				m_framesProcessed++;
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
		m_avIOCtx = avio_alloc_context(m_fileStreamBuffer, FILESTREAMBUFFERSZ, 0, m_fileStreamData, FileStreamRead, 0, FileStreamSeek);
		if (m_avIOCtx == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		m_avFormatCtx = avformat_alloc_context();
		if (m_avFormatCtx == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		m_avFormatCtx->pb = m_avIOCtx;
		m_avFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

		// Open media file using custom IO setup above instead of using file name. Opening a file using file name will invoke fopen C API call that only have
		// access within the app installation directory and appdata folder. Custom IO allows access to file selected using FilePicker dialog.
		if (avformat_open_input(&m_avFormatCtx, "", NULL, NULL) < 0)
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

	videoProperties = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Nv12, m_avVideoCodecCtx->width, m_avVideoCodecCtx->height);
	m_videoSampleProvider = ref new MediaSampleProvider(m_pReader, m_avFormatCtx, m_avVideoCodecCtx);

	// Detect the correct framerate
	if (m_avVideoCodecCtx->framerate.num != 0 || m_avVideoCodecCtx->framerate.den != 1)
	{
		videoProperties->FrameRate->Numerator = m_avVideoCodecCtx->framerate.num;
		videoProperties->FrameRate->Denominator = m_avVideoCodecCtx->framerate.den;
	}
	else if (m_avFormatCtx->streams[m_videoStreamIndex]->avg_frame_rate.num != 0 || m_avFormatCtx->streams[m_videoStreamIndex]->avg_frame_rate.den != 0)
	{
		videoProperties->FrameRate->Numerator = m_avFormatCtx->streams[m_videoStreamIndex]->avg_frame_rate.num;
		videoProperties->FrameRate->Denominator = m_avFormatCtx->streams[m_videoStreamIndex]->avg_frame_rate.den;
	}

	videoProperties->Bitrate = m_avVideoCodecCtx->bit_rate;
	m_videoStreamDescriptor = ref new VideoStreamDescriptor(videoProperties);

	return (m_videoStreamDescriptor != nullptr && m_videoSampleProvider != nullptr) ? S_OK : E_OUTOFMEMORY;
}

HRESULT FFmpegSDK::InitFFmpegContext()
{
	HRESULT hr = S_OK;

	if (SUCCEEDED(hr))
	{
		if (avformat_find_stream_info(m_avFormatCtx, NULL) < 0)
		{
			hr = E_FAIL; // Error finding info
		}
	}

	if (SUCCEEDED(hr))
	{
		m_pReader = ref new FFmpegReader(m_avFormatCtx);
		if (m_pReader == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}


	if (SUCCEEDED(hr))
	{
		// Find the video stream and its decoder
		AVCodec* avVideoCodec = nullptr;
		m_videoStreamIndex = av_find_best_stream(m_avFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &avVideoCodec, 0);
		if (m_videoStreamIndex != AVERROR_STREAM_NOT_FOUND && avVideoCodec)
		{
			// FFmpeg identifies album/cover art from a music file as a video stream
			// Avoid creating unnecessarily video stream from this album/cover art
			if (m_avFormatCtx->streams[m_videoStreamIndex]->disposition == AV_DISPOSITION_ATTACHED_PIC)
			{
				m_videoStreamIndex = AVERROR_STREAM_NOT_FOUND;
				avVideoCodec = nullptr;
			}
			else
			{
				//Tells codec to export motion frames
				AVDictionary *opts = NULL;
				av_dict_set(&opts, "flags2", "+export_mvs", 0);

				m_avVideoCodecCtx = m_avFormatCtx->streams[m_videoStreamIndex]->codec;
				if (avcodec_open2(m_avVideoCodecCtx, avVideoCodec, &opts) < 0)
				{
					m_avVideoCodecCtx = nullptr;
					hr = E_FAIL; // Cannot open the video codec
				}
				else
				{
					// Detect video format and create video stream descriptor accordingly
					hr = CreateVideoStreamDescriptor();
					if (SUCCEEDED(hr))
					{
						hr = m_videoSampleProvider->AllocateResources();
						if (SUCCEEDED(hr))
						{
							m_pReader->SetVideoStream(m_videoStreamIndex, m_videoSampleProvider);
						}

					}
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		// Convert media duration from AV_TIME_BASE to TimeSpan unit
		m_mediaDuration = { LONGLONG(m_avFormatCtx->duration * 10000000 / double(AV_TIME_BASE)) };
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
