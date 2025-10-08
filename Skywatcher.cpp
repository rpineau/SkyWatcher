#include "Skywatcher.h"

// Constructor for Skywatcher
Skywatcher::Skywatcher(SerXInterface *pSerX, SleeperInterface *pSleeper, TheSkyXFacadeForDriversInterface *pTSX)
{
#ifdef SB_WIN_BUILD
	int wsa_err;
#endif
    
    m_pSerX = pSerX;
	m_pSleeper = pSleeper;
	m_pTSX = pTSX;
	
	m_bLinked = false;
	IsBeyondThePole = false;
	m_bGotoInProgress = false;
	m_bParkInProgress = false;
	
	m_dDeltaHASteps = 0.0;
	
	MCVersion = 0;
	MountCode = 0;

	// Initially not connected to WiFi
	m_bWiFi = false;
	
	// Set initial value of m_dDeltaHASteps - approximate over correction to get good goto performance
	// Will be updated and refined after each slew.
	m_dDeltaHASteps = 70.0;		// Opposite direction in Northern Hemisphere

#ifdef  SKYW_DEBUG
#if defined(SB_WIN_BUILD)
	sprintf(m_sLogfilePath,"%s%s%s", getenv("HOMEDRIVE"), getenv("HOMEPATH"),"\\Skylog.txt");
#else
	sprintf(m_sLogfilePath, "%s%s", getenv("HOME"), "/Skylog.txt");
#endif

	LogFile = fopen(m_sLogfilePath, "w");
	// Turn off buffering for the log file
	setbuf(LogFile, NULL);

	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
#ifdef SB_LINUX_BUILD
	fprintf(LogFile, "[%s] SkyW New Constructor Called Home: %s\n", timestamp, getenv("HOME"));
#endif
	fprintf(LogFile, "[%s] SkyW New Constructor Called latitude %f\n", timestamp, m_pTSX->latitude());
#endif

#ifdef SB_WIN_BUILD
	// Required to start up Winsock.
	wsa_err = WSAStartup(MAKEWORD(2, 2), &wsadata);
#ifdef  SKYW_DEBUG
	fprintf(LogFile, "[%s] SkyW Constructor WSAERR %d \n", timestamp, wsa_err);
#endif
#endif

#if defined SKYW_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(LogFile, "[%s] [Skywatcher::Skywatcher] version %3.2f build 2020_05_21_12_45.\n", timestamp, SKYWATCHER_DRIVER_VERSION);
    fflush(LogFile);
#endif

}


Skywatcher::~Skywatcher(void)
{
#ifdef SKYW_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(LogFile, "[%s] SkyW Destructor Called\n", asctime( localtime(&ltime) ) );
    // Close LogFile
    if (LogFile) fclose(LogFile);
#endif

#ifdef SB_WIN_BUILD
	// End Windsock
    WSACleanup();
#endif

}


void Skywatcher::SetConnectionData(char *serialportname, char *IPAddress, int port, bool wifi)
{
  if (wifi) {
    strncpy(m_cIPAddress, IPAddress, sizeof(m_cIPAddress));
    m_iWiFiPortNumber = port;
    m_bWiFi = wifi;
  }else {
    strncpy(m_cPortname, serialportname, sizeof(m_cPortname));
    m_bWiFi = wifi;
  }
#ifdef SKYW_DEBUG
  ltime = time(NULL);
  timestamp = asctime(localtime(&ltime));
  timestamp[strlen(timestamp) - 1] = 0;
  fprintf(LogFile, "[%s] SkyW::SetConnectionData Called port %s ipaddress %s port %d wifi %d\n", asctime(localtime(&ltime)), serialportname, IPAddress, port, wifi);
#endif
}
    
