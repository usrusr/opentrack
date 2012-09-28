/********************************************************************************
* FaceTrackNoIR		This program is a private project of the some enthusiastic	*
*					gamers from Holland, who don't like to pay much for			*
*					head-tracking.												*
*																				*
* Copyright (C) 2010-2011	Wim Vriend (Developing)								*
*							Ron Hendriks (Researching and Testing)				*
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
* FTNoIR_Protocol_Mouse	The Class, that communicates headpose-data by			*
*						generating Mouse commands.								*
*						Many games (like FPS's) support Mouse-look features,	*
*						but no face-tracking.									*
********************************************************************************/
/*
	Modifications (last one on top):
	20110401 - WVR: Moved protocol to a DLL, convenient for installation etc.
	20101224 - WVR: Base class is no longer inheriting QThread. sendHeadposeToGame
					is called from run() of Tracker.cpp
*/
#include "ftnoir_protocol_mouse.h"

/** constructor **/
FTNoIR_Protocol_MOUSE::FTNoIR_Protocol_MOUSE()
{
	prev_fMouse_X = 0.0f;
	prev_fMouse_Y = 0.0f;
	prev_fMouse_Wheel = 0.0f;
	frame_delay = 0;

	loadSettings();
}

/** destructor **/
FTNoIR_Protocol_MOUSE::~FTNoIR_Protocol_MOUSE()
{
}

/** helper to Auto-destruct **/
void FTNoIR_Protocol_MOUSE::Release()
{
    delete this;
}

void FTNoIR_Protocol_MOUSE::Initialize()
{
int ScreenX, ScreenY;

	ScreenX = GetSystemMetrics(SM_CXSCREEN);
	ScreenY = GetSystemMetrics(SM_CYSCREEN);

	qDebug() << "Initialize(): Screen width (x) = " << ScreenX << ", height (y) = " << ScreenY;

	return;
}

//
// Scale the measured value to the Joystick values
//
long FTNoIR_Protocol_MOUSE::scale2AnalogLimits( float x, float min_x, float max_x ) {
double y;

	y = ((MOUSE_AXIS_MAX - MOUSE_AXIS_MIN)/(max_x - min_x)) * x + ((MOUSE_AXIS_MAX - MOUSE_AXIS_MIN)/2) + MOUSE_AXIS_MIN;
	return (long) y;
}

//
// Load the current Settings from the currently 'active' INI-file.
//
void FTNoIR_Protocol_MOUSE::loadSettings() {
	QSettings settings("Abbequerque Inc.", "FaceTrackNoIR");	// Registry settings (in HK_USER)

	QString currentFile = settings.value ( "SettingsFile", QCoreApplication::applicationDirPath() + "/Settings/default.ini" ).toString();
	QSettings iniFile( currentFile, QSettings::IniFormat );		// Application settings (in INI-file)

	iniFile.beginGroup ( "Mouse" );
	Mouse_Style = (FTN_MouseStyle) (iniFile.value ( "Style", 1 ).toInt() - 1);
	Mouse_X = (FTN_AngleName) (iniFile.value ( "Mouse_X", 1 ).toInt() - 1);
	Mouse_Y = (FTN_AngleName) (iniFile.value ( "Mouse_Y", 1 ).toInt() - 1);
	Mouse_Wheel = (FTN_AngleName) (iniFile.value ( "Mouse_Wheel", 1 ).toInt() - 1);

	mouse_X_factor = iniFile.value("SensX", 10).toFloat() / 10.0f;
	mouse_Y_factor = iniFile.value("SensY", 10).toFloat() / 10.0f;
	mouse_Wheel_factor = iniFile.value("SensWheel", 10).toFloat() / 10.0f;

	useVirtualDesk = iniFile.value ( "useVirtualDesk", 0 ).toBool();

	iniFile.endGroup ();
}

