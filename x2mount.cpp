#include <string.h>
#include <math.h>
#include "x2mount.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/mount/asymmetricalequatorialinterface.h"
#include "../../licensedinterfaces/mount/needsrefractioninterface.h"

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

#ifdef HEQ5_DEBUG
	LogFile = fopen(HEQ5_LOGFILENAME, "w");
#endif	


	m_bSynced = false;
	m_bParked = false;

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
    time_t ltime=time(NULL);
	for (i = 0; i < NSLEWSPEEDS; i++) {
		fprintf(LogFile, "[%s] Slew Speed[%d] %f %s\n", asctime(localtime(&ltime)), i, SlewSpeeds[i], SlewSpeedNames[i]);
	}
#endif	
	m_CurrentRateIndex = 3;

	// Read the current stored values for the settings
	if (m_pIniUtil)
	{
		m_HomePolarisClock = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_ALIGNMENT_CLOCK_POSITION, -1);
		m_HomeAlignmentDEC = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_DEC, -1000.0);
		m_HomeAlignmentHA = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_HA, -1000.0);
		m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORT_NAME,DEFAULT_COMPORT, m_PortName, MAX_PORT_NAME_SIZE);
		m_iST4GuideRateIndex = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_GUIDERATE, 2);
		m_iPostSlewDelay = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SLEWDELAY, 0);
	}
	// Set the guiderate index

	if (m_HomeAlignmentDEC > -1000 && m_HomeAlignmentHA > -1000 && m_HomePolarisClock > -1) {
		m_bPolarisHomeAlignmentSet = true;
	}
	else {
		m_bPolarisHomeAlignmentSet = false;
	}

	m_bPolarisAlignmentSlew = false;   // Only true when slewing to Polaris for alignment

#ifdef HEQ5_DEBUG
    ltime=time(NULL);
	fprintf(LogFile, "[%s] Polaris Alignment: %f %f %d %d %s %d\n", asctime(localtime(&ltime)), m_HomeAlignmentDEC, m_HomeAlignmentHA, m_HomePolarisClock, m_bPolarisHomeAlignmentSet, m_PortName, m_iST4GuideRateIndex);
#endif
	X2MutexLocker ml(GetMutex());  // Mount should not be connected yet, but just in case...
	SkyW.SetST4GuideRate(m_iST4GuideRateIndex);

}
X2Mount::~X2Mount()
{
	// Write the stored values
#ifdef HEQ5_DEBUG
    time_t ltime=time(NULL);
	fprintf(LogFile, "[%s] Polaris Alignment on exit: %f %f %d %d %s %d\n", asctime(localtime(&ltime)), m_HomeAlignmentDEC, m_HomeAlignmentHA, m_HomePolarisClock, m_bPolarisHomeAlignmentSet, m_PortName, m_iST4GuideRateIndex);
#endif	
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_ALIGNMENT_CLOCK_POSITION, m_HomePolarisClock);
	m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_DEC, m_HomeAlignmentDEC);
	m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_ALIGNMENT_HA, m_HomeAlignmentHA);
	m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORT_NAME, m_PortName);
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_GUIDERATE, m_iST4GuideRateIndex);
	m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SLEWDELAY, m_iPostSlewDelay);

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
	if (LogFile) fclose(LogFile);
#endif

}

int	X2Mount::queryAbstraction(const char* pszName, void** ppVal)
{
	*ppVal = NULL;

	//if (!strcmp(pszName, SyncMountInterface_Name))
		//*ppVal = dynamic_cast<SyncMountInterface*>(this);
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

	//Add support for the optional LoggerInterface
	/* if (!strcmp(pszName, LoggerInterface_Name)) {
		*ppVal = GetLogger();
	}
	*/

#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] Query Abstraction Called: %s\n", asctime(localtime(&ltime)), pszName);
    }
#endif
	return SB_OK;
}

//OpenLoopMoveInterface
int	X2Mount::startOpenLoopMove(const MountDriverInterface::MoveDir& Dir, const int& nRateIndex)
{
	X2MutexLocker ml(GetMutex());
	m_CurrentRateIndex = nRateIndex;
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] startOpenLoopMove called %d %d\n", asctime(localtime(&ltime)), Dir, nRateIndex);
    }
#endif
	return SkyW.StartOpenSlew(Dir, SlewSpeeds[nRateIndex]);
}
int	X2Mount::endOpenLoopMove(void)
{
#ifdef HEQ5_DEBUG
    if (LogFile){
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] endOpenLoopMove Called\n", asctime(localtime(&ltime)));
    }
