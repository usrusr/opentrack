#include "ftnoir_tracker_joystick.h"
#include "facetracknoir/global-settings.h"

TrackerControls::TrackerControls() :
QWidget()
{
	ui.setupUi( this );

	// Connect Qt signals to member-functions
	connect(ui.btnOK, SIGNAL(clicked()), this, SLOT(doOK()));
	connect(ui.btnCancel, SIGNAL(clicked()), this, SLOT(doCancel()));

	connect(ui.chkEnableRoll, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.chkEnablePitch, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.chkEnableYaw, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.chkEnableX, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.chkEnableY, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	connect(ui.chkEnableZ, SIGNAL(stateChanged(int)), this, SLOT(settingChanged(int)));
	loadSettings();
}

//
// Destructor for server-dialog
//
TrackerControls::~TrackerControls() {
}

void TrackerControls::Initialize(QWidget *parent) {
	QPoint offsetpos(100, 100);
	if (parent) {
		this->move(parent->pos() + offsetpos);
	}
	show();
}

void TrackerControls::doOK() {
	save();
	this->close();
}

void TrackerControls::showEvent ( QShowEvent * ) {
	loadSettings();
}

void TrackerControls::doCancel() {
	if (settingsDirty) {
		int ret = QMessageBox::question ( this, "Settings have changed", "Do you want to save the settings?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Discard );

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

void TrackerControls::loadSettings() {

	QSettings settings("opentrack");	// Registry settings (in HK_USER)

	QString currentFile = settings.value ( "SettingsFile", QCoreApplication::applicationDirPath() + "/settings/default.ini" ).toString();
	QSettings iniFile( currentFile, QSettings::IniFormat );		// Application settings (in INI-file)

    iniFile.beginGroup ( "tracker-joy" );
	ui.chkEnableRoll->setChecked(iniFile.value ( "EnableRoll", 1 ).toBool());
	ui.chkEnablePitch->setChecked(iniFile.value ( "EnablePitch", 1 ).toBool());
	ui.chkEnableYaw->setChecked(iniFile.value ( "EnableYaw", 1 ).toBool());
	ui.chkEnableX->setChecked(iniFile.value ( "EnableX", 1 ).toBool());
	ui.chkEnableY->setChecked(iniFile.value ( "EnableY", 1 ).toBool());
	ui.chkEnableZ->setChecked(iniFile.value ( "EnableZ", 1 ).toBool());
	iniFile.endGroup ();

	settingsDirty = false;
}

void TrackerControls::save() {
	QSettings settings("opentrack");	// Registry settings (in HK_USER)

	QString currentFile = settings.value ( "SettingsFile", QCoreApplication::applicationDirPath() + "/settings/default.ini" ).toString();
	QSettings iniFile( currentFile, QSettings::IniFormat );		// Application settings (in INI-file)

    iniFile.beginGroup ( "tracker-joy" );
	iniFile.setValue ( "EnableRoll", ui.chkEnableRoll->isChecked() );
	iniFile.setValue ( "EnablePitch", ui.chkEnablePitch->isChecked() );
	iniFile.setValue ( "EnableYaw", ui.chkEnableYaw->isChecked() );
	iniFile.setValue ( "EnableX", ui.chkEnableX->isChecked() );
	iniFile.setValue ( "EnableY", ui.chkEnableY->isChecked() );
	iniFile.setValue ( "EnableZ", ui.chkEnableZ->isChecked() );
	iniFile.endGroup ();

	settingsDirty = false;
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