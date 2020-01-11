#include "x2mount.h"

X2Mount::X2Mount(const char* pszDriverSelection,
				 const int& nInstanceIndex,
				 SerXInterface					* pSerX,
				 TheSkyXFacadeForDriversInterface	* pTheSkyX,
				 SleeperInterface					* pSleeper,
				 BasicIniUtilInterface			* pIniUtil,
				 LoggerInterface					* pLogger,
				 MutexInterface					* pIOMutex,
				 TickCountInterface				* pTickCount) : SkyW(pSerX, pSleeper, pTheSkyX)
{
	// Variables for HEQ5
	
	m_nPrivateMulitInstanceIndex	= nInstanceIndex;
	m_pSerX							= pSerX;
	m_pTheSkyXForMounts				= pTheSkyX;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;
		
	m_bSynced = false;
	m_bParked = false;


#ifdef  HEQ5_DEBUG
	// Open log file
#if defined(SB_WIN_BUILD)
	sprintf(m_sLogfilePath, "%s%s%s", getenv("HOMEDRIVE"), getenv("HOMEPATH"), "\\X2Mountlog.txt");
#else
	sprintf(m_sLogfilePath, "%s%s", getenv("HOME"), "/X2Mountlog.txt");
#endif
	LogFile = fopen(m_sLogfilePath, "w");

	setbuf(LogFile, NULL);
#endif

	// Set Slew Speeds and Names
	
	SlewSpeeds[0] = 0.5;		sprintf(SlewSpeedNames[0], "1/2 Sidereal");
	SlewSpeeds[1] = 1.0;		sprintf(SlewSpeedNames[1], "Sidereal");
	SlewSpeeds[2] = 16.0;		sprintf(SlewSpeedNames[2], "16x Sidereal");
	SlewSpeeds[3] = 64.0;		sprintf(SlewSpeedNames[3], "64x Sidereal");
	SlewSpeeds[4] = 128.0;		sprintf(SlewSpeedNames[4], "128x Sidereal");
	SlewSpeeds[5] = 256.0;		sprintf(SlewSpeedNames[5], "256x Sidereal");
	SlewSpeeds[6] = 800.0;		sprintf(SlewSpeedNames[6], "800x Sidereal");
	
	sprintf(GuideSpeedNames[0], "1.0 Sidereal");
	sprintf(GuideSpeedNames[1], "3/4 Sidereal");
	sprintf(GuideSpeedNames[2], "1/2 Sidereal");
	sprintf(GuideSpeedNames[3], "1/4 Sidereal");
	
#ifdef HEQ5_DEBUG
	int i;
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	for (i = 0; i < NSLEWSPEEDS; i++) {
		fprintf(LogFile, "[%s] Slew Speed[%d] %f %s\n", timestamp, i, SlewSpeeds[i], SlewSpeedNames[i]);
	}
#endif

	m_CurrentRateIndex = 3;
	
	// Read the current stored values for the settings
	if (m_pIniUtil)
	{
		m_HomePolarisClock = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_ALIGNMENT_CLOCK_POSITION, -1);
		m_HomeAlignmentDEC = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_DEC, -1000.0);
		m_HomeAlignmentHA = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_HA, -1000.0);
		m_iST4GuideRateIndex = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_GUIDERATE, 2);
		m_iPostSlewDelay =  m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SLEWDELAY, 0);
		m_bParked = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_ISPARKED, 0);
		m_lDecParkEncoder = (unsigned long) m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_DECPARKENCODER, 0.0);
		m_lRaParkEncoder = (unsigned long) m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_RAPARKENCODER, 0.0);
		m_dEastSlewLim = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_EASTSLEWLIM, 0.0);
		m_dWestSlewLim = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_WESTSLEWLIM, 0.0);
		m_dFlipHourAngle = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_FLIPHOURANGLE, 0.0);
		m_dMinAngleAboveHorizon = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_ANGLEABOVEHORIZON, 0.0);
		m_iBaudRate = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_BAUDRATE, 9600);
		m_iLEDBrightness = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_LEDBRIGHTNESS, 128);
		m_bPecEnabled = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_PECENABLED, 0);
		m_bWiFiEnabled = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_WIFIENABLED, 0);
		m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_WIFIIP, "192.168.4.1", m_cWiFiIPAddress, MAX_PORT_NAME_SIZE);
		m_iWiFiPort = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_WIFIPORT, 11880);
	}

#ifdef HEQ5_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Park Encoder Values: Parked %d RA %u Dec %u\n", timestamp, m_bParked, m_lRaParkEncoder, m_lDecParkEncoder);
	fprintf(LogFile, "[%s] PEC Setting:  PecEnabled %d\n", timestamp, m_bPecEnabled);
#endif

	// Set the guiderate index
	
	if (m_HomeAlignmentDEC > -1000 && m_HomeAlignmentHA > -1000 && m_HomePolarisClock > -1) {
		m_bPolarisHomeAlignmentSet = true;
	}
	else {
		m_bPolarisHomeAlignmentSet = false;
	}
	
	m_bPolarisAlignmentSlew = false;   // Only true when slewing to Polaris for alignment
	
#ifdef HEQ5_DEBUG
	if (LogFile) {
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Polaris Alignment: %f %f %d %d %d\n", timestamp, m_HomeAlignmentDEC, m_HomeAlignmentHA, m_HomePolarisClock, m_bPolarisHomeAlignmentSet, m_iST4GuideRateIndex);
	}
#endif
	X2MutexLocker ml(GetMutex());  // Mount should not be connected yet, but just in case...
	SkyW.SetST4GuideRate(m_iST4GuideRateIndex);
	SkyW.SetBaudRate(m_iBaudRate);
	
}

X2Mount::~X2Mount()
{
	// Write the stored values
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Polaris Alignment on exit: %f %f %d %d %d\n", timestamp, m_HomeAlignmentDEC, m_HomeAlignmentHA, m_HomePolarisClock, m_bPolarisHomeAlignmentSet, m_iST4GuideRateIndex);
	}
#endif
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_ALIGNMENT_CLOCK_POSITION, m_HomePolarisClock);
	m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_DEC, m_HomeAlignmentDEC);
	m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_HA, m_HomeAlignmentHA);
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_GUIDERATE, m_iST4GuideRateIndex);
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SLEWDELAY, m_iPostSlewDelay);
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_ISPARKED, m_bParked);
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_LEDBRIGHTNESS, m_iLEDBrightness);

	if (m_pSerX)
		delete m_pSerX;
	if (m_pTheSkyXForMounts)
		delete m_pTheSkyXForMounts;
	if (m_pSleeper)
		delete m_pSleeper;
	if (m_pIniUtil)
		delete m_pIniUtil;
	if (m_pLogger)
		delete m_pLogger;
	if (m_pIOMutex)
		delete m_pIOMutex;
	if (m_pTickCount)
		delete m_pTickCount;
	