#endif
	X2MutexLocker ml(GetMutex());
	return SkyW.SetTrackingRates(true, true, 0.0, 0.0);                    // Starting to track will stop move
}

int X2Mount::rateCountOpenLoopMove(void) const
{
	return NSLEWSPEEDS;
}

int	X2Mount::rateNameFromIndexOpenLoopMove(const int& nZeroBasedIndex, char* pszOut, const int& nOutMaxSize)
{
	snprintf(pszOut, nOutMaxSize, "%s", SlewSpeedNames[nZeroBasedIndex]);
	return SB_OK;
}

int	X2Mount::rateIndexOpenLoopMove(void)
{
	return m_CurrentRateIndex;
}

#pragma mark - UI binding

int X2Mount::execModalSettingsDialog(void)
{
	int nErr = SB_OK;
	X2ModalUIUtil uiutil(this, m_pTheSkyXForMounts);
	X2GUIInterface*					ui = uiutil.X2UI();
	X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
	bool bPressedOK = false;
	int i;

#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] execModelSettingsDialog called %d\n", asctime(localtime(&ltime)), m_bPolarisHomeAlignmentSet);
    }
#endif

	if (NULL == ui) return ERR_POINTER;

	if ((nErr = ui->loadUserInterface("Skywatcher.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
		return nErr;

	if (NULL == (dx = uiutil.X2DX())) {
		return ERR_POINTER;
	}

	// Set values in the userinterface

	dx->setPropertyString("lineEdit", "text", m_PortName);

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
		dx->setEnabled("pushButton_2", true);                 // Move to right polar alignment slot
	}
	else {
		dx->setEnabled("pushButton_2", false);               // Cannot move while parked, not connected or haven't aligned the reticule yet
	}

	if (SkyW.isConnected()) {
		dx->setEnabled("lineEdit", false);                   // Cannot edit the port name while connected.
	}
	if (!SkyW.isConnected() || m_bParked) {
		dx->setEnabled("pushButton", false);                 // Must be conneected and not parked to set polar alignment home location
		dx->setEnabled("comboBox", false);
	}
	if (!SkyW.isConnected()) {
		dx->setPropertyString("label_4", "text", "Disconnected");
	}
	else if (m_bParked) {
		dx->setPropertyString("label_4", "text", "Parked");
	}
	else {
		dx->setPropertyString("label_4", "text", "Unparked");
	}

	//Display the user interface
	if ((nErr = ui->exec(bPressedOK)))
		return nErr;

	//Retreive values from the user interface
	if (bPressedOK)
	{		
#ifdef HEQ5_DEBUG
        if (LogFile) {
            time_t ltime=time(NULL);
            fprintf(LogFile, "[%s] execModealSetting:: Pressed OK button\n", asctime(localtime(&ltime)));
        }
#endif
		dx->propertyString("lineEdit","text",m_PortName, MAX_PORT_NAME_SIZE);
		m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORT_NAME, m_PortName);
		m_iST4GuideRateIndex = dx->currentIndex("comboBox_2");
		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_GUIDERATE, m_iST4GuideRateIndex);
		SkyW.SetST4GuideRate(m_iST4GuideRateIndex);
		dx->propertyInt("spinBox", "value", m_iPostSlewDelay);
		m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SLEWDELAY, m_iPostSlewDelay);

#ifdef HEQ5_DEBUG
        if (LogFile){
            time_t ltime=time(NULL);
            fprintf(LogFile, "[%s] execModealSetting:: Port Name %s\n", asctime(localtime(&ltime)), m_PortName);
        }
#endif
	}
	return nErr;



}

void X2Mount::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
	double HAPolaris, HAOctansSigma;
	double const RAPolaris = 2.879280694;                           // Calculated on 12/2/2017
	double const RAOctansSigma = 21.36480556;						// Calculated on 24/2/2017

	int err;
	X2MutexLocker ml(GetMutex());

#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] uievent %s\n", asctime(localtime(&ltime)), pszEvent);
    }
