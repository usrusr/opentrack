/* Copyright (c) 2013 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "ftnoir_tracker_base/ftnoir_tracker_base.h"
#include "ftnoir_tracker_aruco.h"
#include "ui_aruco-trackercontrols.h"
#include "facetracknoir/global-settings.h"
#include <cmath>
#include <QMutexLocker>
#include <aruco.h>
#include <opencv2/opencv.hpp>
#include <opencv/highgui.h>
#include <vector>
#include <cstdio>
#include <memory>

#if defined(_WIN32)
#   define NO_DSHOW_STRSAFE
#   include <dshow.h>
#else
#   include <unistd.h>
#endif

// delicious copypasta
static QList<QString> get_camera_names(void) {
    QList<QString> ret;
#if defined(_WIN32)
	// Create the System Device Enumerator.
	HRESULT hr;
	ICreateDevEnum *pSysDevEnum = NULL;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&pSysDevEnum);
	if (FAILED(hr))
	{
		return ret;
	}
	// Obtain a class enumerator for the video compressor category.
	IEnumMoniker *pEnumCat = NULL;
	hr = pSysDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumCat, 0);

	if (hr == S_OK) {
		// Enumerate the monikers.
		IMoniker *pMoniker = NULL;
		ULONG cFetched;
		while (pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK) {
			IPropertyBag *pPropBag;
			hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropBag);
			if (SUCCEEDED(hr))	{
				// To retrieve the filter's friendly name, do the following:
				VARIANT varName;
				VariantInit(&varName);
				hr = pPropBag->Read(L"FriendlyName", &varName, 0);
				if (SUCCEEDED(hr))
				{
					// Display the name in your UI somehow.
					QString str((QChar*)varName.bstrVal, wcslen(varName.bstrVal));
					ret.append(str);
				}
				VariantClear(&varName);

				////// To create an instance of the filter, do the following:
				////IBaseFilter *pFilter;
				////hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter,
				////	(void**)&pFilter);
				// Now add the filter to the graph. 
				//Remember to release pFilter later.
				pPropBag->Release();
			}
			pMoniker->Release();
		}
		pEnumCat->Release();
	}
	pSysDevEnum->Release();
#else
    for (int i = 0; i < 16; i++) {
        char buf[128];
        sprintf(buf, "/dev/video%d", i);
        if (access(buf, R_OK | W_OK) == 0) {
            ret.append(buf);
        } else {
            continue;
        }
    }
#endif
    return ret;
}

typedef struct {
	int width;
	int height;
} resolution_tuple;

static resolution_tuple resolution_choices[] = {
	{ 640, 480 },
	{ 320, 240 },
	{ 320, 200 },
	{ 0, 0 }
};

void Tracker::load_settings()
{
    QMutexLocker foo(&mtx);
	QSettings settings("opentrack");
	QString currentFile = settings.value( "SettingsFile", QCoreApplication::applicationDirPath() + "/settings/default.ini" ).toString();
	QSettings iniFile( currentFile, QSettings::IniFormat );

	iniFile.beginGroup( "aruco-Tracker" );
    fov = iniFile.value("fov", 56).toFloat();
    force_fps = iniFile.value("fps", 0).toInt();
    camera_index = iniFile.value("camera-index", -1).toInt();
    int res = iniFile.value("resolution", 0).toInt();
    if (res < 0 || res >= (int)(sizeof(resolution_choices) / sizeof(resolution_tuple)))
		res = 0;
	resolution_tuple r = resolution_choices[res];
	force_width = r.width;
    force_height = r.height;
	enableRX = iniFile.value("enable-rx", true).toBool();
	enableRY = iniFile.value("enable-ry", true).toBool();
	enableRZ = iniFile.value("enable-rz", true).toBool();
	enableTX = iniFile.value("enable-tx", true).toBool();
	enableTY = iniFile.value("enable-ty", true).toBool();
	enableTZ = iniFile.value("enable-tz", true).toBool();

    for (int i = 0; i < 3; i++)
    {
        headpos[i] = iniFile.value(QString("headpos-%1").arg(i), 0).toDouble();
    }
    headpitch = iniFile.value("pitch", 0).toDouble();

	iniFile.endGroup();
}

Tracker::Tracker()
{
    layout = nullptr;
    stop = false;
	videoWidget = NULL;
	enableRX = enableRY = enableRZ = enableTX = enableTY = enableTZ = true;
	load_settings();
}

Tracker::~Tracker()
{
    stop = true;
    wait();
	if (videoWidget)
		delete videoWidget;
    if(layout)
        delete layout;
    qDebug() << "releasing camera, brace for impact";
    camera.release();
    qDebug() << "all done!";
}

void Tracker::StartTracker(QFrame* videoframe)
{
    videoframe->show();
    videoWidget = new ArucoVideoWidget(videoframe);
    QHBoxLayout* layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(videoWidget);
    if (videoframe->layout())
        delete videoframe->layout();
    videoframe->setLayout(layout);
    videoWidget->show();
    load_settings();
    start();
    for (int i = 0; i < 6; i++)
        pose[i] = 0;
    this->layout = layout;
}

#define HT_PI 3.1415926535

void Tracker::run()
{
    camera = cv::VideoCapture(camera_index);  
    if (force_width)
        camera.set(CV_CAP_PROP_FRAME_WIDTH, force_width);
    if (force_height)
        camera.set(CV_CAP_PROP_FRAME_HEIGHT, force_height);
    if (force_fps)
        camera.set(CV_CAP_PROP_FPS, force_fps);
    
    aruco::MarkerDetector detector;
    detector.setDesiredSpeed(3);
    detector.setThresholdParams(11, 6);

    cv::Rect last_roi(65535, 65535, 0, 0);

    cv::Mat color, color_, grayscale, rvec, tvec;

    const double stateful_coeff = 0.81;
    
    if (!camera.isOpened())
    {
        fprintf(stderr, "aruco tracker: can't open camera\n");
        return;
    }

    while (!stop)
        if(camera.read(color_))
            break;

    auto freq = cv::getTickFrequency();
    auto last_time = cv::getTickCount();
    int fps = 0;
    int last_fps = 0;
    cv::Point2f last_centroid;
    while (!stop)
    {
        if (!camera.read(color_))
            continue;
        auto tm = cv::getTickCount();
        color_.copyTo(color);
        cv::cvtColor(color, grayscale, cv::COLOR_BGR2GRAY);

        const int scale = frame.cols > 480 ? 2 : 1;

        const float focal_length_w = 0.5 * grayscale.cols / tan(0.5 * fov * HT_PI / 180);
        const float focal_length_h = 0.5 * grayscale.rows / tan(0.5 * fov * grayscale.rows / grayscale.cols * HT_PI / 180.0);
        cv::Mat intrinsics = cv::Mat::eye(3, 3, CV_32FC1);
        intrinsics.at<float> (0, 0) = focal_length_w;
        intrinsics.at<float> (1, 1) = focal_length_h;
        intrinsics.at<float> (0, 2) = grayscale.cols/2;
        intrinsics.at<float> (1, 2) = grayscale.rows/2;

        cv::Mat dist_coeffs = cv::Mat::zeros(5, 1, CV_32FC1);
        
        for (int i = 0; i < 5; i++)
            dist_coeffs.at<float>(i) = 0;
        
        std::vector< aruco::Marker > markers;

        if (last_roi.width > 0 &&
                (detector.detect(grayscale(last_roi), markers, cv::Mat(), cv::Mat(), -1, false),
                 markers.size() == 1 && markers[0].size() == 4))
        {
            detector.setMinMaxSize(std::max(0.2, 0.08 * grayscale.cols / last_roi.width),
                                   std::min(1.0, 0.39 * grayscale.cols / last_roi.width));
            auto& m = markers.at(0);
            for (int i = 0; i < 4; i++)
            {
                auto& p = m.at(i);
                p.x += last_roi.x;
                p.y += last_roi.y;
            }
        }
        else
        {
            detector.setMinMaxSize(0.09, 0.4);
            detector.detect(grayscale, markers, cv::Mat(), cv::Mat(), -1, false);
        }

        if (markers.size() == 1 && markers[0].size() == 4) {
            const aruco::Marker& m = markers.at(0);
            for (int i = 0; i < 4; i++)
                cv::line(color, m[i], m[(i+1)%4], cv::Scalar(0, 0, 255), scale, 8);
        }

        auto time = cv::getTickCount();

        if ((long) (time / freq) != (long) (last_time / freq))
        {
            last_fps = fps;
            fps = 0;
            last_time = time;
        }

        fps++;

        char buf[128];

        frame = color.clone();

        ::sprintf(buf, "Hz: %d", last_fps);
        cv::putText(frame, buf, cv::Point(10, 32), cv::FONT_HERSHEY_PLAIN, scale, cv::Scalar(0, 255, 0), scale);
        ::sprintf(buf, "Jiffies: %ld", (long) (10000 * (time - tm) / freq));
        cv::putText(frame, buf, cv::Point(10, 54), cv::FONT_HERSHEY_PLAIN, scale, cv::Scalar(80, 255, 0), scale);
        
        if (markers.size() == 1 && markers[0].size() == 4) {
            const aruco::Marker& m = markers.at(0);
            const float size = 7;
            
            cv::Mat obj_points(4,3,CV_32FC1);
            obj_points.at<float>(1,0)=-size + headpos[0];
            obj_points.at<float>(1,1)=-size + headpos[1];
            obj_points.at<float>(1,2)=0 + headpos[2];
            obj_points.at<float>(2,0)=size + headpos[0];
            obj_points.at<float>(2,1)=-size + headpos[1];
            obj_points.at<float>(2,2)=0 + headpos[2];
            obj_points.at<float>(3,0)=size + headpos[0];
            obj_points.at<float>(3,1)=size + headpos[1];
            obj_points.at<float>(3,2)=0 + headpos[2];
            obj_points.at<float>(0,0)=-size + headpos[0];
            obj_points.at<float>(0,1)=size + headpos[1];
            obj_points.at<float>(0,2)=0 + headpos[2];

            last_roi = cv::Rect(65535, 65535, 0, 0);

            for (int i = 0; i < 4; i++)
            {
                auto foo = m.at(i);
                last_roi.x = std::min<int>(foo.x, last_roi.x);
                last_roi.y = std::min<int>(foo.y, last_roi.y);
                last_roi.width = std::max<int>(foo.x, last_roi.width);
                last_roi.height = std::max<int>(foo.y, last_roi.height);
            }
            {
                last_roi.width -= last_roi.x;
                last_roi.height -= last_roi.y;
                last_roi.x -= last_roi.width * stateful_coeff;
                last_roi.y -= last_roi.height * stateful_coeff;
                last_roi.width *= stateful_coeff * 3;
                last_roi.height *= stateful_coeff * 3;
                last_roi.x = std::max<int>(0, last_roi.x);
                last_roi.y = std::max<int>(0, last_roi.y);
                last_roi.width = std::min<int>(grayscale.cols - last_roi.x, last_roi.width);
                last_roi.height = std::min<int>(grayscale.rows - last_roi.y, last_roi.height);
            }
            
            cv::solvePnP(obj_points, m, intrinsics, dist_coeffs, rvec, tvec, false, cv::ITERATIVE);
            
            cv::Mat rotation_matrix = cv::Mat::zeros(3, 3, CV_64FC1);
            
            cv::Mat junk1(3, 3, CV_64FC1), junk2(3, 3, CV_64FC1);
        
            cv::Rodrigues(rvec, rotation_matrix);
        
            {
                const double beta = headpitch * HT_PI / 180;
                double pitch[] = {
                    1, 0, 0,
                    0, cos(beta), -sin(beta),
                    0, sin(beta), cos(beta)
                };
                cv::Mat rot(3, 3, CV_64F, pitch);
                cv::Mat tvec2 = rot * tvec;
                rotation_matrix = rot * rotation_matrix;

                cv::Vec3d euler = cv::RQDecomp3x3(rotation_matrix, junk1, junk2);

                QMutexLocker lck(&mtx);

                for (int i = 0; i < 3; i++)
                    pose[i] = tvec2.at<double>(i);

                pose[Yaw] = euler[1];
                pose[Pitch] = -euler[0];
                pose[Roll] = euler[2];
            }

            std::vector<cv::Point2f> repr2;
            std::vector<cv::Point3f> centroid;
            centroid.push_back(cv::Point3f(0, 0, 0));
            cv::projectPoints(centroid, rvec, tvec, intrinsics, dist_coeffs, repr2);

            {
                auto s = cv::Scalar(255, 0, 255);
                cv::circle(frame, repr2.at(0), 4, s, -1);
            }

            last_centroid = repr2[0];
        }
        else
        {
            last_roi = cv::Rect(65535, 65535, 0, 0);
        }

        if (frame.rows > 0)
            videoWidget->update_image(frame);
    }
}

bool Tracker::GiveHeadPoseData(double *data)
{
    QMutexLocker lck(&mtx);
    
    if (enableRX)
        data[Yaw] = pose[Yaw];
    if (enableRY)
        data[Pitch] = pose[Pitch];
    if (enableRZ)
        data[Roll] = pose[Roll];
    if (enableTX)
        data[TX] = pose[TX];
    if (enableTY)
        data[TY] = pose[TY];
    if (enableTZ)
        data[TZ] = pose[TZ];
    
	return true;
}

class TrackerDll : public Metadata
{
	// ITrackerDll interface
	void getFullName(QString *strToBeFilled);
	void getShortName(QString *strToBeFilled);
	void getDescription(QString *strToBeFilled);
	void getIcon(QIcon *icon);
};

//-----------------------------------------------------------------------------
void TrackerDll::getFullName(QString *strToBeFilled)
{
    *strToBeFilled = "aruco";
}

void TrackerDll::getShortName(QString *strToBeFilled)
{
	*strToBeFilled = "aruco";
}

void TrackerDll::getDescription(QString *strToBeFilled)
{
	*strToBeFilled = "";
}

void TrackerDll::getIcon(QIcon *icon)
{
    *icon = QIcon(":/images/aruco.png");
}


//-----------------------------------------------------------------------------
//#pragma comment(linker, "/export:GetTrackerDll=_GetTrackerDll@0")

extern "C" FTNOIR_TRACKER_BASE_EXPORT Metadata* CALLING_CONVENTION GetMetadata()
{
	return new TrackerDll;
}

//#pragma comment(linker, "/export:GetTracker=_GetTracker@0")

extern "C" FTNOIR_TRACKER_BASE_EXPORT ITracker* CALLING_CONVENTION GetConstructor()
{
    return new Tracker;
}

////////////////////////////////////////////////////////////////////////////////
// Factory function that creates instances if the Tracker-settings dialog object.

// Export both decorated and undecorated names.
//   GetTrackerDialog     - Undecorated name, which can be easily used with GetProcAddress
//                          Win32 API function.
//   _GetTrackerDialog@0  - Common name decoration for __stdcall functions in C language.
//#pragma comment(linker, "/export:GetTrackerDialog=_GetTrackerDialog@0")

extern "C" FTNOIR_TRACKER_BASE_EXPORT ITrackerDialog* CALLING_CONVENTION GetDialog( )
{
    return new TrackerControls;
}

TrackerControls::TrackerControls()
{
    tracker = nullptr;
	ui.setupUi(this);
    setAttribute(Qt::WA_NativeWindow, true);
	connect(ui.cameraName, SIGNAL(currentIndexChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.cameraFPS, SIGNAL(currentIndexChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.cameraFOV, SIGNAL(valueChanged(double)), this, SLOT(settingChanged(double)));
	connect(ui.rx, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.ry, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.rz, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.tx, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.ty, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.tz, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
    connect(ui.cx, SIGNAL(valueChanged(double)), this, SLOT(settingChanged(double)));
    connect(ui.cy, SIGNAL(valueChanged(double)), this, SLOT(settingChanged(double)));
    connect(ui.cz, SIGNAL(valueChanged(double)), this, SLOT(settingChanged(double)));
    //connect(ui.buttonCancel, SIGNAL(clicked()), this, SLOT(doCancel()));
    //connect(ui.buttonOK, SIGNAL(clicked()), this, SLOT(doOK()));
    //connect(ui.buttonSettings, SIGNAL(clicked()), this, SLOT(cameraSettings()));
    connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(doOK()));
    connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(doCancel()));
    loadSettings();
	settingsDirty = false;
}

TrackerControls::~TrackerControls()
{
}

void TrackerControls::showEvent(QShowEvent *)
{
}

void TrackerControls::Initialize(QWidget*)
{
    loadSettings();
	show();
}

void TrackerControls::loadSettings()
{
	ui.cameraName->clear();
	QList<QString> names = get_camera_names();
	names.prepend("Any available");
	ui.cameraName->addItems(names);
	QSettings settings("opentrack");
	QString currentFile = settings.value( "SettingsFile", QCoreApplication::applicationDirPath() + "/settings/default.ini" ).toString();
	QSettings iniFile( currentFile, QSettings::IniFormat );
	iniFile.beginGroup( "aruco-Tracker" );
	ui.cameraName->setCurrentIndex(iniFile.value("camera-index", -1).toInt() + 1);
    ui.cameraFOV->setValue(iniFile.value("fov", 56).toFloat());
	int fps;
	switch (iniFile.value("fps", 0).toInt())
	{
	default:
	case 0:
		fps = 0;
		break;
	case 30:
		fps = 1;
		break;
	case 60:
		fps = 2;
		break;
	case 120:
		fps = 3;
		break;
	}
	ui.cameraFPS->setCurrentIndex(fps);
	ui.rx->setCheckState(iniFile.value("enable-rx", true).toBool() ? Qt::Checked : Qt::Unchecked);
	ui.ry->setCheckState(iniFile.value("enable-ry", true).toBool() ? Qt::Checked : Qt::Unchecked);
	ui.rz->setCheckState(iniFile.value("enable-rz", true).toBool() ? Qt::Checked : Qt::Unchecked);
	ui.tx->setCheckState(iniFile.value("enable-tx", true).toBool() ? Qt::Checked : Qt::Unchecked);
	ui.ty->setCheckState(iniFile.value("enable-ty", true).toBool() ? Qt::Checked : Qt::Unchecked);
	ui.tz->setCheckState(iniFile.value("enable-tz", true).toBool() ? Qt::Checked : Qt::Unchecked);
    ui.resolution->setCurrentIndex(iniFile.value("resolution", 0).toInt());

    QDoubleSpinBox* headpos[] = {
        ui.cx,
        ui.cy,
        ui.cz
    };

    for (int i = 0; i < 3; i++)
    {
        headpos[i]->setValue(iniFile.value(QString("headpos-%1").arg(i)).toDouble());
    }

    ui.pitch_deg->setValue(iniFile.value("pitch", 0).toDouble());

	iniFile.endGroup();
	settingsDirty = false;
}

void TrackerControls::save()
{
	QSettings settings("opentrack");
	QString currentFile = settings.value( "SettingsFile", QCoreApplication::applicationDirPath() + "/settings/default.ini" ).toString();
	QSettings iniFile( currentFile, QSettings::IniFormat );

	iniFile.beginGroup( "aruco-Tracker" );
	iniFile.setValue("fov", ui.cameraFOV->value());
	int fps;
	switch (ui.cameraFPS->currentIndex())
	{
	case 0:
	default:
		fps = 0;
		break;
	case 1:
		fps = 30;
		break;
	case 2:
		fps = 60;
		break;
	case 3:
		fps = 120;
		break;
	}
	iniFile.setValue("fps", fps);
	iniFile.setValue("camera-index", ui.cameraName->currentIndex() - 1);
	iniFile.setValue("enable-rx", ui.rx->checkState() != Qt::Unchecked ? true : false);
	iniFile.setValue("enable-ry", ui.ry->checkState() != Qt::Unchecked ? true : false);
	iniFile.setValue("enable-rz", ui.rz->checkState() != Qt::Unchecked ? true : false);
	iniFile.setValue("enable-tx", ui.tx->checkState() != Qt::Unchecked ? true : false);
	iniFile.setValue("enable-ty", ui.ty->checkState() != Qt::Unchecked ? true : false);
	iniFile.setValue("enable-tz", ui.tz->checkState() != Qt::Unchecked ? true : false);
	iniFile.setValue("resolution", ui.resolution->currentIndex());
    iniFile.setValue("pitch", ui.pitch_deg->value());

    QDoubleSpinBox* headpos[] = {
        ui.cx,
        ui.cy,
        ui.cz
    };

    for (int i = 0; i < 3; i++)
    {
        iniFile.setValue(QString("headpos-%1").arg(i), headpos[i]->value());
    }
	iniFile.endGroup();
	settingsDirty = false;
    if (tracker)
        tracker->load_settings();
}

void TrackerControls::doOK()
{
	save();
	this->close();
}

void TrackerControls::doCancel()
{
	if (settingsDirty) {
		int ret = QMessageBox::question ( this,
										  "Settings have changed",
										  "Do you want to save the settings?",
										  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
										  QMessageBox::Discard );

		switch (ret) {
			case QMessageBox::Save:
				save();
				this->close();
				break;
			case QMessageBox::Discard:
				this->close();
				break;
			case QMessageBox::Cancel:
				// Cancel was clicked
				break;
			default:
				// should never be reached
			break;
		}
	}
	else {
		this->close();
	}
}
