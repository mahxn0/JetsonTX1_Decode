#include "gstCamera.h"
#include "gstUtility.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <sstream> 
#include <unistd.h>
#include <string.h>

#include <QMutex>
#include <QWaitCondition>

#include "cudaMappedMemory.h"
#include "cudaYUV.h"
#include "cudaRGB.h"

//#include "tensorNet.h"


// 构造函数
gstCamera::gstCamera()
{	
	mAppSink    = NULL;
	mBus        = NULL;
	mPipeline   = NULL;	
	mV4L2Device = -1;
	
	mWidth  = 0;
	mHeight = 0;
	mDepth  = 0;
	mSize   = 0;
	
	mWaitEvent  = new QWaitCondition();
	mWaitMutex  = new QMutex();
	mRingMutex  = new QMutex();
	
	mLatestRGBA       = 0;
	mLatestRingbuffer = 0;
	mLatestRetrieved  = false;
	
	for( uint32_t n=0; n < NUM_RINGBUFFERS; n++ )
	{
		mRingbufferCPU[n] = NULL;
		mRingbufferGPU[n] = NULL;
		mRGBA[n]          = NULL;
	}
}


// 析构函数	
gstCamera::~gstCamera()
{
	
}


// 转换RGBA
bool gstCamera::ConvertRGBA( void* input, void** output, bool zeroCopy )
{
	if( !input || !output )
		return false;
	
	if( !mRGBA[0] )
	{
		const size_t size = mWidth * mHeight * sizeof(float4);

		for( uint32_t n=0; n < NUM_RINGBUFFERS; n++ )
		{
			if( zeroCopy )
			{
				void* cpuPtr = NULL;
				void* gpuPtr = NULL;

				if( !cudaAllocMapped(&cpuPtr, &gpuPtr, size) )
				{
					printf(LOG_CUDA "gstCamera -- failed to allocate zeroCopy memory for %ux%xu RGBA texture\n", mWidth, mHeight);
					return false;
				}

				if( cpuPtr != gpuPtr )
				{
					printf(LOG_CUDA "gstCamera -- zeroCopy memory has different pointers, please use a UVA-compatible GPU\n");
					return false;
				}

				mRGBA[n] = gpuPtr;
			}
			else
			{
				if( CUDA_FAILED(cudaMalloc(&mRGBA[n], size)) )
				{
					printf(LOG_CUDA "gstCamera -- failed to allocate memory for %ux%u RGBA texture\n", mWidth, mHeight);
					return false;
				}
			}
		}
		
		printf(LOG_CUDA "gstreamer camera -- allocated %u RGBA ringbuffers\n", NUM_RINGBUFFERS);
	}
	
	if( onboardCamera() )
	{
		// onboard camera is NV12
		if( CUDA_FAILED(cudaNV12ToRGBAf((uint8_t*)input, (float4*)mRGBA[mLatestRGBA], mWidth, mHeight)) )
			return false;
	}
	else
	{
		// USB webcam is RGB
		if( CUDA_FAILED(cudaRGBToRGBAf((uchar3*)input, (float4*)mRGBA[mLatestRGBA], mWidth, mHeight)) )
			return false;
	}
	
	*output     = mRGBA[mLatestRGBA];
	mLatestRGBA = (mLatestRGBA + 1) % NUM_RINGBUFFERS;
	return true;
}


// onEOS
void gstCamera::onEOS(_GstAppSink* sink, void* user_data)
{
	printf(LOG_GSTREAMER "gstreamer decoder onEOS\n");
}


// onPreroll
GstFlowReturn gstCamera::onPreroll(_GstAppSink* sink, void* user_data)
{
	printf(LOG_GSTREAMER "gstreamer decoder onPreroll\n");
	return GST_FLOW_OK;
}


// onBuffer
GstFlowReturn gstCamera::onBuffer(_GstAppSink* sink, void* user_data)
{
	//printf(LOG_GSTREAMER "gstreamer decoder onBuffer\n");
	
	if( !user_data )
		return GST_FLOW_OK;
		
	gstCamera* dec = (gstCamera*)user_data;
	
	dec->checkBuffer();
	dec->checkMsgBus();
	return GST_FLOW_OK;
}
	

