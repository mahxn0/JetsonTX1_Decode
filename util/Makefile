#*************************************************************************
#Copyright:
#
#Auther:mahxn0
#Date:  2017-10-26
#Version:多路解码的动态链接库
#Description:Demo
#
#************************************************************************     
Gstreamer=1
GPU=1
QT=1
CC=g++
NCC=nvcc
src=cuda/cudaFont.cu cuda/cudaNormalize.cu cuda/cudaOverlay.cu cuda/cudaResize.cu cuda/cudaRGB.cu cuda/cudaYUV-NV12.cu cuda/cudaYUV-YUYV.cu cuda/cudaYUV-YV12.cu
obj=cudaFont.o cudaNormalize.o cudaOverlay.o cudaResize.o cudaRGB.o cudaYUV-NV12.o cudaYUV-YUYV.o cudaYUV-YV12.o
IncPath=../build/aarch64/include
ifeq ($(Gstreamer),1)
LDFLAGS+= `pkg-config --libs gstreamer-1.0`
CFLAGS+= `pkg-config --cflags gstreamer-1.0`
endif
ifeq ($(GPU),1)
LDFLAGS+=-I/usr/local/cuda/include  
CFLAGS+=-L/usr/local/cuda/lib64 -lcuda -lcudart -lcublas -lcurand  -lgstapp-1.0
endif
ifeq ($(QT),1)
LDFLAGS+=-I /usr/share/qt4/include/QtGui
endif
LDFLAGS+=-I /usr/share/qt4/include
#loadImage.o:loadImage.cpp loadImage.h
#	${CC} ${LDFLAGS} -I${IncPath} -c loadImage.cpp ${CFLAGS}
#	@echo "loadImage.o done 显示编译.o文件"
#cudaFont.o:cuda/cudaFont.cu cuda/cudaFont.h
#	${NCC} ${LDFLAGS} -I${IncPath} -c cuda/cudaFont.cu ${CFLAGS}
#	@echo "cudaFont.o done 显示编译.o文件"
#cudaNormalize.o:cuda/cudaNormalize.cu cuda/cudaNormalize.h
#	${NCC} ${LDFLAGS} -I${IncPath} -c cuda/cudaNormalize.cu ${CFLAGS}
#	@echo "cudaNormalize.o done 显示编译.o文件"
#cudaOverlay.o:cuda/cudaOverlay.cu cuda/cudaOverlay.h
#	${NCC} ${LDFLAGS} -I${IncPath} -c cuda/cudaOverlay.cu ${CFLAGS}
#	@echo "cudaOverlay.o done 显示编译.o文件"
#cudaResize.o:cuda/cudaResize.cu cuda/cudaResize.h
#	${NCC} ${LDFLAGS} -I${IncPath} -c cuda/cudaResize.cu ${CFLAGS}
#	@echo "cudaResize.o done 显示编译.o文件"
#cudaRGB.o:cuda/cudaRGB.cu cuda/cudaRGB.h
#	${NCC} ${LDFLAGS} -I${IncPath} -c cuda/cudaRGB.cu ${CFLAGS}
#	@echo "C} ${LDFLAGS} -I${IncPath} -c cuda/cudaYUV-YUYV.cu ${CFLAGS} 
#        @echo "cudaRGB.o done 显示编译.o文件"
#cudaYUV-YUYV.o:cuda/cudaYUV-YUYV.cu cuda/cudaYUV.h
#	${NCC} ${LDFLAGS} -I${IncPath} -c cuda/cudaYUV-YUYV.cu ${CFLAGS} 
#	@echo "cudaYUV-YUYV.o done 显示编译.o文件"
#cudaYUV-NV12.o:cuda/cudaYUV-NV12.cu cuda/cudaYUV.h
#	${NCC} ${LDFLAGS} -I${IncPath} -c cuda/cudaYUV-NV12.cu ${CFLAGS} 
#	@echo "cudaYUV-NV12.o done 显示编译.o文件"
#cudaYUV-YV12.o:cuda/cudaYUV-YV12.cu cuda/cudaYUV.h
#	${NCC} ${LDFLAGS} -I${IncPath} -c cuda/cudaYUV-YV12.cu ${CFLAGS} 
#	@echo "cudaYUV-YV12.o done 显示编译.o文件"
#glTexture.o:display/glTexture.cpp display/glTexture.h
#	${NCC} ${LDFLAGS} -I${IncPath} -fPIC -c display/glTexture.cpp ${CFLAGS}
#	@echo "glTexture.o is done 显示编译.o文件"
#glDisplay.o:display/glDisplay.cpp display/glDisplay.h
#	${CC} ${LDFLAGS} -I${IncPath}  -fPIC -c display/glDisplay.cpp ${CFLAGS}
#	@echo "glDisplay.o is done 显示编译.o文件"
#gstUtility.o:camera/gstUtility.cpp camera/gstUtility.h
#	${CC} ${LDFLAGS} -I${IncPath} -fPIC -c camera/gstUtility.cpp ${CFLAGS}
#	@echo "glUtility.o is done 显示编译.o文件"
#gstCamera.o:camera/gstCamera.cpp camera/gstCamera.h
#	${CC} ${LDFLAGS} -I${IncPath}  -I /usr/include/qt4/QtCore -fPIC -c camera/gstCamera.cpp ${CFLAGS} -std=c++11
#	@echo "gstCamera.o is done 显示编译.o文件"
#libtx1det.so:
#	${CC} -shared -o libtx1det.so gstCamera.o gstUtility.o glDisplay.o glTexture.o cudaYUV-NV12.o cudaYUV-YV12.o cudaYUV-YUYV.o cudaRGB.o cudaResize.o cudaOverlay.o cudaNormalize.o cudaFont.o loadImage.o
clean:
	rm *.o
	@echo "CLEAN *o Finished!!!"
