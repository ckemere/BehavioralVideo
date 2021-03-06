﻿#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "SerialCameraController.h"
#include "Threads.h"
#include "dummycameracontroller.h"
#include "VideoGLWidget.h"
#include "VideoWriter.h"
#include "CameraInterfaces/PtGrey.h"
#include "CameraInterfaces/SupportedCamera.h"
#include "CameraInterfaces/FakeCamera.h"


#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QMenuBar>
#include <QErrorMessage>
#include <QToolBar>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QTabWidget>

#include <QtUiTools/QtUiTools>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    layout = new QGridLayout(ui->centralWidget);

    widgetx = 0;
    widgety = 1;
    numCameras = 0;
    numCamerasReadyToWrite = 0;
    numCamerasInitialized = 0;
    numCamerasCapturing = 0;
    controllerInitialized = false;
    triggerType = NO_SELECTION;

    // Build Controller Menu
    connect(ui->actionDummyController, SIGNAL(triggered()), this,SLOT(openDummyController()));
    if (isSerialControllerConnected()) {
        ui->actionSerialController->setEnabled(true);
        ui->actionSerialController->setChecked(false);
        ui->actionSerialController->setEnabled(true);

        connect(ui->actionSerialController, SIGNAL(triggered()),this,SLOT(openSerialController()));
    }

    connect(ui->actionFakeCamera,SIGNAL(triggered()),this,SLOT(openFakeVideo()));

    cameraMapper = new QSignalMapper(this);

    //detect Point Grey cameras and add to menu
    QStringList *cameraNameList = new QStringList;
    FindPointGreyCameras(cameraNameList);
    qDebug() << "Number of cameras found " << cameraNameList->size();
    if (cameraNameList->size() > 0) {
        ui->menuOpenCamera->addSection("PointGrey Cameras");
        for (int x=0; x < cameraNameList->size(); x++)
        {
            QAction *action = new QAction("Point Grey " + cameraNameList->at(x),this);
            ui->menuOpenCamera->addAction(action);
            connect(action, SIGNAL(triggered()),cameraMapper,SLOT(map()));
            cameraMapper->setMapping(action,cameraNameList->at(x).toUInt());
        }
        connect(cameraMapper,SIGNAL(mapped(int)),this,SLOT(openPGCamera(int)));
    }
    else {
        ui->menuOpenCamera->addSection("No PointGrey Cameras Found");
    }

    cameraNameList->clear();
    FindSupportedCameras(cameraNameList);
    qDebug() << "Number of v4l2 cameras found " << cameraNameList->size();
    if (cameraNameList->size() > 0) {
        ui->menuOpenCamera->addSection("V4L2 Cameras");
        cameraMapper = new QSignalMapper(this);
        for (int x=0; x < cameraNameList->size(); x++)
        {
            QAction *action = new QAction(cameraNameList->at(x),this);
            ui->menuOpenCamera->addAction(action);
            connect(action, SIGNAL(triggered()),cameraMapper,SLOT(map()));
            cameraMapper->setMapping(action,cameraNameList->at(x));
        }
        connect(cameraMapper,SIGNAL(mapped(QString)),this,SLOT(openV4L2Camera(QString)));
    }
    else {
        ui->menuOpenCamera->addSection("No PointGrey Cameras Found");
    }
    ui->centralWidget->setLayout(layout);


    QDialog *preferencesWindow = new QDialog();
    QDialogButtonBox *prefButtons = new QDialogButtonBox(QDialogButtonBox::Ok, preferencesWindow);
    connect(prefButtons, SIGNAL(accepted()), preferencesWindow, SLOT(accept()));
    prefTabWidget = new QTabWidget(preferencesWindow);
    QWidget *generalPrefs = new QWidget(preferencesWindow);
    prefTabWidget->addTab(generalPrefs,"General");
    QVBoxLayout *prefVBox = new QVBoxLayout(preferencesWindow);
    prefVBox->addWidget(prefTabWidget,1);
    prefVBox->addWidget(prefButtons);

    connect(ui->actionPreferences, SIGNAL(triggered()), preferencesWindow, SLOT(show()));

    VideoWriter *writer = new VideoWriter();
    QThread *thread = new QThread();
    connect(this, SIGNAL(destroyed()), thread, SLOT(deleteLater()));
    thread->setObjectName("Behavioral Video Video Compression");
    thread->start();
    writer->moveToThread(thread);
    writer->addPreferencesPanel(prefTabWidget);

    dataController = new DataController(writer,0);
    dataController->moveToThread(&dataControllerThread);

    connect(ui->actionOpenVideoFile, SIGNAL(triggered()), this, SLOT(openVideoFile()));
    connect(this, SIGNAL(initializeVideoWriting(QString)), dataController, SLOT(initializeVideoWriting(QString)));
    qRegisterMetaType<SavingState>("SavingState");
    connect(dataController, SIGNAL(updateSavingMenus(SavingState)), this, SLOT(updateVideoMenus(SavingState)));
    connect(ui->actionRecord, SIGNAL(triggered()), dataController, SLOT(startVideoRecording()));
    connect(ui->actionStop, SIGNAL(triggered()), dataController, SLOT(stopVideoWriting()));

    savingState = NOT_SAVING;
    cameraState = ASYNC;

    VideoGLWidget *videoWidget = new VideoGLWidget();
    videoWidget->setSurfaceType(QSurface::OpenGLSurface);
    videoWidget->create();
    qRegisterMetaType<VideoGLWidget*>("VideoGLWidget*");
    QMetaObject::invokeMethod(dataController, "registerVideoWidget", Qt::QueuedConnection,
                              Q_ARG(VideoGLWidget*, videoWidget));

    videoContainer = new QWidget(this);
    layout->addWidget(videoContainer,0,0);
    videoContainer->setStyleSheet("QWidget {background: light gray}");
    QWidget *container = QWidget::createWindowContainer(videoWidget,ui->centralWidget);
    QToolBar *toolBar = new QToolBar(videoContainer);
    toolBar->addAction(ui->actionPreferences);
    toolBar->addAction(ui->actionOpenVideoFile);
    toolBar->addAction(ui->actionRecord);
    toolBar->addAction(ui->actionStop);
    toolBar->setAllowedAreas(Qt::TopToolBarArea);
    QGridLayout *vContainerLayout = new QGridLayout(videoContainer);
    vContainerLayout->addWidget(toolBar);
    vContainerLayout->setSpacing(0);
    vContainerLayout->addWidget(container);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::openVideoFile()
{
    bool fileSelected = false;
    QString filename;
    QDateTime curtime = QDateTime::currentDateTime();

    QString defaultFilename = QDir::currentPath() + "/BehaviorVideo_" + curtime.toString("MMddyyyy_hhmmss");

    while (!fileSelected) {
        filename = QFileDialog::getSaveFileName(this, tr("Select Video Filename"),
                                                defaultFilename, tr("Video File (*.mp4)"), 0,
                                                   QFileDialog::DontUseNativeDialog);

        if (filename == NULL)
            break;
        if (QFile(filename).exists()) {
            QMessageBox msgBox;
            msgBox.setText("File exists.");
            msgBox.setInformativeText("Do you want to overwrite?");
            msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
            msgBox.setDefaultButton(QMessageBox::No);
            int ret = msgBox.exec();
            if (ret == QMessageBox::Yes) {
                fileSelected = true;
            }
        }
        else
            fileSelected = true;
    }

    if (fileSelected)
        emit initializeVideoWriting(filename);
}


