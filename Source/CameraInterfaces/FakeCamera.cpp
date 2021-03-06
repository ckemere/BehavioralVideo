#include <QDebug>
#include <QVideoFrame>
#include "FakeCamera.h"

FakeVideoGenerator::FakeVideoGenerator(QObject *parent) :
    GenericCameraInterface(parent)
{
    frameTimer = new QTimer(this);
    QObject::connect(frameTimer, SIGNAL(timeout()), this, SLOT(GenerateNextFrame()));
}

void FakeVideoGenerator::Initialize()
{
    width = 640;
    height = 480;
    currentFrame = new QVideoFrame(width * height * 3, QSize(width,height), width, QVideoFrame::Format_RGB24);
    //currentFrame = new QImage(width, height, QImage::Format_RGB888);
    if (!currentFrame)
        qDebug() << "QImage not allocated";
    currentFrame_YUV = avcodec_alloc_frame();
    if (!currentFrame_YUV)
        qDebug() << "Could not allocate frame";
    int num_bytes = avpicture_get_size(PIX_FMT_YUV420P,width,height);
    currentFrame_YUV_buf = av_malloc(num_bytes);
    avpicture_fill((AVPicture*)currentFrame_YUV, (uint8_t*)currentFrame_YUV_buf, PIX_FMT_YUV420P, width, height);

    sws_ctx = sws_getContext(width, height, PIX_FMT_YUV420P, width, height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
    currentFrame_RGB = avcodec_alloc_frame();
    if (!currentFrame_YUV)
        qDebug() << "Could not allocate frame";
    if (!currentFrame->map(QAbstractVideoBuffer::WriteOnly))
        qDebug() << "Failed to map current frame";
    avpicture_fill((AVPicture*)currentFrame_RGB, currentFrame->bits(), PIX_FMT_RGB24, width, height);
    currentFrame->unmap();

    frameIdx = 0;

    isInitialized = true;

}

FakeVideoGenerator::~FakeVideoGenerator(void) {
    av_free(currentFrame_YUV->data[0]);
    av_free(currentFrame_YUV);
    av_free(currentFrame_RGB);
}

void FakeVideoGenerator::GenerateNextFrame(void) {
    int x,y;
    int i = frameIdx;
    // Y
    for(y=0;y<height;y++) {
        for(x=0;x<width;x++) {
            currentFrame_YUV->data[0][y * currentFrame_YUV->linesize[0] + x] = x + y + i * 3;
        }
    }

    // Cb and Cr
    for(y=0;y<height/2;y++) {
        for(x=0;x<width/2;x++) {
            currentFrame_YUV->data[1][y * currentFrame_YUV->linesize[1] + x] = 128 + y + i * 2;
            currentFrame_YUV->data[2][y * currentFrame_YUV->linesize[2] + x] = 64 + x + i * 5;
        }
    }

  //  qDebug() << "Doing color conversion";
    if (!currentFrame->map(QAbstractVideoBuffer::WriteOnly))
        qDebug() << "Failed to map current frame";
    sws_scale(sws_ctx, currentFrame_YUV->data, currentFrame_YUV->linesize, 0, height,
              currentFrame_RGB->data, currentFrame_RGB->linesize);

    // should add a timestamp here?

    emit newFrame(*currentFrame);
    currentFrame->unmap();
    frameIdx++;
}

void FakeVideoGenerator::StartCapture(bool enableStrobe)
{
    qDebug() << "Fake camera start. Camera " << cameraName;
    frameTimer->start(30);
    emit capturingStarted();
}

void FakeVideoGenerator::StopCapture()
{
    qDebug() << "Fake camera stop. Camera " << cameraName;
    frameTimer->stop();
    emit capturingEnded();
}