int Skywatcher::WiFiCheck(void)
{

  int err = 0;
  int oldblinked = m_bLinked;
  // IF WiFi not enabled, return error
  if (!m_bWiFi) return ERR_COMMOPENING;

#ifdef	SKYW_DEBUG
  ltime = time(NULL);
  timestamp = asctime(localtime(&ltime));
  timestamp[strlen(timestamp) - 1] = 0;
  fprintf(LogFile, "[%s] WiFiCheck: Entered\n", timestamp);
#endif

#if defined SB_LINUX_BUILD || defined SB_WIN_BUILD || defined SB_MAC_BUILD // Lets try all three for now
  /* socket: create the socket */
  struct hostent *server;
  m_isockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  server = gethostbyname(m_cIPAddress);

#ifdef	SKYW_DEBUG
  ltime = time(NULL);
  timestamp = asctime(localtime(&ltime));
  timestamp[strlen(timestamp) - 1] = 0;
  fprintf(LogFile, "[%s] WiFiCheck: sockid %d\n", timestamp, m_isockfd);
#endif

  if (m_isockfd < 0 || !server) return ERR_COMMOPENING;

  /* build the server's Internet address */
  // First initialise m_serveraddr
  memset(&m_serveraddr, 0, sizeof(m_serveraddr));

  m_serveraddr.sin_family = AF_INET;
  memcpy(&m_serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
  m_serveraddr.sin_port = htons(m_iWiFiPortNumber);
  m_iserverlen = sizeof(m_serveraddr);

  /* Now attempt to read the mount name */
  m_bLinked = true; // Without this, connect will be called by SendSkyWatcherCommand

#ifdef	SKYW_DEBUG
  ltime = time(NULL);
  timestamp = asctime(localtime(&ltime));
  timestamp[strlen(timestamp) - 1] = 0;
  fprintf(LogFile, "[%s] WiFiCheck: About to call ReadMountData()\n", timestamp);
#endif

  err = ReadMountData(); if (err) {
    m_bLinked = false;
    m_bWiFi = false;
    return err;
  }

  /* Now restore m_bLinked to the previous value since have got the data we need */
  m_bLinked = oldblinked;

#ifdef	SKYW_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(LogFile, "[%s] WiFiCheck: MountName %s\n", timestamp, MountName);

#endif

  return SB_OK;
#else
  return ERR_COMMOPENING;
#endif
}
  
int Skywatcher::Connect(void)
{
	int err;
	char response[SKYWATCHER_MAX_CMD];
	char command[SKYWATCHER_MAX_CMD];
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::Connect Called %s using %d bps wifi %d\n", timestamp, m_cPortname, m_iBaudRate, m_bWiFi);
#endif
	
	if (m_bWiFi) {
	  // Use WiFiCheck to set wifi port and check it can be opened
	  err = WiFiCheck(); if (err) return err;
	  m_bLinked = true;
	  // WiFiCheck also reads the mount data, so no need to repeat that here.
	} else {
	  // Try to connect at stored baud rate first
	  if(m_pSerX->open(m_cPortname, m_iBaudRate, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0) {
	    m_bLinked = true;
#ifdef SKYW_DEBUG
	    ltime = time(NULL);
	    timestamp = asctime(localtime(&ltime));
	    timestamp[strlen(timestamp) - 1] = 0;
	    fprintf(LogFile, "[%s] Skyw::Connect opened %s\n", timestamp, m_cPortname);
#endif
	  }
	  else {
#ifdef SKYW_DEBUG
	    ltime = time(NULL);
	    timestamp = asctime(localtime(&ltime));
	    timestamp[strlen(timestamp) - 1] = 0;
	    fprintf(LogFile, "[%s] Skyw::Connect did not open %s\n", timestamp, m_cPortname);
#endif
	    m_bLinked = false;
	  }
	  if (!m_bLinked) return ERR_COMMOPENING;
	  
      // m_cmdDelayTimer.Reset();
	  err = ReadMountData();
	  
	  //Error reading the Mount Data, try to connect using alterntive baud rate
	  if (err) {
	    m_pSerX->close();			// Close serial port prior to trying to reopen
	    if (m_iBaudRate == 9600) {
	      m_iBaudRate = 115200;
	    }
	    else {
	      m_iBaudRate = 9600;
	    }
	    
#ifdef SKYW_DEBUG
	    ltime = time(NULL);
	    timestamp = asctime(localtime(&ltime));
	    timestamp[strlen(timestamp) - 1] = 0;
	    fprintf(LogFile, "[%s] Skyw::Connect Called %s using %d bps\n", timestamp, m_cPortname, m_iBaudRate);
#endif
	    if (m_pSerX->open(m_cPortname, m_iBaudRate, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0) {
	      m_bLinked = true;
#ifdef SKYW_DEBUG
	      ltime = time(NULL);
	      timestamp = asctime(localtime(&ltime));
	      timestamp[strlen(timestamp) - 1] = 0;
	      fprintf(LogFile, "[%s] Skyw::Connect opened %s\n", timestamp, m_cPortname);
#endif
	    }
	    else {
#ifdef SKYW_DEBUG
	      ltime = time(NULL);
	      timestamp = asctime(localtime(&ltime));
	      timestamp[strlen(timestamp) - 1] = 0;
	      fprintf(LogFile, "[%s] Skyw::Connect did not open %s\n", timestamp, m_cPortname);
#endif
	      m_bLinked = false;
	    }
	    if (!m_bLinked) return ERR_COMMOPENING;
	    err = ReadMountData();

	  }
	
	  //Error reading the Mount Data. Close serial port and set to unlinked then return error. Restore baud rate to initial value
	  if (err) {
	    m_bLinked = false;
	    m_pSerX->close();
	    if (m_iBaudRate == 9600) {
	      m_iBaudRate = 115200;
	    }
	    else {
	      m_iBaudRate = 9600;
	    }
	    return err;
	  }
	}

	// If the mount has encoders, turn them off
	if (m_bSupportDualEncoder) {
#ifdef SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::Connect About to turn off encoders\n", timestamp);
#endif
		long2Revu24str(ENCODER_OFF_CMD, command);
		err = SendSkywatcherCommand(SetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
#ifdef SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::Connect Encoders off Axis 1: %s\n", timestamp, response);
#endif
		err = SendSkywatcherCommand(SetFeatureCmd, Axis2, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
#ifdef SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::Connect Encoders off Axis 2: %s\n", timestamp, response);
#endif
	}

	// If mount supports PEC but PEC is not running, try and turn it on. Will set flag if PEC data not valid
	err = TurnOnPec(); if (err) return err;

	// Now initialise motors in axes - good for astronomer to hear the motors come on!
	err = SendSkywatcherCommand(Initialize, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	err = SendSkywatcherCommand(Initialize, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;

	if (m_ST4GuideRate >= 0 && m_ST4GuideRate < 10) {
		SetST4GuideRate(m_ST4GuideRate);
	}
	
	// See if Mount is motion. If so, assume that this is tracking and set rates.
	// If not in motion, then set rates to zero.
	err = GetAxesStatus(); if (err) return err;
	if (AxisStatus[RA].motionmode == STOPPED) {
		err = SetTrackingRates(false, true, 0.0, 0.0); if (err) return err;
	}
	else if (AxisStatus[RA].motionmode != GOTO) {
		err = SetTrackingRates(true, true, 0.0, 0.0); if (err) return err;
	}

	// Set flat to indicate whether north or south latitude
	NorthHemisphere = (m_pTSX->latitude() > 0);
#ifdef	SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Connect: latitude %f %d\n", timestamp, m_pTSX->latitude(), NorthHemisphere);
#endif
	return err;
}

int Skywatcher::StartPecTraining()
{
	int err;
	char response[SKYWATCHER_MAX_CMD];
	char command[SKYWATCHER_MAX_CMD];
	long2Revu24str(START_PPEC_TRAINING_CMD, command);
	err = SendSkywatcherCommand(SetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

#ifdef	SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] StartPecTraining: response %s\n", timestamp, response);
#endif

	return err;
}

int Skywatcher::CancelPecTraining()
{
	int err;
	char response[SKYWATCHER_MAX_CMD];
	char command[SKYWATCHER_MAX_CMD];
	long2Revu24str(STOP_PPEC_TRAINING_CMD, command);
	err = SendSkywatcherCommand(SetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

#ifdef	SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] StopPecTraining: response %s\n", timestamp, response);
#endif

	return err;
}
int Skywatcher::TurnOnPec() 
{
	int err;
	char response[SKYWATCHER_MAX_CMD];
	char command[SKYWATCHER_MAX_CMD];

	// Cannot read extended commands - 
	if (!m_bExtendedCmds) {
		m_bValidPECData = false;
		m_bPECTrackingOn = false;
		return SB_OK;
	}

	long2Revu24str(TURN_PPEC_ON_CMD, command);
	err = SendSkywatcherCommand(SetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

#ifdef	SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] TurnOnPec: response %s\n", timestamp, response);
#endif
	
	if (response[0] == '!') {
		m_bValidPECData = false;
		long2Revu24str(TURN_PPEC_OFF_CMD, command);
		err = SendSkywatcherCommand(SetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD);
		m_bPECTrackingOn = false;
#ifdef	SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] TurnOnPec - post failure, turning off PEC: response %s\n", timestamp, response);
#endif
		return err;
	}

	// Set flag to say tracaking on
	m_bValidPECData = true;
	m_bPECTrackingOn = true;

#ifdef	SKYW_DEBUG
	long2Revu24str(INQUIRE_STATUS, command);
	err = SendSkywatcherCommand(GetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Post turning on Pec Extended Status: response %s\n", timestamp, response);
#endif

	return err;
}

int Skywatcher::TurnOffPec() 
{
	int err;
	char response[SKYWATCHER_MAX_CMD];
	char command[SKYWATCHER_MAX_CMD];
	if (!m_bExtendedCmds) {
		m_bPECTrackingOn = false;
		return SB_OK;
	}

	long2Revu24str(TURN_PPEC_OFF_CMD, command);
	err = SendSkywatcherCommand(SetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

	// Set flag to say tracking off
	m_bPECTrackingOn = false;

#ifdef	SKYW_DEBUG
	long2Revu24str(INQUIRE_STATUS, command);
	err = SendSkywatcherCommand(GetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Post turning off Pec Extended Status: response %s\n", timestamp, response);
#endif

	return err;
}

bool Skywatcher::GetIsPecTrainingOn(void)
{
	int decode;
	char response[SKYWATCHER_MAX_CMD];
	char command[SKYWATCHER_MAX_CMD];


	// Attempt to read the extended mount status
	long2Revu24str(INQUIRE_STATUS, command);
	SendSkywatcherCommand(GetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD);

	// First test to see if the extended data set gives reponse or error. If error, cannot be read.
	if (response[0] == '=') {
		decode = HEX(response[1]);                              // Turn second character of response into a number
		m_bPECTrainingOn = decode & 0x1;                        // First bit of response
	}
	else {
		m_bPECTrainingOn = false;
	}

#ifdef	SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s]GetIsPecTrainingOn: reponse %s Training on %d\n", timestamp, response, m_bPECTrainingOn);
#endif

	return m_bPECTrainingOn;
}

int Skywatcher::SetPolarScopeIllumination(int Brightness)
{
	int err;
	char cmdarg[SKYWATCHER_MAX_CMD], response[SKYWATCHER_MAX_CMD];
	long2Revu8str(Brightness, cmdarg);

	// Do not try and set the illumination if no scope is present.
	if (!m_bPolarScope) return SB_OK;

	err = SendSkywatcherCommand(SetPolarScopeLEDBrightness, Axis1, cmdarg, response, SKYWATCHER_MAX_CMD);
#ifdef	SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Connect: SetPolerScopeIllumination Brightness %d cmdarg %s response %s err %d\n", timestamp, Brightness, cmdarg, response, err);
#endif
	return err;
}

int Skywatcher::SetST4GuideRate(int m_GuideRateIndex)
{
	int err = SB_OK;
	char cmdargs[SKYWATCHER_MAX_CMD], response[SKYWATCHER_MAX_CMD];
	m_ST4GuideRate = m_GuideRateIndex;
	
	if (m_bLinked) {
		sprintf(cmdargs, "%d", m_ST4GuideRate);
		err = SendSkywatcherCommand(SetGuideRate, Axis1, cmdargs, response, SKYWATCHER_MAX_CMD); if (err) return err;
		err = SendSkywatcherCommand(SetGuideRate, Axis2, cmdargs, response, SKYWATCHER_MAX_CMD); if (err) return err;
	}
	return err;
}

int Skywatcher::Disconnect(void)
{
	char response[SKYWATCHER_MAX_CMD];
	int err;
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::Disconnect Called\n", timestamp);
#endif

	if (m_bLinked) {
		// First stop motors
		err = SendSkywatcherCommand(NotInstantAxisStop, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
		err = SendSkywatcherCommand(NotInstantAxisStop, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;

		// Now clear buffers and disconnect the serial port
		if (!m_bWiFi) {
		  m_pSerX->flushTx();
		  m_pSerX->purgeTxRx();
		  m_pSerX->close();
		}
	}
	m_bLinked = false;
	return SB_OK;
}

int Skywatcher::Abort(void)
{
	char response[SKYWATCHER_MAX_CMD];
	int err;
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::Abort Called\n", timestamp);
#endif
	
	// Abandon goto or park if in progress to make sure iterative goto does not start it again!
	m_bGotoInProgress = false;
	m_bParkInProgress = false;
	// Stop movement in each axis
	err = SendSkywatcherCommand(NotInstantAxisStop, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	err = SendSkywatcherCommand(NotInstantAxisStop, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
	// Set tracking rates to be off
	m_bTracking = false;
	m_dRATrackingRate = 15.0410681; // Convention to say tracking is off - see TSX documentation
	m_dDETrackingRate = 0.0;

	return err;
}

int Skywatcher::StopAxesandWait(void)
{
	int err = 0, count = 0;
	unsigned long currentRAStep, currentDEStep;
	char response[SKYWATCHER_MAX_CMD];

	// First, stop both axes
	err = SendSkywatcherCommand(NotInstantAxisStop, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	err = SendSkywatcherCommand(NotInstantAxisStop, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
	//
	do {
		if (count > 0) m_pSleeper->sleep(100);	//Sleep for 100 milliseconds if not stopped
		err = GetAxesStatus(); if (err) return err;
		count++;
	} while (AxisStatus[RA].motionmode != STOPPED || AxisStatus[DEC].motionmode != STOPPED);
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StopAxesandWait Called count %d\n", timestamp, count-1);
#endif
	
	// Check that they are actually stopped!
	count = 0;
	do {
		if (count > 0) m_pSleeper->sleep(100);	//Sleep for 100 milliseconds if not stopped
		currentRAStep = RAStep;
		currentDEStep = DEStep;
		err = InquireMountAxisStepPositions(); if (err) return err;
		count++;
	} while (currentRAStep != RAStep || currentDEStep != DEStep);
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StopAxesandWait motors moving count %d\n", timestamp, count - 1);
#endif
	
	return err;
}

int Skywatcher::StopAxisandWait(SkywatcherAxis Axis)
{
	int err = 0, count = 0;
	SkywatcherAxisStatus CurrentAxisStatus;
	char response[SKYWATCHER_MAX_CMD];
	
	// First, stop axis
	err = SendSkywatcherCommand(NotInstantAxisStop, Axis, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
	//
	do {
		if (count > 0) m_pSleeper->sleep(100);	//Sleep for 100 milliseconds if not stopped
		err = GetAxisStatus(Axis, CurrentAxisStatus); if (err) return err;
		count++;
	} while (CurrentAxisStatus.motionmode != STOPPED);
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StopAxisandWait count %d\n", timestamp, count - 1);
#endif
	
	return err;
}


// End open Slew - to try and make as efficient as possible, avoid non-requred calls to mount
int Skywatcher::EndOpenSlew(void)
{
    int err;
    double RARate, DECRate;
    char command[SKYWATCHER_MAX_CMD], response[SKYWATCHER_MAX_CMD];
    SkywatcherDirection Direction = FORWARD;
    int Speedmode = 1; // Low speed mode since have restricted slew rates to siderial or below.
    unsigned long Period;
    
    Direction = NorthHemisphere ? FORWARD : BACKWARD; // Tracking is backwards in Southern hemisphere
    RARate = SKYWATCHER_SIDEREAL_SPEED;
    DECRate = 0.0;
    
    // Save tracking rates for TSX interface - no difference from Siderial speed since rates ignored
    m_bTracking = true;
    m_dRATrackingRate = 0.0;
    m_dDETrackingRate = 0.0;
    
    /*
    err = SetTrackingRateAxis(Axis1, RARate, RASteps360, RAInteruptFreq, RAHighspeedRatio);
    if (err) return err;
    err = SetTrackingRateAxis(Axis2, DECRate, DESteps360, DEInteruptFreq, DEHighspeedRatio);
    return err;
    */
    
    snprintf(command, SKYWATCHER_MAX_CMD, "%d%d", Speedmode, Direction);
    err = SendSkywatcherCommand(SetMotionMode, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

    // Calulate interupt period for slew rate - see SKYWATCHER basic api code for details
    Period = (unsigned long) ((double) RAInteruptFreq / (double) RASteps360*360.0*3600.0 / RARate );
    
    // Set AxisPeriod
    long2Revu24str(Period, command);    // Convert period into string
    err = SendSkywatcherCommand(SetStepPeriod, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

    // Start axis moving
    err = SendSkywatcherCommand(StartMotion, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;

    // Now stop DEC axis moving.
    err = SendSkywatcherCommand(InstantAxisStop, Axis2, NULL, response, SKYWATCHER_MAX_CMD);

    return err;
}

int Skywatcher::SetTrackingRates(const bool& bTrackingOn, const bool& bIgnoreRates, const double& dRaRateArcSecPerSec, const double& dDecRateArcSecPerSec)
{
	// dRaRateArcSecPerSec and dDecRateArcSecPerSec contain the rate of change of RA and DEC of the object in arcsec/sec.
	// To turn this into tracking rates:
	// RA:	since HA = LST - RA, must subtract this from Sidereal speed
	// DEC: If pre-meridian, increasing DEC means increasing steps, so sign should be the same as dDecRateArcSecPerSec.
	//		If post-medidian, increasing DEC decreases steps, so this should be the opposite sign.
	// We are pre-meridian if DEStep < DEStepInit, otherwise post-meridian
	
	int err = InquireMountAxisStepPositions(); if (err) return err;	 // Get Axis Positions in class members RAStep and DEStep
	double RARate, DECRate;

#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::SetTrackingRates Called bTrackingOn %d %f %f %f\n", timestamp, bTrackingOn, dRaRateArcSecPerSec, dDecRateArcSecPerSec, SKYWATCHER_SIDEREAL_SPEED);
#endif
	
	if (bTrackingOn) { // set tracking
		if (bIgnoreRates) { // No movement in DEC and siderial for RA
			RARate = NorthHemisphere ? SKYWATCHER_SIDEREAL_SPEED : -SKYWATCHER_SIDEREAL_SPEED;
			DECRate = 0.0;
			// Now save tracking rates for TSX interface - no difference from Siderial speed since rates ignored
			m_bTracking = true;
			m_dRATrackingRate = 0.0;
			m_dDETrackingRate = 0.0;
		}
		else {
			if (NorthHemisphere) {
				RARate = SKYWATCHER_SIDEREAL_SPEED - dRaRateArcSecPerSec;
				DECRate = (DEStep < DEStepInit ? dDecRateArcSecPerSec : -dDecRateArcSecPerSec);
			}
			else {
				RARate = -(SKYWATCHER_SIDEREAL_SPEED - dRaRateArcSecPerSec);
				DECRate = (DEStep > DEStepInit ? dDecRateArcSecPerSec : -dDecRateArcSecPerSec);
			}
			// Now save tracking rates for TSX interface - must capture rates
			m_bTracking = true;
			m_dRATrackingRate = dRaRateArcSecPerSec;
			m_dDETrackingRate = dDecRateArcSecPerSec;
		}
	}
	else {
		// Tracking is off
		RARate = 0.0;
		DECRate = 0.0;
		m_bTracking = false;
		m_dRATrackingRate = 15.0410681; // Convention to say tracking is off - see TSX documentation
		m_dDETrackingRate = 0;
	}
	

	err = SetTrackingRateAxis(Axis1, RARate, RASteps360, RAInteruptFreq, RAHighspeedRatio);
	if (err) return err;
	err = SetTrackingRateAxis(Axis2, DECRate, DESteps360, DEInteruptFreq, DEHighspeedRatio);
	return err;
    
    
}

int Skywatcher::SetTrackingRateAxis(SkywatcherAxis Axis, double Rate, unsigned long Steps360, unsigned long InteruptFrequency, unsigned long HighspeedRatio)
{
	char command[SKYWATCHER_MAX_CMD], response[SKYWATCHER_MAX_CMD];
	SkywatcherDirection Direction = FORWARD;
	SkywatcherAxisStatus CurrentAxisStatus;
	int Speedmode;
	unsigned long Period;
	int err = 0;

#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::SetTrackingRateAxis Called Axis %c %f %lu %lu %lu\n", timestamp, Axis, Rate, Steps360, InteruptFrequency, HighspeedRatio);
#endif
	
	if (Rate < 0) {
		Direction = BACKWARD;
		Rate = -Rate;
	}
	
	// Limit maximum speed - input is in arcsec per sec
	if (Rate > SKYWATCHER_SIDEREAL_SPEED * SKYWATCHER_MAXSLEW_RATE) {
		Rate = SKYWATCHER_SIDEREAL_SPEED * SKYWATCHER_MAXSLEW_RATE;
	}
	
	if (Rate < SKYWATCHER_SIDEREAL_SPEED * SKYWATCHER_MINSLEW_RATE) { // Rate effectively zero - stop axis
		err = SendSkywatcherCommand(NotInstantAxisStop, Axis, NULL, response, SKYWATCHER_MAX_CMD);
#ifdef SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::SetTrackingRateAxis NotInstantAxisStop %d %s\n", timestamp, err, response);
#endif
		return err;
	}
	
	if (Rate < SKYWATCHER_SIDEREAL_SPEED * SKYWATCHER_LOWSPEED_RATE) {
		Speedmode = 1;			// Lowspeed Slew
	} else {
		Speedmode = 3;			// Highspeed slewing mode is mode 3;
		Rate = Rate / (double)HighspeedRatio; // See Skywatcher Basic API code for details
	}
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::SetTrackingRateAxis Speedmode %d Direction %d\n", timestamp, Speedmode, Direction);
#endif
	
	// Get axis status and determine if need to stop axis
	err = GetAxisStatus(Axis, CurrentAxisStatus); if (err) return err;
	
	// Determine if need to stop and set motion mode
	if (CurrentAxisStatus.motionmode == GOTO ||							// Need to stop if in GOTO (though should not be here in GOTO mode!)
		CurrentAxisStatus.speedmode == HIGHSPEED ||						// Or if slewing at high speed
		Speedmode == 3 ||												// Or if next move is at high speed
		CurrentAxisStatus.direction != Direction ||						// Or a change of direction
		CurrentAxisStatus.motionmode == STOPPED)  {						// Don't need to stop axis, but need to set direction mode
		
		// Stop axis if needed
		if (CurrentAxisStatus.motionmode != STOPPED) { err = StopAxisandWait(Axis); if (err) return err; }
		
		// Tell mount to do slew with speed and direction
		sprintf(command, "%d%d", Speedmode, Direction);
		err = SendSkywatcherCommand(SetMotionMode, Axis, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
	}
	
	
	// Calulate interupt period for slew rate - see SKYWATCHER basic api code for details
	Period = (unsigned long) ((double) InteruptFrequency / (double) Steps360*360.0*3600.0 / Rate );
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::SetTrackingRateAxis Period %lu Steps360 %lu InteruptFrequency %lu\n", timestamp, Period, Steps360, InteruptFrequency);
#endif
	
	// Set AxisPeriod
	long2Revu24str(Period, command);	// Convert period into string
	err = SendSkywatcherCommand(SetStepPeriod, Axis, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
	// Start axis moving
	err = SendSkywatcherCommand(StartMotion, Axis, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
	return err;
}

int Skywatcher::GetAxesStatus(void)
{
	int err;
	
	err = GetAxisStatus(Axis1, AxisStatus[RA]); if (err) return err;
	err = GetAxisStatus(Axis2, AxisStatus[DEC]); if (err) return err;
	
	// Set flag to indicate whether under a GOTO or not.
	IsNotGoto = (AxisStatus[RA].motionmode != GOTO && AxisStatus[DEC].motionmode != GOTO);
	
	//Set flag to indicate if beyond the pole
	IsBeyondThePole = NorthHemisphere ? (DEStep < DEStepInit): (DEStep > DEStepInit);
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::GetAxisStaus RAMotion %d DecMotion %d IsNotGoto %d\n", timestamp, AxisStatus[RA].motionmode, AxisStatus[DEC].motionmode, IsNotGoto);
#endif
	
	return err;
	
}

int Skywatcher::GetAxisStatus(SkywatcherAxis Axis, SkywatcherAxisStatus &AxisStatus)
{
	char response[SKYWATCHER_MAX_CMD];
	int err;
	err = SendSkywatcherCommand(InquireAxisStatus, Axis, NULL, response, SKYWATCHER_MAX_CMD);
	if (err) return err;
	
	// See Skywatcher Basic API for details of the code below
	if ((response[2] & 0x01) != 0) { // Axis is running
		if ((response[1] & 0x01) != 0) { // Slewing status
			AxisStatus.motionmode = SLEW;
		}
		else {
			AxisStatus.motionmode = GOTO;
		}
	}
	else {
		AxisStatus.motionmode = STOPPED;
	}
	
	if ((response[1] & 0x02) == 0) { // Direction is positive
		AxisStatus.direction = FORWARD;
	}
	else {
		AxisStatus.direction = BACKWARD;
	}
	
	if ((response[1] & 0x04) != 0) { // Highspeed Mode
		AxisStatus.speedmode = HIGHSPEED;
	}
	else {
		AxisStatus.speedmode = LOWSPEED;
	}
	
	if ((response[3] & 1) == 0) { // Direction is positive
		AxisStatus.initialised = NOTINITIALISED;
	}
	else {
		AxisStatus.initialised = INITIALISED;
	}
	
	return err;
	
}
int Skywatcher::GetMountHAandDec(double& dHa, double& dDec)
{
	double HANow;				 // Current HA of target
	double DeltaHA;				 // Used to determine how close to the goto location
	double DeltaHAPostTracking;
	int err = InquireMountAxisStepPositions(); // Get Axis Positions in class members RAStep and DEStep
	if (err) return err;

    // Now convert to HA and Dec
	HAandDECfromEncoderValues(RAStep, DEStep, dHa, dDec);
	
	// Also take opportunity to update Mount Axes Status
	err = GetAxesStatus();
	
	// Was parking on going and has it stopped? If so, signal parking has stopped and cancel goto
	if (m_bParkInProgress && IsNotGoto) {
		m_bParkInProgress = false;
		m_bGotoInProgress = false;
#ifdef SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::GetMountHAandDec - about to stop parking\n", timestamp);
#endif
		err = SetTrackingRates(false, true, 0.0, 0.0); if (err) return err;
	}
	
	// Iterative goto. If goto was ongoing, but motors have now stopped, see how close to target RA (DEC should be exact)
	if (m_bGotoInProgress && IsNotGoto) {

		/* Goto has finished. Astro EQ mounts seem to need a :G send at the end of a slew according to the INDI code */
		/* ResetMotions does this */
		err = ResetMotions(); if (err) return err;

		HANow = m_pTSX->hourAngle(m_dGotoRATarget);
		DeltaHA = (dHa - HANow)*15.0*3600.0;	  // Delta in arcsec
		
		// Iterate towards the desired HA & DEC unless a newer mount - seems to cause problems.
		if (abs(DeltaHA) > SKYWATCHER_GOTO_ERROR && m_iGotoIterations < SKYWATCHER_MAX_GOTO_ITERATIONS) { // If error bigger than should be iterate again;

			// First confirm that the axes have stopped
			err = StopAxesandWait(); if (err) return err;
			err = StartSlewTo(m_dGotoRATarget, m_dGotoDECTarget,0.0);

#ifdef SKYW_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] Skyw::GetMountHAandDec m_iGotoIterations %d DeltaHA %f\n", timestamp, m_iGotoIterations, DeltaHA);
#endif
		}
		else {	// Close enough - stop goto and start tracking
#ifdef SKYW_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] Skyw::GetMountHAandDec - about to start tracking\n", timestamp);
#endif
	
			err = SetTrackingRates(true, true, 0.0, 0.0); if (err) return err;
			m_bGotoInProgress = false;
			
			// Now tracking has started, update positions and save error - takes a little time to start mount.
			err = InquireMountAxisStepPositions();
			// Now convert to HA and Dec
			HAandDECfromEncoderValues(RAStep, DEStep, dHa, dDec);
			DeltaHAPostTracking = (dHa - m_pTSX->hourAngle(m_dGotoRATarget))*RASteps360 / 24.0;
			
			// Now save DeltaHA to improve next slew - add to existing step error
			m_dDeltaHASteps -= DeltaHAPostTracking;
			
#ifdef SKYW_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] Skyw::GetAxisStaus Goto Succeeded %d DeltaHA arc sec %f Steps %f\n", timestamp, m_iGotoIterations, DeltaHAPostTracking*360.0*3600.0 / (double)RASteps360, m_dDeltaHASteps);
#endif
		}
	}
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::GetMountHAandDec called %lu %lu %f %f\n", timestamp, RAStep, DEStep, dHa, dDec);
#endif
	
	return err;
}


void Skywatcher::EncoderValuesfromHAanDEC(double dHa, double dDec, unsigned long &RAEncoder, unsigned long &DEEncoder, bool bUseBTP)
{
	// If bUseBTP is true, take value from current state of BTP to determine which side of the merdian we are on - only used to sync the mount
	if (NorthHemisphere) {
		if ((bUseBTP && IsBeyondThePole) || (!bUseBTP && dHa < m_dFlipHour)) {	 //	 Pre-Meridian
			DEEncoder = (int)(DEStepInit + (dDec - 90.0)*DESteps360 / 360.0);
			RAEncoder = (int)(RAStepInit + (dHa + 6.0)*RASteps360 / 24.0);
		}
		else {	// Post-Meridian
			DEEncoder = (int)(DEStepInit - (dDec - 90.0)*DESteps360 / 360.0);
			RAEncoder = (int)(RAStepInit + (dHa - 6.0)*RASteps360 / 24.0);
		}
	}
	else {
		if ((bUseBTP && IsBeyondThePole) || (!bUseBTP && dHa < m_dFlipHour)) {	 //	 Pre-Meridian
			DEEncoder = (int)(DEStepInit + (dDec + 90.0)*DESteps360 / 360.0);
			RAEncoder = (int)(RAStepInit - (dHa + 6.0)*RASteps360 / 24.0);
		}
		else {	// Post-Meridian
			DEEncoder = (int)(DEStepInit - (dDec + 90.0)*DESteps360 / 360.0);
			RAEncoder = (int)(RAStepInit - (dHa - 6.0)*RASteps360 / 24.0);
		}
	}
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::EncodervaluefromHAandDec called %lu %lu %f %f bUseBTP %d IsBeyondThePole %d\n", timestamp, RAEncoder, DEEncoder,  dHa, dDec, bUseBTP, IsBeyondThePole);
#endif
}

void Skywatcher::HAandDECfromEncoderValues(unsigned long RAEncoder, unsigned long DEEncoder, double &dHa, double &dDec)
{
	// Convert from encoder values
	if (NorthHemisphere) {
		if (DEEncoder < DEStepInit) {	  //  Pre-meridian
			dDec = 90.0 - (DEStepInit*1.0 - DEEncoder*1.0) / (DESteps360*1.0)*360.0;
			dHa = -6.0 + (RAEncoder*1.0 - RAStepInit*1.0) / (RASteps360*1.0)*24.0;
		}
		else {
			dDec = 90.0 + (DEStepInit*1.0 - DEEncoder*1.0) / (DESteps360*1.0)*360.0;
			dHa = 6.0 + (RAEncoder*1.0 - RAStepInit*1.0) / (RASteps360*1.0)*24.0;
		}
	}
	else {
		if (DEEncoder > DEStepInit) {	  //  Pre-meridian
			dDec = -90.0 - (DEStepInit*1.0 - DEEncoder*1.0) / (DESteps360*1.0)*360.0;
			dHa = -6.0 - (RAEncoder*1.0 - RAStepInit*1.0) / (RASteps360*1.0)*24.0;
		}
		else {
			dDec = -90.0 + (DEStepInit*1.0 - DEEncoder*1.0) / (DESteps360*1.0)*360.0;
			dHa = 6.0 - (RAEncoder*1.0 - RAStepInit*1.0) / (RASteps360*1.0)*24.0;
		}
		
	}
}

//Next is utility function since not in math.h in unix
long Skywatcher::abs(long value)
{
	if (value > 0.0) {
		return value;
	} else {
		return -value;
	}
}

int Skywatcher::StartOpenSlew(const MountDriverInterface::MoveDir &		Dir, double rate)
{
	int err;
	double RARate = SKYWATCHER_SIDEREAL_SPEED;
	double DECRate = 0.0;
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StartOpenSlew called direction %d rate %f\n", timestamp, Dir, rate);
#endif
	// Diretions are tuned to match jog so that can have same reflection settings.
	if ((NorthHemisphere && (DEStep < DEStepInit)) || (!NorthHemisphere && (DEStep > DEStepInit))) {	// Pre-Meridian
#ifdef SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::StartOpenSlew called Pre-Meridian: NorthHemisphere %d DESSTEP %lu DESTEPINIT %lu\n", timestamp, NorthHemisphere, DEStep, DEStepInit);
#endif
		switch (Dir) {
			case MountDriverInterface::MD_NORTH:;
				DECRate += rate*SKYWATCHER_SIDEREAL_SPEED;
				break;
			case MountDriverInterface::MD_SOUTH:
				DECRate -= rate*SKYWATCHER_SIDEREAL_SPEED;
				break;
			case MountDriverInterface::MD_WEST:
				RARate -= rate * SKYWATCHER_SIDEREAL_SPEED;
				break;
			case MountDriverInterface::MD_EAST:
				RARate += rate * SKYWATCHER_SIDEREAL_SPEED;
				break;
		}
	}
	else {				 // Post-Meridian
#ifdef SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::StartOpenSlew called Post-Meridian: NorthHemisphere %d DESSTEP %lu DESTEPINIT %lu\n", timestamp, NorthHemisphere, DEStep, DEStepInit);
#endif
		switch (Dir) {
			case MountDriverInterface::MD_NORTH:;
				DECRate -= rate*SKYWATCHER_SIDEREAL_SPEED;
				break;
			case MountDriverInterface::MD_SOUTH:
				DECRate += rate*SKYWATCHER_SIDEREAL_SPEED;
				break;
			case MountDriverInterface::MD_WEST:
				RARate -= rate * SKYWATCHER_SIDEREAL_SPEED;
				break;
			case MountDriverInterface::MD_EAST:
				RARate += rate * SKYWATCHER_SIDEREAL_SPEED;
				break;
		}
	}
	
	// In Southern Hemisphere, RARates are in the opposite direction
	if (NorthHemisphere == false) {
		RARate = -RARate;
	}
	
	// Only set tracking rate for relevant axis - important for diagonal moves (otherwise reset the second axis)
	if (Dir == MountDriverInterface::MD_EAST || Dir == MountDriverInterface::MD_WEST) {
		err = SetTrackingRateAxis(Axis1, RARate, RASteps360, RAInteruptFreq, RAHighspeedRatio);
	}
	else {
		err = SetTrackingRateAxis(Axis2, DECRate, DESteps360, DEInteruptFreq, DEHighspeedRatio);
	}
	
	return err;
	
}

// Next function starts a slew to align the polar axis
int Skywatcher::PolarAlignment(double dHAHome, double dDecHome, int HomeIndex, double HaPolaris, double HAOctansSigma)
{
	unsigned long TargetRaStep, TargetDeStep, MaxStep;
	int err;
	err = StopAxesandWait(); if (err) return err;				  // Ensure no motion before starting a slew
	err = InquireMountAxisStepPositions(); if (err) return err;	  // Get Axis Positions in class members RAStep and DEStep	int err;

	// Calculate target RA and DEC step locations for the Alignment Home Position
	m_dFlipHour = 0.0;   // Set flip our angle to zero to avoid problems polaraligning.
	EncoderValuesfromHAanDEC(dHAHome, dDecHome, TargetRaStep, TargetDeStep, false);
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::PolarAlignment called hAHome %f decHome %f HomeIndex %d HaPolaris %f RAStepPolarHome %lu\n", timestamp, dHAHome, dDecHome, HomeIndex, HaPolaris, TargetRaStep);
#endif
	// Now add on steps to position of Polaris, and allowing for location of home poistion (12 O'clock, 3 0'Clock etc).
	if (NorthHemisphere) {
		// Add 12 to take account of inversion in polar scope, and 6 hours for every location clockwise of 12
		TargetRaStep += int((HaPolaris + 12.0 + HomeIndex*6.0)*RASteps360 / 24.0);
	}
	else {
		// Add 12 to take account of inversion in polar scope, subtract 6 hours for every location clockwise of 12 - RA in opposite direction in South
		// Also added 24 to ensure positive
		TargetRaStep += int((HAOctansSigma + 12.0 - HomeIndex*6.0+24.0)*RASteps360 / 24.0);
	}
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::PolarAlignment called TargetRAStep %lu\n", timestamp, TargetRaStep);
#endif
	
	
	// Now make sure Targetstep is in range - no more than half way round RA axis
	while (TargetRaStep > RAStepInit + RASteps360 / 2) {
		TargetRaStep -= RASteps360;
	}
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::PolarAlignment called TargetRAStep in range %lu\n", timestamp, TargetRaStep);
#endif
	// Detemine max step in either RA or DEC - used to calculate time of slew to get more accurate RA position
	if (abs(TargetRaStep - RAStep) > abs(TargetDeStep - DEStep)) {
		MaxStep = abs(TargetRaStep - RAStep);
	}
	else {
		MaxStep = abs(TargetDeStep - DEStep);
	}
	
	// Set Slews in Train - add on time taken for slew to RA position
	err = StartTargetSlew(Axis1, RAStep, TargetRaStep, RASteps360, MaxStep); if (err) return err;
	if (err)return err;
	// In this case MaxStep=0 - no time dependency for DEC
	err = StartTargetSlew(Axis2, DEStep, TargetDeStep, DESteps360, 0); if (err) return err;
	
	// Update axes status
	err = GetAxesStatus();
	
	return err;
}

int Skywatcher::StartSlewTo(const double& dRa, const double& dDec, const double& dFlipHourAngle)
{
	unsigned long TargetRaStep, TargetDeStep, MaxStep;
	int err;
	double HA;

	// Is this the first goto attempt?
	if (!m_bGotoInProgress) {
		err = StopAxesandWait(); if (err) return err;				  // Ensure no motion before starting a slew
		m_dGotoRATarget = dRa;
		m_dGotoDECTarget = dDec;
		m_iGotoIterations = 1;
		m_bGotoInProgress = true;
		m_dFlipHour = dFlipHourAngle;
	}
	else {
		m_iGotoIterations++;
	}

	// Determine HA from RA
	HA = m_pTSX->hourAngle(dRa);

	// Calculate target RA and DEC step locations
	err = InquireMountAxisStepPositions(); if (err) return err;	  // Get Axis Positions in class members RAStep and DEStep
	EncoderValuesfromHAanDEC(HA, dDec, TargetRaStep, TargetDeStep, false);

	// Detemine max step in either RA or DEC - used to calculate time of slew to get more accurate RA position
	if (abs(TargetRaStep - RAStep) > abs(TargetDeStep - DEStep)) {
		MaxStep = abs(TargetRaStep - RAStep);
	}
	else {
		MaxStep = abs(TargetDeStep - DEStep);
	}

#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StartSlewTo called RA %f Dec %f HA %f TargetRAStep %lu TargetDEStep %lu\n", timestamp, dRa, dDec, HA, TargetRaStep, TargetDeStep);
	fprintf(LogFile, "[%s] Skyw::StartSlewTo called RAStepInit %lu RASteps360 %lu DEStepINit %lu DESteps %lu MaxStep%lu\n", timestamp, RAStepInit, RASteps360, DEStepInit, DESteps360, MaxStep);
	fprintf(LogFile, "[%s] Skyw::StartSlewTo called RAStep %lu DEStep %lu\n", timestamp, RAStep, DEStep);
#endif

	// Set Slews in Train - add on time taken for slew to RA position
	err = StartTargetSlew(Axis1, RAStep, TargetRaStep, RASteps360, MaxStep);
	if (err)return err;
	// In this case MaxStep=0 - no time dependency for DEC
	err = StartTargetSlew(Axis2, DEStep, TargetDeStep, DESteps360, 0);

	// Update axes status
	err = GetAxesStatus();

	return err;
}

// Gets the current encoder values
int Skywatcher::GetMountEncoderValues(unsigned long& RaEncoderValue, unsigned long& DecEncoderValue)
{
	int err = GetAxesStatus(); if (err) return err;
	RaEncoderValue = RAStep;
	DecEncoderValue = DEStep;

	return SB_OK;
}

int Skywatcher::SyncToRAandDec(const double& dRa, const double& dDec)
{

	unsigned long TargetRaStep = 0, TargetDeStep = 0;
	double HA = 0;

#ifdef SKYW_DEBUG
	fprintf(LogFile, "Skyw::SyncToTo called RA %f Dec %f\n", dRa, dDec);
#endif

	// Determine HA from RA
	HA = m_pTSX->hourAngle(dRa);

	// Calculate encoder values from HA and DEC - set the UseBTP flag to true - ignore flip hour angle and use current side of mount to determine whether pre or post meridian
	EncoderValuesfromHAanDEC(HA, dDec, TargetRaStep, TargetDeStep, true);

	// Set Mount axis to this location
#ifdef SKYW_DEBUG
	fprintf(LogFile, "Skyw::SyncToTo called RA %f Dec %f HA %f TargetRAStep %lu TargetDEStep %lu RAStep %lu DeStep %lu\n", dRa, dDec, HA, TargetRaStep, TargetDeStep, RAStep, DEStep);
#endif

	return SyncToEncoder(TargetRaStep, TargetDeStep, true);

}

int Skywatcher::SyncToEncoder(unsigned long& TargetRaStep, unsigned long& TargetDeStep, bool b_tracking_on)
{
	int err;
	char command[SKYWATCHER_MAX_CMD], response[SKYWATCHER_MAX_CMD];

	// Stop axis before setting location
	err = StopAxesandWait(); 

	long2Revu24str(TargetRaStep, command);	// Convert target steps into string
	err = SendSkywatcherCommand(SetAxisPositionCmd, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

#ifdef SKYW_DEBUG
	fprintf(LogFile, "Skyw::SyncToEncoder set TargetRAStep command: %s response: %s\n", command, response);
#endif

	long2Revu24str(TargetDeStep, command);	// Convert target steps into string
	err = SendSkywatcherCommand(SetAxisPositionCmd, Axis2, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

#ifdef SKYW_DEBUG
	fprintf(LogFile, "Skyw::SyncToEncoder set TargetDeStep command: %s response: %s\n", command, response);
#endif


	// Start trackign again if required
	if (b_tracking_on) {
		err = SetTrackingRates(true, true, 0.0, 0.0); if (err) return err;
		err = GetAxesStatus(); if (err) return err;
	}

	return err;

}

int Skywatcher::StartPark(void)
{
	int err = 0;
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StartPark called\n", timestamp);
#endif
	err = StopAxesandWait(); if (err) return err;				  // Ensure no motion before starting a slew
	err = InquireMountAxisStepPositions(); if (err) return err;	  // Get Axis Positions in class members RAStep and DEStep
	m_bParkInProgress = true;									  // Set state of parking to be true.
	m_bGotoInProgress = true;									  // And goto since used Goto to get to the parking location.
	m_bTracking = false;										  // No longer tracking since about to start a slew.
	
	
	// Slew to default parking position for RA
	err = StartTargetSlew(Axis1, RAStep, RAStepInit, RASteps360, 0);
	if (err)return err;
	// Slew to default parking position for DEC
	err = StartTargetSlew(Axis2, DEStep, DEStepInit, DESteps360, 0);
	
	// Update axes status
	err = GetAxesStatus();
	return err;
}

int Skywatcher::StartTargetSlew(SkywatcherAxis Axis, long CurrentStep, long TargetStep, long StepsPer360, long MaxStep)
{
	int err = 0;
	SkywatcherDirection Direction;
	int lowspeed = 0;
	long MovingSteps = 0, LowSpeedGotoMargin;
	char command[SKYWATCHER_MAX_CMD], response[SKYWATCHER_MAX_CMD];
	double StepsPerSecSiderial = StepsPer360 / SKYWATCHER_SIDEREAL_DAY;
	double SlowSlewSpeed = StepsPerSecSiderial * SKYWATCHER_LOWSPEED_RATE;
	double FastSlewSpeed = StepsPerSecSiderial * SKYWATCHER_HIGHSPEED_RATE;
	double Sign = 0.0;	 // Need to add time for the move - sign used to determine whether to add or subtract steps to make this happen

    // First determine if moving backwards or forwards
	MovingSteps = TargetStep - CurrentStep;
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StartTargetSlew CurrentStep %ld TargetStep %ld Movingsteps %ld MaxStep %ld\n", timestamp, CurrentStep, TargetStep, MovingSteps, MaxStep);
	fprintf(LogFile, "[%s] Skyw::StartTargetSlew CurrentStep %ld TargetStep %ld Movingsteps %ld\n", timestamp, CurrentStep, TargetStep, MovingSteps);
#endif
	if (MovingSteps > 0) {
		Direction = FORWARD;
		Sign = NorthHemisphere ? 1.0 : - 1.0;
	}
	else {
		Direction = BACKWARD;
		MovingSteps = -MovingSteps;
		Sign = NorthHemisphere ? -1.0 : 1.0;
	}
	
	
	// Now determine if close enough for a low speed slew - calculated as is 5s at 128x siderial
	if (MaxStep == 0) Sign = 0; // No need to add on additional steps for DEC axis.
	LowSpeedGotoMargin = (int)((float)StepsPer360 * 640.0 / (24.0*3600.0));
	if (MovingSteps < LowSpeedGotoMargin) {
		lowspeed = 2; // Goto command 0 is high speed slew, 2 is lowspeed slew
		// Adjust moving steps to allow for time to slew.
		MovingSteps += Sign*(MaxStep*1.0 / SlowSlewSpeed)*StepsPerSecSiderial;
	}
	else {
		lowspeed = 0; // Goto command 0 is high speed slew, 2 is lowspeed slew
		//Adjust moving steps to allow for time to slew
		MovingSteps += Sign*((MaxStep*1.0 - LowSpeedGotoMargin*1.0) / FastSlewSpeed + LowSpeedGotoMargin / SlowSlewSpeed)*StepsPerSecSiderial;
	}
	
	// Subtract m_dDeltaHASteps which is error in last slew in steps
	MovingSteps += Sign* m_dDeltaHASteps;

	// Possible that moving steps could now have changed sign! If so, trap change sign and direction.
	if (MovingSteps < 0) {
		if (Direction == FORWARD) {
			Direction = BACKWARD;
		}
		else {
			Direction = FORWARD;
		}
		MovingSteps = -MovingSteps;
#ifdef SKYW_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::StartTargetSlew: Trapped Negative Move. Now Movingsteps: %ld Direction %d\n", timestamp, MovingSteps, Direction);
#endif

	}

	
	// Tell mount to do goto with speed and direction
	sprintf(command, "%d%d", lowspeed, Direction);
	err = SendSkywatcherCommand(SetMotionMode, Axis, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StartTargetSlew called SetMotion response %s movingsteps %ld direction %d\n", timestamp, response, MovingSteps, Direction);
#endif
	
	// Tell mount the target to move to
	long2Revu24str(MovingSteps, command);	// Convert moving steps into string
	err = SendSkywatcherCommand(SetGotoTargetIncrement, Axis, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StartTargetSlew called SetGototTarget Command %s :%s\n", timestamp, command, response);
#endif
	// Set breaksteps for the target - when turns into low speed slew
	long2Revu24str(SKYWATCHER_BREAKSTEPS, command); // Convert break steps into string
	err = SendSkywatcherCommand(SetBreakPointIncrement, Axis, command, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::StartTargetSlew called SetBreakPointIncrementCommand %s :%s\n", timestamp, command, response);
#endif
	
	// Finally, start the slew
	err = SendSkywatcherCommand(StartMotion, Axis, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;

	m_bTracking = false; // Now now longer tracking

	return err;
	
}

int Skywatcher::InquireMountAxisStepPositions(void)
{
	char response[SKYWATCHER_MAX_CMD];
	int err;
	
	// Get axis motor positions
	err = SendSkywatcherCommand(GetAxisPosition, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	RAStep = Revu24str2long(response + 1);
	
	err = SendSkywatcherCommand(GetAxisPosition, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
	DEStep = Revu24str2long(response + 1);
	
	return err;
}

int Skywatcher::ReadMountData(void)
{
	char response[SKYWATCHER_MAX_CMD];
	char command[SKYWATCHER_MAX_CMD];
	unsigned long tmpMCVersion = 0;
	unsigned int decode; // Used to decode response from extend inquiry
	int err;

    // Get Mount Status - find out if this has been initialised already
	err = GetAxesStatus(); if (err) return err;
	
	// Read the mount and software version
	err = SendSkywatcherCommand(InquireMotorBoardVersion, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	
	
	// Now Decode motorboard response - code from Skywatcher Basic API - but looks complicated given the simple string returned
	sprintf(MCVersionName, "%c%c.%c%c", response[1], response[2], response[3], response[4]);
	tmpMCVersion = Revu24str2long(response + 1);
	MCVersion = ((tmpMCVersion & 0xFF) << 16) | ((tmpMCVersion & 0xFF00)) | ((tmpMCVersion & 0xFF0000) >> 16);
	MountCode = MCVersion & 0xFF;
	//	sprintf(MCVersionName, "%04lx", (MCVersion >> 8)); - version in Skywatcher Basic API
	
	// Translate MountCode into text:
	switch (MountCode) {
		case 0x00: strcpy(MountName, "EQ6"); break;
		case 0x01: strcpy(MountName, "HEQ5"); break;
		case 0x02: strcpy(MountName, "EQ5"); break;
		case 0x03: strcpy(MountName, "EQ3"); break;
		case 0x04: strcpy(MountName, "EQ8"); break;
		case 0x05: strcpy(MountName, "AZEQ6"); break;
		case 0x06: strcpy(MountName, "AZEQ5"); break;

		// Next set of mountcodes are from Frank Liu at Skywatcher with more recent control boards
		case 0x20: strcpy(MountName, "EQ-R/EQ8-RH"); break;
		case 0x21: strcpy(MountName, "EQ-8"); break;
		case 0x22: strcpy(MountName, "AZ-EQ6"); break;
		case 0x23: strcpy(MountName, "EQ6-R"); break;
		case 0x24: strcpy(MountName, "EQ6"); break;
		case 0xA5: strcpy(MountName, "Az Gti"); break;

		default: // Unrecognised mount
			// Frank also tells me that any mount with code < 0x80 is an EQmount, so recognise those mounts
			if (MountCode < 0x80) {
				strcpy(MountName, "EQ Mount");
			}
			else {
				strcpy(MountName, "Unsupported");
				err = ERR_DEVICENOTSUPPORTED;
				return err;
			}
	}

#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::ReadMountData InquireMotorBoardVersion response string %s\n", timestamp, response);
	fprintf(LogFile, "[%s] Skyw::ReadMountData Mount Code %lu\n", timestamp, MountCode);
	fprintf(LogFile, "[%s] Skyw::ReadMountData MC Version %lu %s\n", timestamp, MCVersion, MCVersionName);
	fprintf(LogFile, "[%s] Skyw::ReadMountData Mount %s\n", timestamp, MountName);
#endif
	
	// Attempt to read the extended mount status
	long2Revu24str(INQUIRE_STATUS, command);
	err = SendSkywatcherCommand(GetFeatureCmd, Axis1, command, response, SKYWATCHER_MAX_CMD);

#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::ReadMountData Get Feature Reponse Axis1: err: %d response string %s\n", timestamp, err, response);
#endif

	// First test to see if the extended data set gives reponse or error. If error, cannot be read.
	if (response[0] == '=') {
		m_bExtendedCmds = true;

		decode = HEX(response[1]);                              // Turn second character of response into a number
		m_bPECTrainingOn = decode & 0x1;                        // First bit of response
		m_bPECTrackingOn = decode & 0x2;	                    // Second bit of second charcter in response

		decode = HEX(response[2]); 
		m_bSupportDualEncoder = decode & 0x1;					// First bit of next character
		m_bSupportPEC = decode & 0x2;							// Second bit of next character

		decode = HEX(response[3]);
		m_bPolarScope = decode & 0x1;							// First bit of next character.
	}
	else {
		m_bExtendedCmds = false;
		m_bPECTrainingOn = false;
		m_bPECTrackingOn = false;
		m_bSupportDualEncoder = false;
		m_bSupportPEC = false;
		m_bPolarScope = false;
	}


#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::ReadMountData PecTraining on %d PecTracking %d Encoders %d Pec %d\n", timestamp, m_bPECTrainingOn, m_bPECTrackingOn, m_bSupportDualEncoder, m_bSupportPEC);
#endif

	// Now read the Steps per 360 degrees for RA and Dec
	err = SendSkywatcherCommand(InquireGridPerRevolution, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	RASteps360 = Revu24str2long(response+1);
	
	err = SendSkywatcherCommand(InquireGridPerRevolution, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	DESteps360 = Revu24str2long(response+1);
	
	// Now read timer interupt frequncy
	err = SendSkywatcherCommand(InquireTimerInterruptFreq, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	RAInteruptFreq = Revu24str2long(response+1);
	
	err = SendSkywatcherCommand(InquireTimerInterruptFreq, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	DEInteruptFreq = Revu24str2long(response+1);
	
	// Now read High speed ratio
	err = SendSkywatcherCommand(InquireHighSpeedRatio, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	RAHighspeedRatio = Highstr2long(response+1);
	
	err = SendSkywatcherCommand(InquireHighSpeedRatio, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
	DEHighspeedRatio = Highstr2long(response+1);
	
	// Has this mount been initialised already? If so, use default values for initial axis location
	if (AxisStatus[RA].initialised == INITIALISED || AxisStatus[DEC].initialised == INITIALISED) {
		RAStepInit = 0x800000;
		DEStepInit = 0x800000;
	}
	else
	{
		// Now work out initial location of scope
		err = SendSkywatcherCommand(GetAxisPosition, Axis1, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
		RAStepInit = Revu24str2long(response + 1);
		
		err = SendSkywatcherCommand(GetAxisPosition, Axis2, NULL, response, SKYWATCHER_MAX_CMD); if (err) return err;
		DEStepInit = Revu24str2long(response + 1);
	}
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::ReadMountData RASteps360 %lu DESteps360 %lu\n", timestamp, RASteps360, DESteps360);
	fprintf(LogFile, "[%s] Skyw::ReadMountData RAStepsWorm %lu DEStepsWorm %lu\n", timestamp, RAInteruptFreq, DEInteruptFreq);
	fprintf(LogFile, "[%s] Skyw::ReadMountData RAHighSpeedRatio %lu DEHighSpeedRatio %lu\n", timestamp, RAHighspeedRatio, DEHighspeedRatio);
	fprintf(LogFile, "[%s] Skyw::ReadMountData RAStepInit %lu DEStepInit %lu\n", timestamp, RAStepInit, DEStepInit);
#endif
	
	
	return err;
}

// Function to retrieve tracking rates
int Skywatcher::GetTrackingRates(bool& bTrackingOn, double& dRaRateArcSecPerSec, double& dDecRateArcSecPerSec)
{
	bTrackingOn = m_bTracking;
	dRaRateArcSecPerSec = m_dRATrackingRate;
	dDecRateArcSecPerSec = m_dDETrackingRate;

	return SB_OK;
}

/* This is taken from the Indi mount - has added this to the end of a slew for AstroEQ mounts */
int Skywatcher::ResetMotions(void)
{
	char command[SKYWATCHER_MAX_CMD], response[SKYWATCHER_MAX_CMD];
	SkywatcherAxisStatus CurrentAxisStatus;
	int err = SB_OK;

#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::ResetMotions Called\n", timestamp);
#endif

	// Get axis status to find direction and set
	err = GetAxisStatus(Axis1, CurrentAxisStatus); if (err) return err;
	sprintf(command, "1%d", CurrentAxisStatus.direction);
	err = SendSkywatcherCommand(SetMotionMode, Axis1, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

	err = GetAxisStatus(Axis2, CurrentAxisStatus); if (err) return err;
	sprintf(command, "1%d", CurrentAxisStatus.direction);
	err = SendSkywatcherCommand(SetMotionMode, Axis2, command, response, SKYWATCHER_MAX_CMD); if (err) return err;

	return SB_OK;
}



int Skywatcher::SendSkywatcherCommand(SkywatcherCommand cmd, SkywatcherAxis Axis, char *cmdArgs, char *response, int maxlen)
{
	int itries;
	int err = SB_OK;

#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::SendSkywatcherCommand Entered: Cmd %c Axis %d Args %s\n", timestamp, cmd, Axis, cmdArgs);
#endif
	// Ensure that the mount is connected. If not, try to connect:
	if (!m_bLinked) {
		err = Connect(); if (err) return err;
	}
	
	// Try SKYWATCHER_MAX_TRIES times until failure
	for (itries = 0; itries < SKYWATCHER_MAX_TRIES; itries++) {
		err = SendSkywatcherCommandInnerLoop(cmd, Axis, cmdArgs, response, maxlen);
		if (err == SB_OK) break;

		// When the usb cable is unplugged, then get err = 5. In this case, the mount will have to be reconnected.
		// So, close the port and return error
		if (err == 5) {
#ifdef SKYW_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] Skyw::SendSkywatcherCommand Error 5 - mount disconnected.\n", timestamp);
#endif
			m_pSerX->close();
			m_bLinked = false;
			return ERR_AUTOTERMINATE;

		}
	}
	
#ifdef SKYW_DEBUG
	if (err != SB_OK) {
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] Skyw::SendSkywatcherCommand Error Code: %d\n", timestamp, err);
	}
	else {
		if (itries > 0){
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(LogFile, "[%s] Skyw::SendSkywatcherCommand itries: %d\n", timestamp, itries);
		}
	}
#endif
	return err;
}

int Skywatcher::SendSkywatcherCommandInnerLoop(SkywatcherCommand cmd, SkywatcherAxis Axis, char *cmdArgs, char *response, int maxlen)
{
	char command[SKYWATCHER_MAX_CMD];
	int err = SB_OK;
	unsigned long NBytesWrite = 0, nBytesRead = 0, totalBytesRead = 0;
	long udpread;
    // int dDelayMs;

	char *bufPtr;
	
	// Format the command;
	if (cmdArgs == NULL) {
		snprintf(command, SKYWATCHER_MAX_CMD, "%c%c%c%c", SkywatcherLeadingChar, cmd, Axis, SkywatcherTrailingChar);
	}
	else {
		snprintf(command, SKYWATCHER_MAX_CMD, "%c%c%c%s%c", SkywatcherLeadingChar, cmd, Axis, cmdArgs, SkywatcherTrailingChar);
	}
	/*
    if(m_cmdDelayTimer.GetElapsedSeconds()<INTER_COMMAND_WAIT) {
        dDelayMs = INTER_COMMAND_WAIT - int(m_cmdDelayTimer.GetElapsedSeconds() *1000);
        if(dDelayMs>0)
            m_pSleeper->sleep(dDelayMs);
    }
    m_cmdDelayTimer.Reset();
     */
    
	// Now send the command
	if (!m_bWiFi) {
	m_pSerX->purgeTxRx();
	err = m_pSerX->writeFile((void *)command, strlen(command), NBytesWrite);
	m_pSerX->flushTx();
	
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::SendSkywatcherCommand Write Command: %c Error Code :%d\n", timestamp, cmd, err);
#endif
	if (err) return err;
	
	//Now Read the response
	// First clear the buffer;
	memset(response, '\0', (size_t)maxlen);
	bufPtr = response;
	do {
		err = m_pSerX->readFile(bufPtr, 1, nBytesRead, SKYWATCHER_MAX_TIMEOUT); if (err) return err;
		
		if (nBytesRead != 1) {// timeout
			err = ERR_RXTIMEOUT;
			break;
		}
		
		totalBytesRead += nBytesRead;
		
	} while (*bufPtr++ != SkywatcherTrailingChar && totalBytesRead < maxlen);
	
	if (!err) *--bufPtr = '\0'; //remove the trailing character
	} 
#if defined SB_LINUX_BUILD || defined SB_WIN_BUILD || defined SB_MAC_BUILD // Lets try all three for now
	else {
	  // Wifi - sending using UDP
	  sockaddr retserver;

#ifdef SB_WIN_BUILD
	  // Winsock does not define socklen_t
	  typedef int socklen_t;

	  // Winsock does not use a timeval struct, but instead a DWORD (32 bit unsigned integer)
	  DWORD tvwin;
#endif

	  socklen_t lenretserver = sizeof(retserver);

#if defined SB_LINUX_BUILD || defined SB_MAC_BUILD
	  // Mac and Unix use a timeval struct to set the timout
	  // Set timeout to 0.1s
	  struct timeval tv;
	  tv.tv_sec = 0;
	  tv.tv_usec = 100000;

	  err = setsockopt(m_isockfd, SOL_SOCKET, SO_RCVTIMEO,(const char *) &tv,sizeof(tv));
	  if (err) return ERR_RXTIMEOUT;
	  err = setsockopt(m_isockfd, SOL_SOCKET, SO_SNDTIMEO,(const char *) &tv,sizeof(tv));
	  if (err) return ERR_TXTIMEOUT;
#endif

#ifdef SB_WIN_BUILD
	  // Timeout measured in milliseconds 0.1s
	  tvwin = 100;
	  err = setsockopt(m_isockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tvwin, sizeof(tvwin));
	  if (err) return ERR_RXTIMEOUT;
	  err = setsockopt(m_isockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tvwin, sizeof(tvwin));
	  if (err) return ERR_TXTIMEOUT;
#endif

	  // Send command to the mount
	  udpread = sendto(m_isockfd, command, strlen(command), 0,  (const sockaddr*) &m_serveraddr,
		       m_iserverlen);
	  if (udpread < 0) return ERR_TXTIMEOUT;

	  // Clear buffer before reading response
	  memset(response, '\0', (size_t) maxlen);

	  // Read response from socket
	  udpread = recvfrom(m_isockfd, response, maxlen, 0, &retserver, &lenretserver);

#ifdef SKYW_DEBUG
	  ltime = time(NULL);
	  timestamp = asctime(localtime(&ltime));
	  timestamp[strlen(timestamp) - 1] = 0;
	  fprintf(LogFile, "[%s] Skyw::SendSkywatcherCommandInnerLoop updread %ld command %s response %s\n", timestamp, udpread, command, response);
#endif

	  if (udpread < 0) return ERR_RXTIMEOUT;
	  response[udpread-1] = '\0'; // remove trailing character
	  totalBytesRead = udpread;
	  err = SB_OK;
	}
#endif

	  
#ifdef SKYW_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(LogFile, "[%s] Skyw::SendSkywatcherCommand %c Error Code :%d  read response %s bytes read %lu\n", timestamp, cmd, err, response, totalBytesRead);
#endif
	return err;
	
}



// Utility functions from Indi which convert to and from character strings to long
unsigned long Skywatcher::Revu24str2long(char *s) {
	unsigned long res = 0;
	res = HEX(s[4]); res <<= 4;
	res |= HEX(s[5]); res <<= 4;
	res |= HEX(s[2]); res <<= 4;
	res |= HEX(s[3]); res <<= 4;
	res |= HEX(s[0]); res <<= 4;
	res |= HEX(s[1]);
	return res;
}

unsigned long Skywatcher::Highstr2long(char *s) {
	unsigned long res = 0;
	res = HEX(s[0]); res <<= 4;
	res |= HEX(s[1]);
	return res;
}

void Skywatcher::long2Revu8str(unsigned long n, char *str) {
	char hexa[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	str[0] = hexa[(n & 0xF0) >> 4];
	str[1] = hexa[(n & 0x0F)];
	str[2] = '\0';
}



void Skywatcher::long2Revu24str(unsigned long n, char *str) {
	char hexa[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	str[0] = hexa[(n & 0xF0) >> 4];
	str[1] = hexa[(n & 0x0F)];
	str[2] = hexa[(n & 0xF000) >> 12];
	str[3] = hexa[(n & 0x0F00) >> 8];
	str[4] = hexa[(n & 0xF00000) >> 20];
	str[5] = hexa[(n & 0x0F0000) >> 16];
	str[6] = '\0';
}