#ifdef HEQ5_DEBUG
	// Close LogFile
	if (LogFile) {
		fclose(LogFile);
	}
#endif
	
}

int X2Mount::queryAbstraction(const char* pszName, void** ppVal)
{
	*ppVal = NULL;
	
	if (!strcmp(pszName, SyncMountInterface_Name))
	    *ppVal = dynamic_cast<SyncMountInterface*>(this);
	if (!strcmp(pszName, SlewToInterface_Name))
		*ppVal = dynamic_cast<SlewToInterface*>(this);
	else if (!strcmp(pszName, AsymmetricalEquatorialInterface_Name))
		*ppVal = dynamic_cast<AsymmetricalEquatorialInterface*>(this);
	else if (!strcmp(pszName, OpenLoopMoveInterface_Name))
		*ppVal = dynamic_cast<OpenLoopMoveInterface*>(this);
	else if (!strcmp(pszName, NeedsRefractionInterface_Name))
		*ppVal = dynamic_cast<NeedsRefractionInterface*>(this);
	//else if (!strcmp(pszName, LinkFromUIThreadInterface_Name))
	//	*ppVal = dynamic_cast<LinkFromUIThreadInterface*>(this);
	else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
		*ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
	else if (!strcmp(pszName, X2GUIEventInterface_Name))
		*ppVal = dynamic_cast<X2GUIEventInterface*>(this);
	else if (!strcmp(pszName, TrackingRatesInterface_Name))
		*ppVal = dynamic_cast<TrackingRatesInterface*>(this);
	else if (!strcmp(pszName, ParkInterface_Name))
		*ppVal = dynamic_cast<ParkInterface*>(this);
	else if (!strcmp(pszName, UnparkInterface_Name))
		*ppVal = dynamic_cast<UnparkInterface*>(this);
    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

	//Add support for the optional LoggerInterface
	/* if (!strcmp(pszName, LoggerInterface_Name)) {
		*ppVal = GetLogger();
	 }
	 */
	
#ifdef HEQ5_DEBUG
#ifdef VERBOSE
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Query Abstraction Called: %s\n", timestamp, pszName);
	}
#endif
#endif
	return SB_OK;
}

//OpenLoopMoveInterface
int X2Mount::startOpenLoopMove(const MountDriverInterface::MoveDir& Dir, const int& nRateIndex)
{
	X2MutexLocker ml(GetMutex());
	m_CurrentRateIndex = nRateIndex;
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] startOpenLoopMove called %d %d\n", timestamp, Dir, nRateIndex);
	}
#endif
	return SkyW.StartOpenSlew(Dir, SlewSpeeds[nRateIndex]);
}

int X2Mount::endOpenLoopMove(void)
{
	int err;
#ifdef HEQ5_DEBUG
	if (LogFile){
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] endOpenLoopMove Called\n", timestamp);
	}
#endif
	X2MutexLocker ml(GetMutex());
	err = SkyW.ResetMotions(); if (err) return err;                        // Seems to be required by AstroEQ mounts.
	return SkyW.SetTrackingRates(true, true, 0.0, 0.0);					   // Starting to track will stop move
}

int X2Mount::rateCountOpenLoopMove(void) const
{
	return NSLEWSPEEDS;
}

int X2Mount::rateNameFromIndexOpenLoopMove(const int& nZeroBasedIndex, char* pszOut, const int& nOutMaxSize)
{
	snprintf(pszOut, nOutMaxSize, "%s", SlewSpeedNames[nZeroBasedIndex]);
	return SB_OK;
}

int X2Mount::rateIndexOpenLoopMove(void)
{
	return m_CurrentRateIndex;
}

int X2Mount::execModalSettingsDialog(void)
{
	int nErr = SB_OK;
	X2ModalUIUtil uiutil(this, m_pTheSkyXForMounts);
	X2GUIInterface*	ui = uiutil.X2UI();
	X2GUIExchangeInterface* dx = NULL;		//Comes after ui is loaded
	bool bPressedOK = false;
	int i;
	char ColourLabel[400];					// String to use to change colour of label.
	char WiFiPortString[MAX_PORT_NAME_SIZE];	// String to use for wifi port
	
#ifdef HEQ5_DEBUG
	time_t ltime;
	char *timestamp;
	if (LogFile) {
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] execModelSettingsDialog called %d\n", timestamp, m_bPolarisHomeAlignmentSet);
	}