void MainWindow::updateVideoMenus(SavingState state)
{
    qDebug() << "updating menus " << savingState;
    switch (state) {
        case READY_TO_WRITE:
            qDebug() << "enablevideosaving";
            ui->actionRecord->setEnabled(true);
            ui->actionOpenVideoFile->setDisabled(true);
            videoContainer->setStyleSheet("QWidget {background: red}");
            savingState = READY_TO_WRITE;
            break;

        case CURRENTLY_WRITING:
            ui->actionRecord->setDisabled(true);
            ui->actionStop->setEnabled(true);
            videoContainer->setStyleSheet("QWidget {background: green}");
            if (controllerInitialized)
                ui->actionOpenVideoFile->setDisabled(true);
            savingState = CURRENTLY_WRITING;
            break;

        case NOT_SAVING:
            ui->actionRecord->setDisabled(true);
            ui->actionStop->setDisabled(true);
            videoContainer->setStyleSheet("QWidget {background: light gray}");
            if (controllerInitialized)
                ui->actionOpenVideoFile->setEnabled(true);
            qDebug() << "menus reset";
            break;
    }
}

void MainWindow::resetSavingMenus()
{
    ui->actionRecord->setDisabled(true);
    ui->actionStop->setDisabled(true);
    if (controllerInitialized)
        ui->actionOpenVideoFile->setEnabled(true);
    qDebug() << "menus reset";
}

