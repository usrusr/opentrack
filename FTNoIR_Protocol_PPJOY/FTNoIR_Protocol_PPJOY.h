/********************************************************************************
* FaceTrackNoIR		This program is a private project of some enthusiastic		*
*					gamers from Holland, who don't like to pay much for			*
*					head-tracking.												*
*																				*
* Copyright (C) 2010	Wim Vriend (Developing)									*
*						Ron Hendriks (Researching and Testing)					*
*																				*
* Homepage																		*
*																				*
* This program is free software; you can redistribute it and/or modify it		*
* under the terms of the GNU General Public License as published by the			*
* Free Software Foundation; either version 3 of the License, or (at your		*
* option) any later version.													*
*																				*
* This program is distributed in the hope that it will be useful, but			*
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY	*
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for	*
* more details.																	*
*																				*
* You should have received a copy of the GNU General Public License along		*
* with this program; if not, see <http://www.gnu.org/licenses/>.				*
*																				*
* PPJoyServer		PPJoyServer is the Class, that communicates headpose-data	*
*					to the Virtual Joystick, created by Deon van der Westhuysen.*
********************************************************************************/
#pragma once
#ifndef INCLUDED_PPJOYSERVER_H
#define INCLUDED_PPJOYSERVER_H

#include "..\ftnoir_protocol_base\ftnoir_protocol_base.h"
#include "ui_FTNoIR_PPJOYcontrols.h"
#include "PPJIoctl.h"
#include <QMessageBox>
#include <QSettings>
#include <QLibrary>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include "Windows.h"
//#include "math.h"

typedef void (WINAPI *importSetPosition)(float x, float y, float z, float xRot, float yRot, float zRot);
typedef void (WINAPI *importTIRViewsStart)(void);
typedef void (WINAPI *importTIRViewsStop)(void);

#define	NUM_ANALOG	7		/* Number of analog values which we will provide */
#define	NUM_DIGITAL	1		/* Number of digital values which we will provide */

#pragma pack(push,1)		/* All fields in structure must be byte aligned. */
typedef struct
{
 unsigned long	Signature;				/* Signature to identify packet to PPJoy IOCTL */
 char			NumAnalog;				/* Num of analog values we pass */
 long			Analog[NUM_ANALOG];		/* Analog values */
 char			NumDigital;				/* Num of digital values we pass */
 char			Digital[NUM_DIGITAL];	/* Digital values */
}	JOYSTICK_STATE;
#pragma pack(pop)


class FTNoIR_Protocol : public IProtocol
{
public:
	FTNoIR_Protocol();
	~FTNoIR_Protocol();

	void Release();
    void Initialize();

	bool checkServerInstallationOK( HANDLE handle );
	void sendHeadposeToGame( THeadPoseData *headpose, THeadPoseData *rawheadpose );
	void getNameFromGame( char *dest );						// Take care dest can handle up to 100 chars...

private:
	HANDLE h;
	JOYSTICK_STATE JoyState;
	DWORD RetSize;
	DWORD rc;
	long *Analog;
	char *Digital;
	int selectedPPJoy;										// Number of virtual joystick (1..16)

//	static long analogDefault,PPJoyCorrection;
	long centerPos[3],centerRot[3];

	void checkAnalogLimits();
	long scale2AnalogLimits( float x, float min_x, float max_x );
	void loadSettings();

};

// Widget that has controls for FTNoIR protocol client-settings.
class PPJOYControls: public QWidget, Ui::UICPPJOYControls, public IProtocolDialog
{
    Q_OBJECT
public:

	explicit PPJOYControls();
    virtual ~PPJOYControls();
	void showEvent ( QShowEvent * event );

	void Release();											// Member functions which are accessible from outside the DLL
    void Initialize(QWidget *parent);
	void registerProtocol(IProtocol *protocol) {
		theProtocol = (FTNoIR_Protocol *) protocol;			// Accept the pointer to the Protocol
	};
	void unRegisterProtocol() {
		theProtocol = NULL;									// Reset the pointer
	};

private:
	Ui::UICPPJOYControls ui;
	void loadSettings();
	void save();

	/** helper **/
	bool settingsDirty;
	FTNoIR_Protocol *theProtocol;

private slots:
	void doOK();
	void doCancel();
	void virtualJoystickSelected( int index );
	void settingChanged() { settingsDirty = true; };
};

//*******************************************************************************************************
// FaceTrackNoIR Protocol DLL. Functions used to get general info on the Protocol
//*******************************************************************************************************
class FTNoIR_ProtocolDll : public IProtocolDll
{
public:
	FTNoIR_ProtocolDll();
	~FTNoIR_ProtocolDll();

	void getFullName(QString *strToBeFilled) { *strToBeFilled = QString("PPJoy Virtual Joystick"); };
	void getShortName(QString *strToBeFilled) { *strToBeFilled = QString("PPJoy"); };
	void getDescription(QString *strToBeFilled) { *strToBeFilled = QString("PPJoy virtual joystick protocol"); };

	void getIcon(QIcon *icon) { *icon = QIcon(":/images/PPJoy.ico"); };
};


#endif//INCLUDED_PPJOYSERVER_H
//END