#endif
	
	if (NULL == ui) return ERR_POINTER;
	
	if ((nErr = ui->loadUserInterface("Skywatcher.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
		return nErr;
	
	if (NULL == (dx = uiutil.X2DX())) {
		return ERR_POINTER;
	}
	
	// Set values in the userinterface

	dx->comboBoxAppendString("comboBox", "12 O'Clock");
	dx->comboBoxAppendString("comboBox", "3 O'Clock");
	dx->comboBoxAppendString("comboBox", "6 O'Clock");
	dx->comboBoxAppendString("comboBox", "9 O'Clock");
	if (m_HomePolarisClock != -1) dx->setCurrentIndex("comboBox", m_HomePolarisClock);
	
	for (i = 0; i < NGUIDESPEEDS; i++) {
		dx->comboBoxAppendString("comboBox_2", GuideSpeedNames[i]);
	}
	if (m_iST4GuideRateIndex != -1) dx->setCurrentIndex("comboBox_2", m_iST4GuideRateIndex);
	
	// Now set the post-slew delay time
	dx->setPropertyInt("spinBox", "value", m_iPostSlewDelay);
	
	// Now enable and disable relevant buttons
	if (m_bPolarisHomeAlignmentSet && SkyW.isConnected() && !m_bParked) {
		dx->setEnabled("pushButton_2", true);				  // Move to right polar alignment slot
	}
	else {
		dx->setEnabled("pushButton_2", false);				 // Cannot move while parked, not connected or haven't aligned the reticule yet
	}
	
	if (!SkyW.isConnected() || m_bParked) {
		dx->setEnabled("pushButton", false);				 // Must be conneected and not parked to set polar alignment home location
		dx->setEnabled("comboBox", false);
	}
	if (!SkyW.isConnected()) {
		sprintf(ColourLabel, "%sDisconnected", QTRED);
		dx->setPropertyString("label_4", "text", ColourLabel);;
	}
	else if (m_bParked) {
		sprintf(ColourLabel, "%sParked", QTGREEN);
		dx->setPropertyString("label_4", "text", ColourLabel);;
	}
	else if (SkyW.GetIsTracking()) {
		sprintf(ColourLabel, "%sTracking On", QTGREEN);
		dx->setPropertyString("label_4", "text", ColourLabel);;
	}
	else {
		sprintf(ColourLabel, "%sTracking Off", QTGREEN);
		dx->setPropertyString("label_4", "text", ColourLabel);;
	}
	
	// Now set the values for the tracking limits.
	dx->setPropertyDouble("doubleSpinBox", "value", m_dEastSlewLim);
	dx->setPropertyDouble("doubleSpinBox_2", "value", m_dWestSlewLim);
	dx->setPropertyDouble("doubleSpinBox_3", "value", m_dFlipHourAngle);
	dx->setPropertyDouble("doubleSpinBox_4", "value", m_dMinAngleAboveHorizon);

	// Store the values in the temp variables to allow comparison and checking the uievent function.
	m_dTempEastSlewLim = m_dEastSlewLim;
	m_dTempWestSlewLim = m_dWestSlewLim;
	m_dTempFlipHourAngle = m_dFlipHourAngle;

	// Set the value for the polar LED brightness and set enabled if connected
	dx->setPropertyInt("horizontalSlider", "value", m_iLEDBrightness);
	if (SkyW.isConnected()) {
		if (SkyW.GetDoesMountHavePolarScope()) {
			dx->setEnabled("horizontalSlider", true);
		}
		else {
			dx->setPropertyString("label_5", "text", "Cannot set LED brightness");
			dx->setEnabled("horizontalSlider", false);
		}
	}
	else {
		dx->setEnabled("horizontalSlider", false);
	}

	// Slew limits should be OK - and if not, will get checked in any event
	sprintf(ColourLabel, "%sSlew limits OK", QTGREEN);
	dx->setPropertyString("label_12", "text", ColourLabel);;

#ifdef HEQ5_DEBUG
	if (LogFile) {
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] execModealSetting::  Input Slew Limits %02f %02f %02f %02f\n", timestamp, m_dEastSlewLim, m_dWestSlewLim, m_dFlipHourAngle, m_dMinAngleAboveHorizon);
	}
#endif

	// Now PEC Interface.
	if (!SkyW.isConnected()) {
		dx->setEnabled("pushButton_3", false);
		dx->setEnabled("checkBox", false);
		dx->setChecked("checkBox", false);
		sprintf(ColourLabel, "%sDisconnected", QTRED);
		dx->setPropertyString("label_16", "text", ColourLabel);
	}
	else if (!SkyW.GetIsPecSupported()) {
		dx->setEnabled("pushButton_3", false);
		dx->setEnabled("checkBox", false);
		dx->setPropertyString("label_16", "text", "PEC Not Supported on this mount");
	}
	else if (m_bParked) {
		dx->setEnabled("pushButton_3", false);
		dx->setEnabled("checkBox", true);
		dx->setChecked("checkBox", m_bPecEnabled);
		sprintf(ColourLabel, "%sParked", QTGREEN);
		dx->setPropertyString("label_16", "text", ColourLabel);
	} 
	else {
		dx->setEnabled("pushButton_3", SkyW.GetIsTracking());
		if (SkyW.GetDoesMountHaveValidPecData()) {
			dx->setEnabled("checkBox", true);
			dx->setChecked("checkBox", m_bPecEnabled);
			if (m_bPecEnabled) {
				sprintf(ColourLabel, "%sPPEC Trained and Enabled", QTGREEN);
			}
			else {
				sprintf(ColourLabel, "%sPPEC Trained but disabled", QTRED);
			}
			dx->setPropertyString("label_16", "text", ColourLabel);
		}
		else {
			dx->setEnabled("checkBox", false);
			dx->setChecked("checkBox", false);
			sprintf(ColourLabel, "%sMount does not have valid PPEC data. Ensure autoguider is running then click \"Start PPEC Training\" to store.", QTRED);
			dx->setPropertyString("label_16", "text", ColourLabel);
		}
	}

	// At start, PPEC training will not be on
	m_bIsPECTraining = false;

	// Now the Wifi interface
	dx->setPropertyString("lineEdit", "text", m_cWiFiIPAddress);
	snprintf(WiFiPortString, MAX_PORT_NAME_SIZE, "%d", m_iWiFiPort);
	dx->setPropertyString("lineEdit_2", "text", WiFiPortString);
	dx->setChecked("checkBox_2", m_bWiFiEnabled);

	// Checking box will cause a changed state event. If enabled, will be handled by uievent routine.
	// Just need to handle the not enabled set up.
	if (!m_bWiFiEnabled) {
	  sprintf(ColourLabel, "%sWiFi Disconnected", QTRED);
	  dx->setPropertyString("label_20", "text", ColourLabel);
	}
	  
#ifdef HEQ5_DEBUG
	if (LogFile) {
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] execModelSettingsDialog IsConnected %d\n", timestamp, SkyW.isConnected());
	}
#endif
	// Is the mount connected? If so, disable controls
	if (SkyW.isConnected()) {
	  
	  dx->setEnabled("lineEdit", false);
	  dx->setEnabled("lineEdit_2", false);
	  dx->setEnabled("checkBox_2", false);  
	}

	// Ensure wifi interface is disabled if not on unix
	/* 
//#ifndef SB_LINUX_BUILD 
	dx->setEnabled("lineEdit", false);
	dx->setEnabled("lineEdit_2", false);
	dx->setEnabled("checkBox_2", false);
	sprintf(ColourLabel, "%sOnly enabled under LINUX", QTRED);
	dx->setPropertyString("label_20", "text", ColourLabel);
//#endif
*/

	//Display the user interface
	if ((nErr = ui->exec(bPressedOK)))
		return nErr;
	
	//Retreive values from the user interface
	if (bPressedOK)
	{
#ifdef HEQ5_DEBUG
		if (LogFile) {
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] execModealSetting:: Pressed OK button\n", timestamp);
		}
#endif
		m_iST4GuideRateIndex = dx->currentIndex("comboBox_2");
		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_GUIDERATE, m_iST4GuideRateIndex);
		SkyW.SetST4GuideRate(m_iST4GuideRateIndex);
		dx->propertyInt("spinBox", "value", m_iPostSlewDelay);
		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SLEWDELAY, m_iPostSlewDelay);

		// Now read and store the values for the tracking and slewing limits
		dx->propertyDouble("doubleSpinBox_2", "value", m_dWestSlewLim);
		dx->propertyDouble("doubleSpinBox", "value", m_dEastSlewLim);

		dx->propertyDouble("doubleSpinBox_3", "value", m_dFlipHourAngle);
		dx->propertyDouble("doubleSpinBox_4", "value", m_dMinAngleAboveHorizon);
		m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_EASTSLEWLIM, m_dEastSlewLim);
		m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_WESTSLEWLIM, m_dWestSlewLim);
		m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_FLIPHOURANGLE, m_dFlipHourAngle);
		m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_ANGLEABOVEHORIZON, m_dMinAngleAboveHorizon);
		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_LEDBRIGHTNESS, m_iLEDBrightness);
		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_PECENABLED, m_bPecEnabled);

		dx->propertyString("lineEdit", "text", m_cWiFiIPAddress, MAX_PORT_NAME_SIZE);
		dx->propertyString("lineEdit_2", "text", WiFiPortString, MAX_PORT_NAME_SIZE);
		m_iWiFiPort = atoi(WiFiPortString);
		m_bWiFiEnabled = dx->isChecked("checkBox_2");