void MainWindow::openSerialController()
{
    qDebug()<< "Initializing Controller";
    controller = new SerialCameraController;
    if (((SerialCameraController *)controller)->connect(name) != 0)
    {
        qDebug() << "Error opening controller";
        ui->actionSerialController->setDisabled(true);
        return;
    }

    triggerType = EXTERNAL_CAMERA_TRIGGER;
    openController();
    ui->actionDummyController->setChecked(true);
}

void MainWindow::openDummyController()
{
    controller = new DummyCameraController;
    triggerType = NO_CAMERA_TRIGGER;
    openController();
    ui->actionDummyController->setChecked(true);
}

void MainWindow::openController()
{
    controllerInitialized = true;
    qRegisterMetaType<GenericCameraController*>("GenericCameraController*");

    QMetaObject::invokeMethod(dataController, "registerCameraController", Qt::QueuedConnection,
                              Q_ARG(GenericCameraController*, controller));
    qRegisterMetaType<TriggerType>("TriggerType");
    QMetaObject::invokeMethod(dataController, "useTriggering", Qt::QueuedConnection, Q_ARG(TriggerType, triggerType));
    ui->actionOpenVideoFile->setEnabled(true);
}

void MainWindow::openPGCamera(int serialNumber)
{
    if (!controllerInitialized) {
        QMessageBox::warning(this, "Controller Not Selected", "No Controller Selected");
        return;
    }

    PtGreyInterface* pgCamera = new PtGreyInterface();
    pgCamera->serialNumber = serialNumber;
    openCamera(pgCamera);
    ((QAction *)cameraMapper->mapping(serialNumber))->setDisabled(true);
}

void MainWindow::openV4L2Camera(QString device)
{
    if (!controllerInitialized) {
        QMessageBox::warning(this, "Controller Not Selected", "No Controller Selected");
        return;
    }

    SupportedCamera *v4l2Camera = new SupportedCamera(device,prefTabWidget,0);
    openCamera(v4l2Camera);
    ((QAction *)cameraMapper->mapping(device))->setDisabled(true);

}

void MainWindow::openFakeVideo()
{
    if (!controllerInitialized) {
        QMessageBox::warning(this, "Controller Not Selected", "No Controller Selected");
        return;
    }

    FakeVideoGenerator *fakeCamera = new FakeVideoGenerator();
    fakeCamera->cameraName = "Test " + QString::number(numCameras);
    openCamera(fakeCamera);
}

void MainWindow::openCamera(GenericCameraInterface *camera)
{
    QMetaObject::invokeMethod(dataController, "stopVideo", Qt::QueuedConnection);

    QThread *thread = new QThread();
    connect(this, SIGNAL(destroyed()), thread, SLOT(deleteLater()));
    thread->setObjectName("Behavioral Video Camera [" + camera->cameraName + "]");
    thread->start();
    camera->moveToThread(thread);

    qRegisterMetaType<GenericCameraInterface*>("GenericCameraInterface*");
    QMetaObject::invokeMethod(dataController, "registerCamera",
                              Qt::QueuedConnection, Q_ARG(GenericCameraInterface*, camera));

    QMetaObject::invokeMethod(camera, "Initialize", Qt::QueuedConnection);
    numCameras++;

    QMetaObject::invokeMethod(dataController, "startVideoViewing", Qt::QueuedConnection);
}