// Capture
bool gstCamera::Capture( void** cpu, void** cuda, unsigned long timeout )
{
	/*wait() 函数必须传入一个已上锁的 mutex 对象，在 wait() 执行过程中，
	mutex一直保持上锁状态，直到调用操作系统的wait_block 在阻塞的一瞬间把 mutex 解锁
	（严格说来应该是原子操作，即系统能保证在真正执行阻塞等待指令时才解锁）。
	另一线程唤醒后，wait() 函数将在第一时间重新给 mutex 上锁（这种操作也是原子的）
	，直到显示调用 mutex.unlock() 解锁。*/
	mWaitMutex->lock();
    const bool wait_result = mWaitEvent->wait(mWaitMutex, timeout);
    mWaitMutex->unlock();
	
	if( !wait_result )
		return false;
	
	mRingMutex->lock();
	const uint32_t latest = mLatestRingbuffer;
	const bool retrieved = mLatestRetrieved;
	mLatestRetrieved = true;
	mRingMutex->unlock();
	
	// skip if it was already retrieved
	if( retrieved )
		return false;
	
	if( cpu != NULL )
		*cpu = mRingbufferCPU[latest];
	
	if( cuda != NULL )
		*cuda = mRingbufferGPU[latest];
	
	return true;
}


#define release_return { gst_sample_unref(gstSample); return; }


// checkBuffer
void gstCamera::checkBuffer()
{
	bool write_flags=true;//默认写数据
	if( !mAppSink )
		return;

	// block waiting for the buffer 函数被唤醒until A sample or EOS 可用 或者appsink 被设置成 ready/null state
	GstSample* gstSample = gst_app_sink_pull_sample(mAppSink);
	
	if( !gstSample )
	{
		printf(LOG_GSTREAMER "gstreamer camera -- gst_app_sink_pull_sample() returned NULL...\n");
		return;
	}
	//get buffer from gstSample
	GstBuffer* gstBuffer = gst_sample_get_buffer(gstSample);
	
	if( !gstBuffer )
	{
		printf(LOG_GSTREAMER "gstreamer camera -- gst_sample_get_buffer() returned NULL...\n");
		return;
	}
	
	// retrieve
	GstMapInfo map; 

	if(	!gst_buffer_map(gstBuffer, &map, GST_MAP_READ) ) 
	{
		printf(LOG_GSTREAMER "gstreamer camera -- gst_buffer_map() failed...\n");
		return;
	}
	
	//gst_util_dump_mem(map.data, map.size); 

	void* gstData = map.data; //GST_BUFFER_DATA(gstBuffer);
	const uint32_t gstSize = map.size; //GST_BUFFER_SIZE(gstBuffer);
	
	if( !gstData )
	{
		printf(LOG_GSTREAMER "gstreamer camera -- gst_buffer had NULL data pointer...\n");
		release_return;
	}
	
	// 取出caps
	GstCaps* gstCaps = gst_sample_get_caps(gstSample);
	
	if( !gstCaps )
	{
		printf(LOG_GSTREAMER "gstreamer camera -- gst_buffer had NULL caps...\n");
		release_return;
	}
	
	GstStructure* gstCapsStruct = gst_caps_get_structure(gstCaps, 0);
	
	if( !gstCapsStruct )
	{
		printf(LOG_GSTREAMER "gstreamer camera -- gst_caps had NULL structure...\n");
		release_return;
	}
	
	// get width & height of the buffer
	int width  = 0;
	int height = 0;
	
	if( !gst_structure_get_int(gstCapsStruct, "width", &width) ||
		!gst_structure_get_int(gstCapsStruct, "height", &height) )
	{
		printf(LOG_GSTREAMER "gstreamer camera -- gst_caps missing width/height...\n");
		release_return;
	}
	
	if( width < 1 || height < 1 )
		release_return;
	
	mWidth  = width;
	mHeight = height;
	mDepth  = (gstSize * 8) / (width * height);
	mSize   = gstSize;
	
	//printf(LOG_GSTREAMER "gstreamer camera recieved %ix%i frame (%u bytes, %u bpp)\n", width, height, gstSize, mDepth);
	
	// make sure ringbuffer is allocated
	if( !mRingbufferCPU[0] )
	{
		for( uint32_t n=0; n < NUM_RINGBUFFERS; n++ )
		{
			if( !cudaAllocMapped(&mRingbufferCPU[n], &mRingbufferGPU[n], gstSize) )
				printf(LOG_CUDA "gstreamer camera -- failed to allocate ringbuffer %u  (size=%u)\n", n, gstSize);
		}
		
		printf(LOG_CUDA "gstreamer camera -- allocated %u ringbuffers, %u bytes each\n", NUM_RINGBUFFERS, gstSize);
	}
	
	// copy to next ringbuffer
	const uint32_t nextRingbuffer = (mLatestRingbuffer + 1) % NUM_RINGBUFFERS;		
	
	//printf(LOG_GSTREAMER "gstreamer camera -- using ringbuffer #%u for next frame\n", nextRingbuffer);
	memcpy(mRingbufferCPU[nextRingbuffer], gstData, gstSize);
	// FILE *fp=fopen("out.yuv","w+");
	// fwrite(gstData,gstSize,1,fp);
	// fclose(fp);
	//test h264 write
	//void *writedata=map.data;
	// while(write_flags==true)
	// {
	// 	FILE *fp=fopen("out.264","a+");
	// 	fwrite(writedata,gstSize,1,fp);
	// 	write_flags=false;
	// 	fclose(fp);
	// }
	gst_buffer_unmap(gstBuffer, &map); 
	//gst_buffer_unref(gstBuffer);
	gst_sample_unref(gstSample);
	
	
	// update and signal sleeping threads
	mRingMutex->lock();
	mLatestRingbuffer = nextRingbuffer;
	mLatestRetrieved  = false;
	mRingMutex->unlock();
	mWaitEvent->wakeAll();
}