#ifdef HEQ5_DEBUG
		if (LogFile) {
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] execModealSetting::  IPadd %s port string %s port %d enabled %d\n", timestamp, m_cWiFiIPAddress, WiFiPortString, m_iWiFiPort, m_bWiFiEnabled);
		}
#endif

		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_WIFIENABLED, m_bWiFiEnabled);
		m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_WIFIIP, m_cWiFiIPAddress);
		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_WIFIPORT, m_iWiFiPort);

#ifdef HEQ5_DEBUG
		if (LogFile) {
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] execModealSetting::  Output Slew Limits %02f %02f %02f %02f\n", timestamp, m_dEastSlewLim, m_dWestSlewLim, m_dFlipHourAngle, m_dMinAngleAboveHorizon);
			fprintf(LogFile, "[%s] execModealSetting::  PecEnabled %d\n", timestamp, m_bPecEnabled);
		}
#endif
    }
	return nErr;
}

void X2Mount::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
	double HAPolaris, HAOctansSigma;
	double const RAPolaris = 2.879280694;							// Calculated on 12/2/2017
	double const RAOctansSigma = 21.36480556;						// Calculated on 24/2/2017
	double readtemp;												// For reading current slew variables
	bool ichange = false;											// Have the slew variables changed?
	bool slewerr = false;											// Is there an error in the slew variables?
	int itemp;														// For reading temporary illuminator data
	char Orientation[200];											// Label for polris/Octans orientation
	char *StarName;													// Name of star for orientation
	double HAStar;													// HA for star for orientation
	int hours;														// Hour for orientation label
	int minutes;													// Minutes for orientation label
	char ColourLabel[400];											// String to use when want to clour a label
	char WiFiPortString[MAX_PORT_NAME_SIZE];						// String for name of WiFiPort

	int err;
	X2MutexLocker ml(GetMutex());

#ifdef HEQ5_DEBUG
	time_t ltime;
	char *timestamp;
	if (LogFile) {
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] uievent %s\n", timestamp, pszEvent);
	}
#endif
	if (!strcmp(pszEvent, "on_timer")) { // Periodical event - use it to check status of slew data and polar illuminator data


		// Set the orientation label
		HAPolaris = m_pTheSkyXForMounts->hourAngle(RAPolaris);
		HAOctansSigma = m_pTheSkyXForMounts->hourAngle(RAOctansSigma);

		// Choose star dependent on latitude.
		if (m_pTheSkyXForMounts->latitude() > 0) {
			// Northern Hemisphere
			StarName = "Polaris";
			HAStar = m_pTheSkyXForMounts->hourAngle(RAPolaris);
		}
		else {
			StarName = "Octans";
			HAStar = m_pTheSkyXForMounts->hourAngle(RAOctansSigma);
		}
		HAStar += 12.0;						// Allow for inversion
		if (HAStar > 24.0) HAStar -= 24.0;	// Ensure in right range
		HAStar = (24 - HAStar) / 2.0;			// Earth goes opposite way to the clock. Also allow for 12 hour display so divide by 2

		// Convert to hours and minutes
		hours = floor(HAStar);
		minutes = floor((HAStar - hours) * 60 + 0.5);

		// Format String
		sprintf(Orientation, "%s clock location:     %02d:%02d", StarName, hours, minutes);
		uiex->setPropertyString("label_17", "text", Orientation);

		// Read the value of the horizontal slider
		uiex->propertyInt("horizontalSlider", "value", itemp);
		if (itemp != m_iLEDBrightness && SkyW.GetDoesMountHavePolarScope()) {
			m_iLEDBrightness = itemp;
			m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_LEDBRIGHTNESS, m_iLEDBrightness);
			SkyW.SetPolarScopeIllumination(m_iLEDBrightness);
#ifdef HEQ5_DEBUG
			time_t ltime;
			char *timestamp;
			if (LogFile) {
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				fprintf(LogFile, "[%s] LED Illuminator %d\n", timestamp, m_iLEDBrightness);
			}
#endif
		}


		// First read the slew limits values and see if any have changed
		uiex->propertyDouble("doubleSpinBox", "value", readtemp);
		if (fabs(readtemp - m_dTempEastSlewLim) > 0.0001) {
			m_dTempEastSlewLim = readtemp;
			ichange = true;
		}

		uiex->propertyDouble("doubleSpinBox_2", "value", readtemp);
		if (fabs(readtemp - m_dTempWestSlewLim) > 0.0001) {
			m_dTempWestSlewLim = readtemp;
			ichange = true;
		}

		uiex->propertyDouble("doubleSpinBox_3", "value", readtemp);
		if (fabs(readtemp - m_dTempFlipHourAngle) > 0.0001) {
			m_dTempFlipHourAngle = readtemp;
			ichange = true;
		}

#ifdef HEQ5_DEBUG
		if (LogFile) {
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] uiEvent::  Input Slew Limits %02f %02f %02f\n", timestamp, m_dTempEastSlewLim, m_dTempWestSlewLim, m_dTempFlipHourAngle);
		}