//
// Update Headpose in Game.
//
void FTNoIR_Protocol_MOUSE::sendHeadposeToGame( THeadPoseData *headpose, THeadPoseData *rawheadpose ) {
float fMouse_X;							// The actual value
float fMouse_Y;
float fMouse_Wheel;


	//
	// Determine which of the 6DOF's is used.
	// The rotations are from -180 to +180 and the translations from -50cm to +50cm.
	// Let's scale the translations to the degrees for simplicity sake...
	//
	switch (Mouse_X) {
		case FTN_PITCH:
			fMouse_X = headpose->pitch;
			break;

		case FTN_YAW:
			fMouse_X = headpose->yaw;
			break;

		case FTN_ROLL:
			fMouse_X = headpose->roll;
			break;

		case FTN_X:
			fMouse_X = headpose->x * 3.0f;
			break;

		case FTN_Y:
			fMouse_X = headpose->y * 3.0f;
			break;

		case FTN_Z:
			fMouse_X = headpose->z * 3.0f;
			break;

		default:
			break;
	}

	//
	// Determine which of the 6DOF's is used.
	// The rotations are from -180 to +180 and the translations from -50cm to +50cm.
	// Let's scale the translations to the degrees for simplicity sake...
	//
	switch (Mouse_Y) {
		case FTN_PITCH:
			fMouse_Y = headpose->pitch;
			break;

		case FTN_YAW:
			fMouse_Y = headpose->yaw;
			break;

		case FTN_ROLL:
			fMouse_Y = headpose->roll;
			break;

		case FTN_X:
			fMouse_Y = headpose->x * 3.0f;
			break;

		case FTN_Y:
			fMouse_Y = headpose->y * 3.0f;
			break;

		case FTN_Z:
			fMouse_Y = headpose->z * 3.0f;
			break;

		default:
			break;
	}

	//
	// Determine which of the 6DOF's is used.
	// The rotations are from -180 to +180 and the translations from -50cm to +50cm.
	// Let's scale the translations to the degrees for simplicity sake...
	//
	switch (Mouse_Wheel) {
		case FTN_PITCH:
			fMouse_Wheel = headpose->pitch;
			break;

		case FTN_YAW:
			fMouse_Wheel = headpose->yaw;
			break;

		case FTN_ROLL:
			fMouse_Wheel = headpose->roll;
			break;

		case FTN_X:
			fMouse_Wheel = headpose->x * 3.0f;
			break;

		case FTN_Y:
			fMouse_Wheel = headpose->y * 3.0f;
			break;

		case FTN_Z:
			fMouse_Wheel = headpose->z * 3.0f;
			break;

		default:
			break;
	}

	//
	// Determine which style is used.
	//
	SecureZeroMemory(&MouseStruct, sizeof(MouseStruct));
	MouseStruct.type = INPUT_MOUSE;
	switch (Mouse_Style) {
		case FTN_ABSOLUTE:
			MouseStruct.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_WHEEL | MOUSEEVENTF_ABSOLUTE;
			if (useVirtualDesk) {
				MouseStruct.mi.dwFlags |= MOUSEEVENTF_VIRTUALDESK;
			}
			MouseStruct.mi.dx = scale2AnalogLimits(-1.0f * fMouse_X * mouse_X_factor, -150, 150);
			MouseStruct.mi.dy = scale2AnalogLimits(fMouse_Y * mouse_Y_factor, -150, 150);
			MouseStruct.mi.mouseData = mouse_Wheel_factor * (fMouse_Wheel - prev_fMouse_Wheel);

			frame_delay = 9999;					// Seems no problem with Absolute positioning
			break;

		case FTN_RELATIVE:
			MouseStruct.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_WHEEL;
			MouseStruct.mi.dx = -1.0f * mouse_X_factor * (fMouse_X - prev_fMouse_X);
			MouseStruct.mi.dy = mouse_Y_factor * (fMouse_Y - prev_fMouse_Y);
			MouseStruct.mi.mouseData = - 1.0f * mouse_Wheel_factor * (fMouse_Wheel - prev_fMouse_Wheel);

			frame_delay += 1;					// Add 1 to the counter
			qDebug() << "sendHeadposeToGame(): FTN_RELATIVE x = " << MouseStruct.mi.dx << ", y = " << MouseStruct.mi.dy;
			break;

		default:
			Mouse_Style = FTN_ABSOLUTE;			// Force to a valid value...
			break;
	}

	//
	// Only send Input, when it has changed.
	// This releases the Mouse, when tracking is stopped (for a while).
	//
	if (frame_delay > 10) {
		if ((prev_fMouse_X != fMouse_X) || (prev_fMouse_Y != fMouse_Y) || (prev_fMouse_Wheel != fMouse_Wheel)) {
			SendInput(1, &MouseStruct, sizeof(MouseStruct));
		}

		prev_fMouse_X = fMouse_X;
		prev_fMouse_Y = fMouse_Y;
		prev_fMouse_Wheel = fMouse_Wheel;
	}
}

//
// Returns 'true' if all seems OK.
//
bool FTNoIR_Protocol_MOUSE::checkServerInstallationOK( HANDLE handle )
{   

	return true;
}

//
// Return a name, if present the name from the Game, that is connected...
//
void FTNoIR_Protocol_MOUSE::getNameFromGame( char *dest )
{   
	sprintf_s(dest, 99, "Mouse");
	return;
}

////////////////////////////////////////////////////////////////////////////////
// Factory function that creates instances if the Protocol object.

// Export both decorated and undecorated names.
//   GetProtocol     - Undecorated name, which can be easily used with GetProcAddress
//                Win32 API function.
//   _GetProtocol@0  - Common name decoration for __stdcall functions in C language.
#pragma comment(linker, "/export:GetProtocol=_GetProtocol@0")

FTNOIR_PROTOCOL_BASE_EXPORT IProtocolPtr __stdcall GetProtocol()
{
	return new FTNoIR_Protocol_MOUSE;
}