// buildLaunchStr
bool gstCamera::buildLaunchStr()
{
	// gst-launch-1.0 nvcamerasrc fpsRange="30.0 30.0" ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, format=(string)I420, framerate=(fraction)30/1' ! \
	// nvvidconv flip-method=2 ! 'video/x-raw(memory:NVMM), format=(string)I420' ! fakesink silent=false -v
	std::ostringstream ss;
	
//#define CAPS_STR "video/x-raw(memory:NVMM), width=(int)2592, height=(int)1944, format=(string)I420, framerate=(fraction)30/1"
//#define CAPS_STR "video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, format=(string)I420, framerate=(fraction)30/1"

	if( onboardCamera() )
	{
	#if NV_TENSORRT_MAJOR > 1	// if JetPack 3.1 (different flip-method)
		const int flipMethod = 0;
	#else
		const int flipMethod = 2;
	#endif
	
		/*ss << "nvcamerasrc fpsRange=\"30.0 30.0\" ! video/x-raw(memory:NVMM), width=(int)" << mWidth << ", height=(int)" << mHeight << ", format=(string)NV12 ! nvvidconv flip-method=" << flipMethod << " ! "; //'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, format=(string)I420, framerate=(fraction)30/1' ! ";
		ss << "video/x-raw ! appsink name=mysink";*/
		// ss << "rtspsrc location=rtsp://admin:buaa123456@192.168.1.106:554/h264/ch1/main/av_stream"<<" latency=0" << " ! ";
        //         ss << "queue ! rtph264depay ! h264parse ! queue ! omxh264dec ! appsink name=mysink";
	}
	else
	{
		/*ss << "v4l2src device=/dev/video" << mV4L2Device << " ! ";
		ss << "video/x-raw, width=(int)" << mWidth << ", height=(int)" << mHeight << ", "; 
		ss << "format=RGB ! videoconvert ! video/x-raw, format=RGB ! videoconvert !";
		ss << "appsink name=mysink";*/
		//rtspsrc location=rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov ! queue ! rtph264depay ! h264parse ! queue ! omxh264dec ! appsink name=mysink
		ss << "rtspsrc location=rtsp://admin:admin666@192.168.1.64:554/h264/ch1/main/av_stream"<<" latency=0" << " ! ";
		ss << "queue ! rtph264depay ! h264parsse ! queue ! omxh264dec ! appsink name=mysink";  
	}
	//mLaunchStr = ss.str();
	//mLaunchStr="rtspsrc location=rtsp://admin:buaa123456@192.168.1.106:554/h264/ch1/main/av_stream  latency=0 ! queue ! rtph264depay ! h264parse ! queue ! omxh264dec ! appsink name=mysink";
	printf(LOG_GSTREAMER "gstreamer decoder pipeline string:\n");
	printf("%s\n", mLaunchStr.c_str());
	return true;
}


// Create
gstCamera* gstCamera::Create( uint32_t width, uint32_t height, int v4l2_device )
{
	if( !gstreamerInit() )
	{
		printf(LOG_GSTREAMER "failed to initialize gstreamer API\n");
		return NULL;
	}
	
	gstCamera* cam = new gstCamera();
	
	if( !cam )
		return NULL;
	
	cam->mV4L2Device = v4l2_device;
	cam->mWidth      = width;
	cam->mHeight     = height;
	cam->mDepth      = cam->onboardCamera() ? 12 : 24;	// NV12 or RGB
	cam->mSize       = (width * height * cam->mDepth) / 8;

	if( !cam->init() )
	{
		printf(LOG_GSTREAMER "failed to init gstCamera\n");
		return NULL;
	}
	
	return cam;
}