#endif
		// Check the variables if the slew limits have changed
		if (ichange) {
			if (m_dTempEastSlewLim > m_dTempWestSlewLim) {
				sprintf(ColourLabel, "%sWest Limit set equal to East Limit: West limit must be >= East Limit", QTRED);
				uiex->setPropertyString("label_12", "text", ColourLabel);
				m_dTempWestSlewLim = m_dTempEastSlewLim;
				uiex->setPropertyDouble("doubleSpinBox_2", "value", m_dTempWestSlewLim);
				slewerr = true;
			}
			if (m_dTempFlipHourAngle < m_dTempEastSlewLim) {
				sprintf(ColourLabel, "%sFlip hour angle set equal to East Limit: must be >= East Limit", QTRED);
				uiex->setPropertyString("label_12", "text", ColourLabel);
				m_dTempFlipHourAngle = m_dTempEastSlewLim;
				uiex->setPropertyDouble("doubleSpinBox_3", "value", m_dTempFlipHourAngle);
				slewerr = true;
			}
			if (m_dTempFlipHourAngle > m_dTempWestSlewLim) {
				sprintf(ColourLabel, "%sFlip hour angle set to West Limit: must be <= West Limit", QTRED);
				uiex->setPropertyString("label_12", "text", ColourLabel);
				m_dTempFlipHourAngle = m_dTempWestSlewLim;
				uiex->setPropertyDouble("doubleSpinBox_3", "value", m_dTempFlipHourAngle);
				slewerr = true;
			}
			// If no errors, say slew limits are OK.
			if (!slewerr) {
				sprintf(ColourLabel, "%sSlew limits OK", QTGREEN);
				uiex->setPropertyString("label_12", "text", ColourLabel);
			}
		}

		// Check to see PPEC is running
		if (m_bIsPECTraining) {  // No need to go further if not running
			if (!SkyW.GetIsPecTrainingOn()) {  // PPEC Training Now Completed
				SkyW.TurnOnPec();
				if (SkyW.GetIsPecTrackingOn()) {
					uiex->setPropertyString("pushButton_3", "text", "Start PPEC Training"); // Change label to indicate now cancelled.
					sprintf(ColourLabel, "%sPPEC Training Completed. Data stored and PPEC running.", QTGREEN);
					uiex->setPropertyString("label_16", "text", ColourLabel);
					uiex->setEnabled("checkBox", true);
					if (!uiex->isChecked("checkBox")) m_bCausedCheckBoxStateChange = true;  // See if have changed state of check box - if so, we should ignore next ui event.
					uiex->setChecked("checkBox", true);
					m_bPecEnabled = true;
				}
				else {
					uiex->setPropertyString("pushButton_3", "text", "Start PPEC Training"); // Change label to indicate now cancelled.
					sprintf(ColourLabel, "%sPPEC Training Failed.", QTRED);
					uiex->setPropertyString("label_16", "text", ColourLabel);
					uiex->setEnabled("checkBox", false);
					if (uiex->isChecked("checkBox")) m_bCausedCheckBoxStateChange = true;  // See if have changed state of check box - if so, we should ignore next ui event.
					uiex->setChecked("checkBox", false);
					m_bPecEnabled = false;
				}

				// Re-enable buttons
				uiex->setEnabled("pushButton", true);  // And all other pushbuttons
				if (m_bPolarisHomeAlignmentSet) uiex->setEnabled("pushButton_2", true);  // And all other pushbuttons
				uiex->setEnabled("pushButtonOK", true);  // And all other pushbuttons
				uiex->setEnabled("pushButtonCancel", true);  // And all other pushbuttons

				// Record that training is now completed
				m_bIsPECTraining = false;
				return;
			}
		}

		// Now see if need to 
		if (!m_bPolarisAlignmentSlew) return; // No need to do anything unless slewing
#ifdef HEQ5_DEBUG
		if (LogFile) {
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] uievent %s %d %d\n", timestamp, pszEvent, SkyW.GetIsNotGoto(), m_bPolarisAlignmentSlew);
		}
#endif
		if (!SkyW.GetIsPolarAlignInProgress()) {			// Slewing has finished
			uiex->setPropertyString("pushButton_2", "text", "Move to Alignment Position"); // Change label to indicate action taken
			m_bPolarisAlignmentSlew = false;
			setTrackingRates(true, true, 0.0, 0.0);        // Start tracking
			sprintf(ColourLabel, "%sTracking On", QTGREEN);
			uiex->setPropertyString("label_4", "text", ColourLabel);
			return;
		}
	}

	if (!strcmp(pszEvent, "on_pushButton_clicked")) { //Set the home polarbutton
		m_HomePolarisClock = uiex->currentIndex("comboBox");
		err = SkyW.GetMountHAandDec(m_HomeAlignmentHA, m_HomeAlignmentDEC);
		if (err) {
			uiex->messageBox("Error", "Problem getting current HA and DEC. Try again.");
			return;
		}

		uiex->setPropertyString("pushButton", "text", "Set"); // Change label to indicate action taken


		// Store values in init file
#ifdef HEQ5_DEBUG
		if (LogFile) {
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] pushButton_clicked Index %d Ra %f dec%f\n", timestamp, m_HomePolarisClock, m_HomeAlignmentHA, m_HomeAlignmentDEC);
		}
