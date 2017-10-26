/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __GSTREAMER_CAMERA_H__
#define __GSTREAMER_CAMERA_H__

#include <gst/gst.h>
#include <string>


struct _GstAppSink;//声明结构体和类
class QWaitCondition;
class QMutex;
/*** gstreamer CSI camera using nvcamerasrc (or optionally v4l2src)
 * @ingroup util
 */
class gstCamera
{
public:
	// 创建camera类
	static gstCamera* Create( int v4l2_device=-1 );	// use onboard camera by default (>=0 for V4L2)
	static gstCamera* Create( uint32_t width, uint32_t height, int v4l2_device=-1 );
	
	// 析构函数
	~gstCamera();

	// 开始和停止流
	bool Open();
	void Close();
	
	// 采集YUV(NV12格式)
	bool Capture( void** cpu, void** cuda, unsigned long timeout=ULONG_MAX );
	
	// 抓取YUV-NV12 CUDA image, 转换成 float4 RGBA (像素范围在 0-255)
	// 转换如果在CPU上进行，设置zeroCopy=true,默认只在CUDA上.
	bool ConvertRGBA( void* input, void** output, bool zeroCopy=false );
	
	// 图像大小信息 inline(内联函数，适合简单的函数)
	inline uint32_t GetWidth() const	  { return mWidth; }
	inline uint32_t GetHeight() const	  { return mHeight; }
	inline uint32_t GetPixelDepth() const { return mDepth; }
	inline uint32_t GetSize() const		  { return mSize; }
	
	// 默认图像大小，可以在create时改变
	static const uint32_t DefaultWidth  = 1280;
	static const uint32_t DefaultHeight = 720;
	
private:
	static void onEOS(_GstAppSink* sink, void* user_data);
	static GstFlowReturn onPreroll(_GstAppSink* sink, void* user_data);//GstFlowReturn 传递流
	static GstFlowReturn onBuffer(_GstAppSink* sink, void* user_data);

	gstCamera();
	
	bool init();
	bool buildLaunchStr();
	void checkMsgBus();
	void checkBuffer();
	//GstBus
	_GstBus*     mBus;//GstBus 异步同步消息
	_GstAppSink* mAppSink;
	_GstElement* mPipeline;

	std::string  mLaunchStr="rtspsrc location=rtsp://admin:buaa123456@192.168.1.106:554/h264/ch1/main/av_stream  latency=0 ! queue ! rtph264depay ! h264parse ! queue ! omxh264dec ! appsink name=mysink";
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
	uint32_t mSize;
	
	static const uint32_t NUM_RINGBUFFERS = 16;//环形队列来解决数据阻塞问题
	
	void* mRingbufferCPU[NUM_RINGBUFFERS];
	void* mRingbufferGPU[NUM_RINGBUFFERS];
	
	QWaitCondition* mWaitEvent;
	//mutex.lock() //锁住互斥量（mutex）。如果互斥量是解锁的，那么当前线程就立即占用并锁定它。否则，当前线程就会被阻塞，知道掌握这个互斥量的线程对它解锁为止。
	//mutex.unlock()//解锁
	//mutex.tryLock()//尝试解锁，如果该互斥量已经锁住，它就会立即返回
	QMutex* mWaitMutex;
	QMutex* mRingMutex;
	
	uint32_t mLatestRGBA;
	uint32_t mLatestRingbuffer;
	bool     mLatestRetrieved;
	
	void* mRGBA[NUM_RINGBUFFERS];
	int   mV4L2Device;	// -1 for onboard, >=0 for V4L2 device
	
	inline bool onboardCamera() const		{ return (mV4L2Device < 0); }
};

#endif