// Create
gstCamera* gstCamera::Create( int v4l2_device )
{
	return Create( DefaultWidth, DefaultHeight, v4l2_device );
}


// init
bool gstCamera::init()
{
	GError* err = NULL;

	// build pipeline string
	if( !buildLaunchStr() )
	{
		printf(LOG_GSTREAMER "gstreamer decoder failed to build pipeline string\n");
		return false;
	}

	// launch pipeline
	mPipeline = gst_parse_launch(mLaunchStr.c_str(), &err);

	if( err != NULL )
	{
		printf(LOG_GSTREAMER "gstreamer decoder failed to create pipeline\n");
		printf(LOG_GSTREAMER "   (%s)\n", err->message);
		g_error_free(err);
		return false;
	}         

	GstPipeline* pipeline = GST_PIPELINE(mPipeline);

	if( !pipeline )
	{
		printf(LOG_GSTREAMER "gstreamer failed to cast GstElement into GstPipeline\n");
		return false;
	}	

	// retrieve pipeline bus
	/*GstBus**/ mBus = gst_pipeline_get_bus(pipeline);

	if( !mBus )
	{
		printf(LOG_GSTREAMER "gstreamer failed to retrieve GstBus from pipeline\n");
		return false;
	}

	// add watch for messages (disabled when we poll the bus ourselves, instead of gmainloop)
	//gst_bus_add_watch(mBus, (GstBusFunc)gst_message_print, NULL);

	// get the appsrc
	GstElement* appsinkElement = gst_bin_get_by_name(GST_BIN(pipeline), "mysink");
	GstAppSink* appsink = GST_APP_SINK(appsinkElement);

	if( !appsinkElement || !appsink)
	{
		printf(LOG_GSTREAMER "gstreamer failed to retrieve AppSink element from pipeline\n");
		return false;
	}
	
	mAppSink = appsink;
	
	// setup callbacks
	GstAppSinkCallbacks cb;
	memset(&cb, 0, sizeof(GstAppSinkCallbacks));
	
	cb.eos         = onEOS;
	cb.new_preroll = onPreroll;
	cb.new_sample  = onBuffer;
	
	gst_app_sink_set_callbacks(mAppSink, &cb, (void*)this, NULL);
	
	return true;
}


// Open
bool gstCamera::Open()
{
	// transition pipline to STATE_PLAYING
	printf(LOG_GSTREAMER "gstreamer transitioning pipeline to GST_STATE_PLAYING\n");
	
	const GstStateChangeReturn result = gst_element_set_state(mPipeline, GST_STATE_PLAYING);

	if( result == GST_STATE_CHANGE_ASYNC )
	{
#if 0
		GstMessage* asyncMsg = gst_bus_timed_pop_filtered(mBus, 5 * GST_SECOND, 
    	 					      (GstMessageType)(GST_MESSAGE_ASYNC_DONE|GST_MESSAGE_ERROR)); 

		if( asyncMsg != NULL )
		{
			gst_message_print(mBus, asyncMsg, this);
			gst_message_unref(asyncMsg);
		}
		else
			printf(LOG_GSTREAMER "gstreamer NULL message after transitioning pipeline to PLAYING...\n");
#endif
	}
	else if( result != GST_STATE_CHANGE_SUCCESS )
	{
		printf(LOG_GSTREAMER "gstreamer failed to set pipeline state to PLAYING (error %u)\n", result);
		return false;
	}

	checkMsgBus();
	usleep(100*1000);
	checkMsgBus();

	return true;
}
	

// Close
void gstCamera::Close()
{
	// stop pipeline
	printf(LOG_GSTREAMER "gstreamer transitioning pipeline to GST_STATE_NULL\n");

	const GstStateChangeReturn result = gst_element_set_state(mPipeline, GST_STATE_NULL);

	if( result != GST_STATE_CHANGE_SUCCESS )
		printf(LOG_GSTREAMER "gstreamer failed to set pipeline state to PLAYING (error %u)\n", result);

	usleep(250*1000);
}


// checkMsgBus
void gstCamera::checkMsgBus()
{
	while(true)
	{
		GstMessage* msg = gst_bus_pop(mBus);

		if( !msg )
			break;

		gst_message_print(mBus, msg, this);
		gst_message_unref(msg);
	}
}