#endif
		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_ALIGNMENT_CLOCK_POSITION, m_HomePolarisClock);
		m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_DEC, m_HomeAlignmentDEC);
		m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_HA, m_HomeAlignmentHA);
		m_bPolarisHomeAlignmentSet = true;
		// Now permit move to alignement to be on
		uiex->setEnabled("pushButton_2", true);


	}

	if (!strcmp(pszEvent, "on_pushButton_2_clicked")) { //Move to alignment
		// Calculate current HA of Polaris
		if (!m_bPolarisAlignmentSlew) {
			HAPolaris = m_pTheSkyXForMounts->hourAngle(RAPolaris);
			HAOctansSigma = m_pTheSkyXForMounts->hourAngle(RAOctansSigma);
			uiex->messageBox("Warning", "May move counterweights above scope");
			err = SkyW.PolarAlignment(m_HomeAlignmentHA, m_HomeAlignmentDEC, m_HomePolarisClock, HAPolaris, HAOctansSigma);
			if (err) {
				uiex->messageBox("Error", "Problem getting current HA and DEC. Connection error?");
				return;
			}

			m_bPolarisAlignmentSlew = true;
			uiex->setPropertyString("pushButton_2", "text", "Abort"); // Change label to indicate action taken
			uiex->setPropertyString("label_4", "text", "Slewing..."); //Update Status
			return;
		}
		else {
			err = SkyW.Abort();
			uiex->setPropertyString("pushButton_2", "text", "Move to Alignment Position"); // Change label to indicate action taken
			sprintf(ColourLabel, "%sTracking Off", QTRED);
			uiex->setPropertyString("label_4", "text", ColourLabel);
			m_bPolarisAlignmentSlew = false;
			return;
		}
	}

	// Now deal with PEC Training data
	if (!strcmp(pszEvent, "on_pushButton_3_clicked")) { // Start PPEC training
		if (!m_bIsPECTraining) {
			uiex->setPropertyString("pushButton_3", "text", "Cancel"); // Change label to indicate action taken
			sprintf(ColourLabel, "%sPPEC Training Running...", QTGREEN);
			uiex->setPropertyString("label_16", "text", ColourLabel);
			uiex->setEnabled("checkBox", false);    // Turn off ability to change PEC
			uiex->setEnabled("pushButton", false);  // And all other pushbuttons
			uiex->setEnabled("pushButton_2", false);  // And all other pushbuttons
			uiex->setEnabled("pushButtonOK", false);  // And all other pushbuttons
			uiex->setEnabled("pushButtonCancel", false);  // And all other pushbuttons

			SkyW.StartPecTraining();
			m_bIsPECTraining = true;
#ifdef HEQ5_DEBUG
			if (LogFile) {
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				fprintf(LogFile, "[%s] pushButton_3_clicked. Started PPEC training\n", timestamp);
			}
#endif
			return;
		}
		else {
			SkyW.CancelPecTraining();
			// See if PPEC can still be turned on
			SkyW.TurnOnPec();

			// Now handle UI
			uiex->setPropertyString("pushButton_3", "text", "Start PPEC Training"); // Change label to indicate now cancelled.
			if (SkyW.GetDoesMountHaveValidPecData()) {
				sprintf(ColourLabel, "%sTraining cancelled, old PPEC being used.", QTRED);
				uiex->setPropertyString("label_16", "text", ColourLabel);
				uiex->setEnabled("checkBox", true);
				if (uiex->isChecked("checkBox") != m_bPecEnabled) m_bCausedCheckBoxStateChange = true;  // See if have changed state of check box - if so, we should ignore next ui event.
				uiex->setChecked("checkBox", m_bPecEnabled);
			}
			else {
				sprintf(ColourLabel, "%sTraining cancelled. Mount does not have valid PPEC data. Ensure autoguider is running then click \"Start PPEC Training\" to store.", QTRED);
				uiex->setPropertyString("label_16", "text", ColourLabel);
				uiex->setEnabled("checkBox", false);
				if (uiex->isChecked("checkBox")) m_bCausedCheckBoxStateChange = true;  // See if have changed state of check box - if so, we should ignore next ui event.
				uiex->setChecked("checkBox", false);
				m_bPecEnabled = false;
			}
			// Re-enable buttons
			uiex->setEnabled("pushButton", true);  // And all other pushbuttons
			if (m_bPolarisHomeAlignmentSet) uiex->setEnabled("pushButton_2", true);  // And all other pushbuttons
			uiex->setEnabled("pushButtonOK", true);  // And all other pushbuttons
			uiex->setEnabled("pushButtonCancel", true);  // And all other pushbuttons

			m_bIsPECTraining = false;
			return;
		}
	}

	// Now deal with Enabling/Disabling PPEC
	if (!strcmp(pszEvent, "on_checkBox_stateChanged")) { // Enabled or disabled box
		if (m_bCausedCheckBoxStateChange) {
			m_bCausedCheckBoxStateChange = false;
		}
		else {
			if (uiex->isChecked("checkBox")) {
				SkyW.TurnOnPec();
				if (SkyW.GetIsPecTrackingOn()) {
					sprintf(ColourLabel, "%sPPEC Tracking Enabled", QTGREEN);
					uiex->setPropertyString("label_16", "text", ColourLabel);
					m_bPecEnabled = true;
				}
				else {
					sprintf(ColourLabel, "%sUnable to start PPEC", QTRED);
					uiex->setPropertyString("label_16", "text", ColourLabel);
					m_bPecEnabled = false;
				}
			}
			else {
				SkyW.TurnOffPec();
				sprintf(ColourLabel, "%sPPEC Tracking Disabled", QTRED);
				uiex->setPropertyString("label_16", "text", ColourLabel);
				m_bPecEnabled = false;
			}
		}
	}

	// Now deal with Enabling/Disabling WiFi
	if (!strcmp(pszEvent, "on_checkBox_2_stateChanged")) { // Enabled or disabled box
#ifdef HEQ5_DEBUG
	  if (LogFile) {
	    ltime = time(NULL);
	    timestamp = asctime(localtime(&ltime));
	    timestamp[strlen(timestamp) - 1] = 0;
	    fprintf(LogFile, "[%s] on_checkBox_2_stageChanged. Status: %d\n", timestamp, uiex->isChecked("checkBox_2"));
	  }
#endif
	  if (uiex->isChecked("checkBox_2")) {
	    // Retrieve the values and attempt to test connection
	    uiex->propertyString("lineEdit", "text", m_cWiFiIPAddress, MAX_PORT_NAME_SIZE);
	    uiex->propertyString("lineEdit_2", "text", WiFiPortString, MAX_PORT_NAME_SIZE);
	    m_iWiFiPort = atoi(WiFiPortString);
	    m_bWiFiEnabled = true;
	    // Store the connection data
	    SkyW.SetConnectionData(m_PortName, m_cWiFiIPAddress, m_iWiFiPort, m_bWiFiEnabled);
	    if (SkyW.WiFiCheck()== SB_OK) {
	      if (SkyW.isConnected()) {
		sprintf(ColourLabel, "%sWiFi Connected to %s", QTGREEN, SkyW.GetMountName());
		uiex->setPropertyString("label_20", "text", ColourLabel);
	      } else {
		sprintf(ColourLabel, "%sWiFi found %s", QTGREEN, SkyW.GetMountName());
		uiex->setPropertyString("label_20", "text", ColourLabel);
	      }
	    } else {
	      sprintf(ColourLabel, "%sCould not connect wifi. Untick enable wifi to edit data.", QTRED);
	      uiex->setPropertyString("label_20", "text", ColourLabel);
	    }
	    // Disable editing of data when connected to wifi
	    uiex->setEnabled("lineEdit", false);
	    uiex->setEnabled("lineEdit_2", false);
	  }
	  else {
	    sprintf(ColourLabel, "%sWiFi Disconnected", QTRED);
	    uiex->setPropertyString("label_20", "text", ColourLabel);
	    m_bWiFiEnabled = false;
	    // Enable editiing of data when not connected
	    uiex->setEnabled("lineEdit", true);
	    uiex->setEnabled("lineEdit_2", true);

	  }
	}
	
}

//LinkInterface
int X2Mount::establishLink(void)
{
	int err;
	X2MutexLocker ml(GetMutex());

	// get serial port device name
	portNameOnToCharPtr(m_PortName, DRIVER_MAX_STRING);
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Establish Link called Portname %s wifi %d\n", timestamp, m_PortName, m_bWiFiEnabled);
	}