#endif
	if (!strcmp(pszEvent, "on_timer")) { // Periodical event - use it to check status of slew
		if (!m_bPolarisAlignmentSlew) return; // No need to do anything unless slewing
#ifdef HEQ5_DEBUG
        if (LogFile) {
            time_t ltime=time(NULL);
            fprintf(LogFile, "[%s] uievent %s %d %d\n", asctime(localtime(&ltime)), pszEvent, SkyW.GetIsNotGoto(), m_bPolarisAlignmentSlew);
        }
#endif
		if (!SkyW.GetIsPolarAlignInProgress()) {            // Slewing has finished
			uiex->setPropertyString("pushButton_2", "text", "Move to Alignment Position"); // Change label to indicate action taken
			m_bPolarisAlignmentSlew = false;
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
            time_t ltime=time(NULL);
            fprintf(LogFile, "[%s] pushButton_clicked Index %d Ra %f dec%f\n", asctime(localtime(&ltime)), m_HomePolarisClock, m_HomeAlignmentHA, m_HomeAlignmentDEC);
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
			err = SkyW.PolarAlignment(m_HomeAlignmentHA, m_HomeAlignmentDEC, m_HomePolarisClock, HAPolaris,HAOctansSigma);
			if (err) {
				uiex->messageBox("Error", "Problem getting current HA and DEC. Connection error?");
				return;
			}
			m_bPolarisAlignmentSlew = true;
			uiex->setPropertyString("pushButton_2", "text", "Abort"); // Change label to indicate action taken
			return;
		}
		else {
			err = SkyW.Abort();
			uiex->setPropertyString("pushButton_2", "text", "Move to Alignment Position"); // Change label to indicate action taken
			m_bPolarisAlignmentSlew = false;
			return;
		}
	}

	return;
}

//LinkInterface
int	X2Mount::establishLink(void)					
{
	X2MutexLocker ml(GetMutex()); 
	// get serial port device name
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] Establish Link called\n", asctime(localtime(&ltime)));
    }
#endif
	return SkyW.Connect(m_PortName);
}
int	X2Mount::terminateLink(void)						
{
	X2MutexLocker ml(GetMutex());
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] Terminate Link called\n", asctime(localtime(&ltime)));
    }
#endif
	return SkyW.Disconnect();
}

bool X2Mount::isLinked(void) const					
{
	bool temp;
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] isLinked called\n", asctime(localtime(&ltime)));
    }
#endif

	temp = SkyW.isConnected();
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
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] driverInfoDetailedInfo Called\n", asctime(localtime(&ltime)));
    }
#endif
		str = "EQ Direct X2 plugin by Colin McGill";
}

double	X2Mount::driverInfoVersion(void) const				
{
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] driverInfoVersion Called\n", asctime(localtime(&ltime)));
    }
#endif
	return SKYWATCHER_DRIVER_VERSION;
}

//AbstractDeviceInfo
void X2Mount::deviceInfoNameShort(BasicStringInterface& str) const				
{
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] driverInfoNameShort Called\n", asctime(localtime(&ltime)));
    }
#endif
	str = "Doesn't Seem to be used";
}
void X2Mount::deviceInfoNameLong(BasicStringInterface& str) const				
{
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] deviceInfoNameLong Called\n", asctime(localtime(&ltime)));
    }
#endif
	str = "Skywatcher EQ Mount";

}
void X2Mount::deviceInfoDetailedDescription(BasicStringInterface& str) const	
{
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] driverInfoDetailedDescription Called\n", asctime(localtime(&ltime)));
    }
#endif
	str = "Connects to a Skywatcher equatorial mount through an EQDIR cable - see the EQMOD project for details";
	
}
void X2Mount::deviceInfoFirmwareVersion(BasicStringInterface& str)		
{
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] driverInfoFirmwareVersion Called\n", asctime(localtime(&ltime)));
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
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] driverInfoModel Called\n", asctime(localtime(&ltime)));
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
int	X2Mount::raDec(double& ra, double& dec, const bool& bCached)
{
	int err = 0;
	double Ha = 0.0;
	X2MutexLocker ml(GetMutex());

#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] raDec Called\n", asctime(localtime(&ltime)));
    }
#endif
	// Get the HA and DEC from the mount
	err = SkyW.GetMountHAandDec(Ha, dec);
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
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] raDec Ra: %f Dec %f\n", asctime(localtime(&ltime)), ra, dec);
    }
#endif
	return err;
}

int	X2Mount::abort(void)												
{
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] abort Called\n", asctime(localtime(&ltime)));
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
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] startSlewTo Called %f %f\n", asctime(localtime(&ltime)), dRa, dDec);
    }
#endif
	X2MutexLocker ml(GetMutex());
	if (dDec > 89.8 || dDec < -89.8) { // Special case - assume is parking to Skywatcher home location - catch and send to correct HA
		err = SkyW.StartPark();
	}
	else {
		err = SkyW.StartSlewTo(dRa, dDec);
	}
	m_wasslewing = true;
	return err;
}