#endif

	// Store the connection data
	SkyW.SetConnectionData(m_PortName, m_cWiFiIPAddress, m_iWiFiPort, m_bWiFiEnabled);
	// Now Connect
	err = SkyW.Connect();

	// Retrieve and store successful baud rate
	m_iBaudRate = SkyW.GetBaudRate();
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_BAUDRATE, m_iBaudRate);

#ifdef HEQ5_DEBUG
		if (LogFile) {
			time_t ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] Establish Link - connected to port %s Baud Rate %d err %d\n", timestamp, m_PortName, m_iBaudRate ,err);
		}
#endif
		
	if (err) return err;

	// Set the Polar Illuminator Value
	err = SkyW.SetPolarScopeIllumination(m_iLEDBrightness); if (err) return err;

	// PEC enabled by default in the Connect routine. Turn off if stored state is off.
	if (!m_bPecEnabled) {
		err = SkyW.TurnOffPec(); if (err) return err;
	}

	// If mount was previously parked, set the encoders to the parked values
	if (m_bParked && m_lDecParkEncoder > 0 && m_lRaParkEncoder > 0) {
		err = SkyW.SyncToEncoder(m_lRaParkEncoder, m_lDecParkEncoder, false);
		if (err) return err;
		// Behave like a Paramount - unpark the mount on connection
		endUnpark();
	}

#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Establish Link - Park %d Dec Encoder %u, Ra Encloder %u err %d\n", timestamp, m_bParked, m_lRaParkEncoder, m_lDecParkEncoder, err);
	}
#endif

	return err;

}

int X2Mount::terminateLink(void)
{
	X2MutexLocker ml(GetMutex());
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Terminate Link called\n", timestamp);
	}
#endif
	return SkyW.Disconnect();
}

bool X2Mount::isLinked(void) const
{
	bool temp;
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
//		timestamp = asctime(localtime(&ltime));
//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] isLinked called\n", timestamp);
	}
#endif
	
	temp = SkyW.isConnected();
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		//		timestamp = asctime(localtime(&ltime));
		//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] isLinked called returning %d\n", timestamp, temp);
	}
#endif
	return temp;
}

bool X2Mount::isEstablishLinkAbortable(void) const	{return false;}

//AbstractDriverInfo


#define DISPLAY_NAMEa "Skywatcher EQDirect"
#define DISPLAY_NAMEb "Skywatcher EQDirect.b"


void	X2Mount::driverInfoDetailedInfo(BasicStringInterface& str) const
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
//		timestamp = asctime(localtime(&ltime));
//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] driverInfoDetailedInfo Called\n", timestamp);
	}
#endif
	str = "EQ Direct X2 plugin by Colin McGill";
}

double	X2Mount::driverInfoVersion(void) const
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
//		timestamp = asctime(localtime(&ltime));
//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] driverInfoVersion Called\n", timestamp);
	}
#endif
	return SKYWATCHER_DRIVER_VERSION;
}

//AbstractDeviceInfo
void X2Mount::deviceInfoNameShort(BasicStringInterface& str) const
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
//		timestamp = asctime(localtime(&ltime));
//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] driverInfoNameShort Called\n", timestamp);
	}
#endif
	str = "Doesn't Seem to be used";
}
void X2Mount::deviceInfoNameLong(BasicStringInterface& str) const
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
//		timestamp = asctime(localtime(&ltime));
//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] deviceInfoNameLong Called\n", timestamp);
	}
#endif
	str = "Skywatcher EQ Mount";
	
}
void X2Mount::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
//		timestamp = asctime(localtime(&ltime));
//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] driverInfoDetailedDescription Called\n", timestamp);
	}
#endif
	str = "Connects to a Skywatcher equatorial mount through an EQDIR cable - see the EQMOD project for details";
	
}
void X2Mount::deviceInfoFirmwareVersion(BasicStringInterface& str)
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] driverInfoFirmwareVersion Called\n", timestamp);
	}
#endif
	if (SkyW.isConnected()) {
		str = SkyW.GetMCVersionName();
	}
	else {
		str = "Not Connected";
	}
}
void X2Mount::deviceInfoModel(BasicStringInterface& str)
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] driverInfoModel Called\n", timestamp);
	}
#endif
	if (SkyW.isConnected()) {
		str = SkyW.GetMountName();
	}
	else {
		str = "Not Connected";
	}
}

//Common Mount specifics
int X2Mount::raDec(double& ra, double& dec, const bool& bCached)
{
	int err = 0;
	double Ha = 0.0;
	double dAz, dAlt;	// To get Azimuth and Altitude later
	X2MutexLocker ml(GetMutex());
	
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] raDec Called\n", timestamp);
	}
#endif
	// Get the HA and DEC from the mount
	err = SkyW.GetMountHAandDec(Ha, dec); 

#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] raDec Called Error: %d\n", timestamp, err);
	}
#endif

	if (err) return (err);

	// Subtract HA from lst to get ra;
	ra = m_pTheSkyXForMounts->lst()-Ha;
	
	// Ensure in range
	if (ra < 0) {
		ra += 24.0;
	}
	else if (ra > 24.0) {
		ra -= 24.0;
	}
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] raDec Ra: %f Dec %f\n", timestamp, ra, dec);
	}
#endif

	// Now check if have exceeded the tracking limits
	// First tracking beyond the meridian. Must be beyond the pole (pointing east of horizon) for this to occur.
	if (SkyW.GetIsBeyondThePole() && SkyW.GetIsTracking() && Ha > m_dWestSlewLim) {
		err = setTrackingRates(false, true, 0.0, 0.0);	// Stop tracking since these have been exceeded
		if (err) return err;
		return ERR_LIMITSEXCEEDED;
	}

	// Now check to see if above the horizon
	err = m_pTheSkyXForMounts->EqToHz(ra, dec, dAz, dAlt); if (err) return err;
	if (!SkyW.GetIsBeyondThePole() && SkyW.GetIsTracking() && dAlt < m_dMinAngleAboveHorizon) {
		err = setTrackingRates(false, true, 0.0, 0.0);	if (err) return err; // Stop tracking since now to low and setting
		return ERR_LIMITSEXCEEDED;
	}
	return err;
}

int X2Mount::abort(void)
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] abort Called\n", timestamp);
	}
#endif
	X2MutexLocker ml(GetMutex());
	return SkyW.Abort();
}

int X2Mount::startSlewTo(const double& dRa, const double& dDec)
{
	int err;
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] startSlewTo Called %f %f\n", timestamp, dRa, dDec);
	}
#endif
	X2MutexLocker ml(GetMutex());
	if (dDec > 89.8 || dDec < -89.8) { 
		// Special case - if sent close to the pole, this is likely to be to the Skywatcher home location (at NCP, weights down).
		// However, at Dec = 90 deg, any RA is possible, so if parking can be at a random angle instead of weights down.
		// So, close to the pole, assume this is to the home location and park at the weights down position.
		err = SkyW.StartPark();
	}
	else {
		err = SkyW.StartSlewTo(dRa, dDec, m_dFlipHourAngle);
	}
	m_wasslewing = true;
	return err;
}

int X2Mount::isCompleteSlewTo(bool& bComplete) const
{
	bComplete = SkyW.GetIsNotGoto();
	
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
//		timestamp = asctime(localtime(&ltime));
//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] isCompleteSlewTo called %d\n", timestamp, bComplete);
	}
#endif
	
	return SB_OK;
}

int X2Mount::endSlewTo(void)
{
	X2MutexLocker ml(GetMutex()); // Stop any other commands getting to the mount until delay is finished.
	// No need to start tracking
	// But start a post-slew delay
	m_pSleeper->sleep(m_iPostSlewDelay * 1000);
	m_wasslewing = false;
	
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] endSlewTo Called\n", timestamp);
	}
#endif
	return SB_OK;
}


// Next two functions sync the mount if required.
int X2Mount::syncMount(const double& ra, const double& dec)
{
	X2MutexLocker ml(GetMutex());
	int err = SB_OK;


#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] syncMount Called ra %f dec %f\n", timestamp, ra, dec);
	}
#endif
	err = SkyW.SyncToRAandDec(ra, dec);

	m_bSynced = true;
	return err;
}

bool X2Mount::isSynced(void)
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] issyncMount Called\n", timestamp);
	}
#endif
	return true;   // As per definition - see X2 Standard for mounts - Mount does not know (or care) if synced.
}

int X2Mount::setTrackingRates(const bool& bTrackingOn, const bool& bIgnoreRates, const double& dRaRateArcSecPerSec, const double& dDecRateArcSecPerSec)
{
	X2MutexLocker ml(GetMutex());
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] setTrackingRates Called Tracking On %d Ignore Rates %d RARate %f DERate %f\n", timestamp, bTrackingOn, bIgnoreRates, dRaRateArcSecPerSec, dDecRateArcSecPerSec);
	}
#endif
	// Next line traps error in TSX 11360 on Pi
//	if (!bIgnoreRates && fabs(dRaRateArcSecPerSec + 1000) < 0.1 && fabs(dDecRateArcSecPerSec + 1000) < 0.1) return SB_OK;

	return SkyW.SetTrackingRates(bTrackingOn, bIgnoreRates, dRaRateArcSecPerSec, dDecRateArcSecPerSec);
	
}

int X2Mount::trackingRates(bool& bTrackingOn, double& dRaRateArcSecPerSec, double& dDecRateArcSecPerSec)
{
	int err;
	X2MutexLocker ml(GetMutex());

	err = SkyW.GetTrackingRates(bTrackingOn, dRaRateArcSecPerSec, dDecRateArcSecPerSec);
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] trackingRates Called Tracking On %d RARate %f DERate %f\n", timestamp, bTrackingOn, dRaRateArcSecPerSec, dDecRateArcSecPerSec);
	}
#endif
	return err;
}

//NeedsRefractionInterface
bool X2Mount::needsRefactionAdjustments(void)
{
	return true;
	
}

/* Parking Interface */
bool X2Mount::isParked(void) {
	
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] isParked Called\n", timestamp);
	}
#endif
	return m_bParked;
}

int X2Mount::startPark(const double& dAz, const double& dAlt)
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] startParked Called\n", timestamp);
	}
#endif

	return SB_OK;  // Slew carried out by TSX, so no need to do it again.
}


int X2Mount::isCompletePark(bool& bComplete) const
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
//		timestamp = asctime(localtime(&ltime));
//		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] isCompletePark Called\n", timestamp);
	}
#endif
	bComplete = SkyW.GetIsParkingComplete();
	return SB_OK;
}
int X2Mount::endPark(void)
{
	int err;

	X2MutexLocker ml(GetMutex());
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] endPark Called\n", timestamp);
	}
#endif
	err = setTrackingRates(false, true, 0.0, 0.0); if (err) return err;
	m_bParked = true;

	// Now record parked positions
	err = SkyW.GetMountEncoderValues(m_lRaParkEncoder, m_lDecParkEncoder); if (err) return err;
	err = m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_ISPARKED, m_bParked); if (err) return err;
	err = m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_RAPARKENCODER, (double) m_lRaParkEncoder); if (err) return err;
	err = m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_DECPARKENCODER, (double) m_lDecParkEncoder); if (err) return err;

#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] endPark Encoder Values: Parked %d Ra %u Dec %u\n", timestamp, m_bParked, m_lRaParkEncoder, m_lDecParkEncoder);
	}
#endif
	return err;
}

int		X2Mount::startUnpark(void)
{
	return SB_OK;
}
/*!Called to monitor the unpark process.  \param bComplete Set to true if the unpark is complete, otherwise set to false.*/
int X2Mount::isCompleteUnpark(bool& bComplete) const
{
	bComplete = true;
	return SB_OK;
}

/*!Called once the unpark is complete.	This is called once for every corresponding startUnpark() allowing software implementations of unpark.*/
int		X2Mount::endUnpark(void)
{
	m_bParked = false;
	// Recorde that mount is unparked
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_ISPARKED, m_bParked);

	return SB_OK;
}

int X2Mount::beyondThePole(bool& bYes) {
	bYes = SkyW.GetIsBeyondThePole();
	return SB_OK;
}



double X2Mount::flipHourAngle() {
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] flipHourAngle called\n", timestamp);
	}
#endif

	return m_dFlipHourAngle;
}


int X2Mount::gemLimits(double& dHoursEast, double& dHoursWest)
{
#ifdef HEQ5_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] gemLimits called\n", timestamp);
	}
#endif

	dHoursEast = m_dEastSlewLim;
	dHoursWest = m_dWestSlewLim;
	return SB_OK;
}


#pragma mark - SerialPortParams2Interface

void X2Mount::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Mount::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORT_NAME, pszPort);

}


void X2Mount::portNameOnToCharPtr(char* pszPort, const unsigned int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize,DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORT_NAME, pszPort, pszPort, nMaxSize);

}