int X2Mount::isCompleteSlewTo(bool& bComplete) const
{
	bComplete = SkyW.GetIsNotGoto();

#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] isCompleteSlewTo called %d\n", asctime(localtime(&ltime)), bComplete);
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
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] endSlewTo Called\n", asctime(localtime(&ltime)));
    }
#endif
	return SB_OK;
}


// Next two functions are place holders - decided not to implement
int X2Mount::syncMount(const double& ra, const double& dec)
{
	X2MutexLocker ml(GetMutex());
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] syncMount Called\n", asctime(localtime(&ltime)));
    }
#endif
	m_bSynced = true;
	return SB_OK;
}

bool X2Mount::isSynced(void)
{
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] syncMount Called\n", asctime(localtime(&ltime)));
    }
#endif
	return true;   // As per definition - see X2 Standard for mounts - Mount does not know (or care) if synced.
}

int X2Mount::setTrackingRates(const bool& bTrackingOn, const bool& bIgnoreRates, const double& dRaRateArcSecPerSec, const double& dDecRateArcSecPerSec) 
{
	X2MutexLocker ml(GetMutex());
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] setTrackingRates Called Tracking On %d Ignore Rates%d\n", asctime(localtime(&ltime)), bTrackingOn, bIgnoreRates);
    }
#endif
	return SkyW.SetTrackingRates(bTrackingOn, bIgnoreRates, dRaRateArcSecPerSec, dDecRateArcSecPerSec);

}

int X2Mount::trackingRates(bool& bTrackingOn, double& dRaRateArcSecPerSec, double& dDecRateArcSecPerSec)
{
	X2MutexLocker ml(GetMutex());
	bTrackingOn = false;
	dRaRateArcSecPerSec = -1000.0;
	dDecRateArcSecPerSec = -1000.0;

	return SB_OK;
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
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] isParked Called\n", asctime(localtime(&ltime)));
    }
#endif
	return m_bParked;
}

int X2Mount::startPark(const double& dAz, const double& dAlt)
{
	double dRa, dDec;
	int err = 0;


	X2MutexLocker ml(GetMutex());
	err = m_pTheSkyXForMounts->HzToEq(dAz, dAlt, dRa, dDec); if (err) return err;
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] startPark Called Alt %f Az %f Ra %f Dec %f\n", asctime(localtime(&ltime)), dAz, dAlt, dRa, dDec);
    }
#endif
	if (fabs(dDec - 90.0) < 0.1 || fabs (dDec+90) < 0.1) {
		err = SkyW.StartPark();
#ifdef HEQ5_DEBUG
        if (LogFile) {
            time_t ltime=time(NULL);
            fprintf(LogFile, "[%s] startPark Called SkyW.StartPark() %d\n", asctime(localtime(&ltime)), err);
        }
#endif
	}
	else {
		err = SkyW.StartSlewTo(dRa, dDec);
#ifdef HEQ5_DEBUG
        if (LogFile) {
            time_t ltime=time(NULL);
            fprintf(LogFile, "[%s] startPark Called SkyW.StartSlewTo Err %d Ra %f Dec %f \n", asctime(localtime(&ltime)), err, dRa, dDec);
        }
#endif
	}

	return err;
}


int X2Mount::isCompletePark(bool& bComplete) const
{
#ifdef HEQ5_DEBUG
    if (LogFile) {
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] isCompletePark Called\n", asctime(localtime(&ltime)));
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
        time_t ltime=time(NULL);
        fprintf(LogFile, "[%s] endPark Called\n", asctime(localtime(&ltime)));
    }
#endif
	err = setTrackingRates(false, true, 0.0, 0.0); if (err) return err;
	m_bParked = true;
	return err;
}

int		X2Mount::startUnpark(void)
{
	return SB_OK;
}
/*!Called to monitor the unpark process.  \param bComplete Set to true if the unpark is complete, otherwise set to false.*/
int	X2Mount::isCompleteUnpark(bool& bComplete) const
{
	bComplete = true;
	return SB_OK;
}

/*!Called once the unpark is complete.  This is called once for every corresponding startUnpark() allowing software implementations of unpark.*/
int		X2Mount::endUnpark(void)
{
	m_bParked = false;
	return SB_OK;
}

int X2Mount::beyondThePole(bool& bYes) {
	bYes = SkyW.GetIsWestofPier();
	return SB_OK;
}
double X2Mount::flipHourAngle() {
	return 0.0;
}

int X2Mount::gemLimits(double& dHoursEast, double& dHoursWest)
{
	dHoursEast = 0.0;
	dHoursWest = 0.0;
	return SB_OK;
}
