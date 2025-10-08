#pragma once
#ifdef SB_WIN_BUILD
#include <WinSock2.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <string.h>
#include <memory.h>
#include <math.h>
#include <stdlib.h>

#include <cstdio>
#include <time.h>
#include <string>


#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/mountdriverinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"


// #include "StopWatch.h"


// #define SKYW_DEBUG 1   // define this to have log files

// Defines below from INDI EQMOD
#define SKYWATCHER_DRIVER_VERSION 3.2
#define SKYWATCHER_MAX_CMD        16
#define SKYWATCHER_MAX_TRIES      3
#define SKYWATCHER_CHAR_BUFFER   1024
#define SKYWATCHER_MAX_TIMEOUT 200

#define SKYWATCHER_SIDEREAL_DAY 86164.09053083288
#define SKYWATCHER_SIDEREAL_SPEED 15.04106864

#define SKYWATCHER_LOWSPEED_RATE 128      // Times Sidereal Speed
#define SKYWATCHER_HIGHSPEED_RATE 800     // Times Siderdeal Speed
#define SKYWATCHER_MINSLEW_RATE  0.0001   // Times Sidereal Speed
#define SKYWATCHER_MAXSLEW_RATE  800      // Times Sidereal Speed

#define SKYWATCHER_BREAKSTEPS 3500

#define SKYWATCHER_MAX_GOTO_ITERATIONS 5   // How many times to attempt an iterative goto
#define SKYWATCHER_GOTO_ERROR          5.0 // Permitted arcseconds of error in goto target

#define INTER_COMMAND_WAIT    100

// Next turns string charcter representing a HEX code into a number
#define HEX(c) (((c) < 'A')?((c)-'0'):((c) - 'A') + 10)

// Define Class for Skywatcher
class Skywatcher
{
public:
	Skywatcher(SerXInterface *pSerX, SleeperInterface *pSleeper, TheSkyXFacadeForDriversInterface *pTSX);
	~Skywatcher();
	
	int Connect(void);
	int Disconnect();
	bool isConnected() const { return m_bLinked; }
	char *GetMCVersionName() { return MCVersionName;  }
	char *GetMountName() { return MountName;  }
	int StartSlewTo(const double& dRa, const double& dDec, const double& dFlipHourAngle);
	int SyncToRAandDec(const double& dRa, const double& dDec);
	int SyncToEncoder(unsigned long& RaEncoder, unsigned long& DecEncoder, bool b_tracking_on);
	int GetMountEncoderValues(unsigned long& RaEncoderValue, unsigned long& DecEncoderValue);
	int GetMountHAandDec(double& dHa, double &dDec);
	bool GetIsNotGoto() const { return !m_bGotoInProgress; }
	bool GetIsParkingComplete() const { return !m_bGotoInProgress; }
	bool GetIsBeyondThePole() const { return IsBeyondThePole; }
	bool GetIsPolarAlignInProgress() const { return !IsNotGoto; }
	bool GetIsPecTrainingOn();
	bool GetIsPecTrackingOn() const { return m_bPECTrackingOn;  }
	bool GetIsPecSupported() const { return m_bSupportPEC; }
	bool GetDoesMountHaveValidPecData() const { return m_bValidPECData;  }
	bool GetDoesMountHavePolarScope() const { return m_bPolarScope;  }
	int TurnOnPec();
	int TurnOffPec();
	int StartPecTraining();
	int CancelPecTraining();

	int Abort(void);
	int SetTrackingRates(const bool& bTrackingOn, const bool& bIgnoreRates, const double& dRaRateArcSecPerSec, const double& dDecRateArcSecPerSec);
	int GetAxesStatus(void);
	int StartPark(void);
	int StartOpenSlew(const MountDriverInterface::MoveDir & 	Dir, double rate);
    int EndOpenSlew(void);
	int PolarAlignment(double dHAHome, double dDecHome, int HomeIndex, double HaPolaris, double HAOctansSigma);
	int SetST4GuideRate(int m_ST4GuideRateIndex);
	void SetWiFiEnabled(bool enabled) { m_bWiFi = enabled; }
	bool GetWiFiEnabled(void) { return m_bWiFi; }
	void SetBaudRate(int BaudRate) { m_iBaudRate = BaudRate; }
	int GetBaudRate(void) { return m_iBaudRate; }            
	int ResetMotions(void);
	int GetTrackingRates(bool& bTrackingOn, double& dRaRateArcSecPerSec, double& dDecRateArcSecPerSec);
	int GetIsTracking() const { return m_bTracking; }
	int SetPolarScopeIllumination(int Brightness);
	void SetConnectionData(char *portname, char *IPAddress, int port, bool wifi);
	int WiFiCheck(void);
	
private:
	SerXInterface *m_pSerX;                       // Serial X interface to use to connect to device
	SleeperInterface *m_pSleeper;				  // Sleeper interface for Sky X
	TheSkyXFacadeForDriversInterface *m_pTSX;	  // Pointer to interface to allow calculation of LST etc.
	
	bool m_bLinked;                               // Connected to the mount?
	int m_ST4GuideRate;	                          // Guide Rate
	int m_iBaudRate;							  // Baud Rate
	
	bool NorthHemisphere;					      // Located in the Northern Hemisphere?

	// Data to be read from mount
	unsigned long MCVersion;                      // Motor Controller Version
	char MCVersionName[SKYWATCHER_MAX_CMD];       // String version of Mount Controller
	unsigned long MountCode;                      // Mount code
	char MountName[SKYWATCHER_MAX_CMD];           // Name of mount from Mount Code
	unsigned long RASteps360;                     // Number of steps in full circle of RA motor
	unsigned long DESteps360;                     // Number of steps in full circle of RA motor
	unsigned long RAInteruptFreq;				  // RA Stepper motor frequency
	unsigned long DEInteruptFreq;                 // DEC Stepper motor frequency
	unsigned long RAHighspeedRatio;               // Motor controller multiplies speed values by this ratio when in low speed mode
	unsigned long DEHighspeedRatio;               // This is a reflect of either using a timer interrupt with an interrupt count greater than 1 for low speed

	// Extended data to be read from mount
	bool m_bExtendedCmds;						  // Can read extended commands
	bool m_bPECTrainingOn;						  // Is PEC Training On?
	bool m_bPECTrackingOn;						  // Ditto PEC tracking
	bool m_bSupportDualEncoder;					  // Does mount have dual encoders?
	bool m_bSupportPEC;							  // Does Mount Support PEC?
	bool m_bValidPECData;						  // Does Mount have Valid PEC data?
	bool m_bPolarScope;							  // Has polar scope that can be altered by commands.

	// or of using microstepping only for low speeds and half/full stepping for high speeds
	unsigned long RAStep;                         // Current RA encoder position in step
	unsigned long DEStep;                         // Current DE encoder position in step
	unsigned long RAStepInit;                     // Initial RA position in step
	unsigned long DEStepInit;                     // Initial DE position in step
	unsigned long RAStepHome;                     // Home RA position in step
	unsigned long DEStepHome;                     // Home DE position in step
	unsigned long RAPeriod;                       // Current RA worm period ??
	unsigned long DEPeriod;                       // Current DE worm period
	
	double m_dGotoRATarget;						  // Current Target RA;
	double m_dGotoDECTarget;                      // Current Goto Target Dec;
	double m_dFlipHour;							  // Flip hour angle for calculations.
	int m_iGotoIterations;					      // Iterations to goto target
	bool m_bGotoInProgress;						  // Is GOTO in progress?
	double m_dDeltaHASteps;						  // Error from previous slew when at low speed slew
	bool m_bParkInProgress;						  // Is a park in progress?

	// Variables for Tracking
	bool m_bTracking;							  // Is the telescope tracking?
	double m_dRATrackingRate;					  // RA Tracking rate in arcsec/sec
	double m_dDETrackingRate;	                  // DEC Tracking Rate in arcsec/sec
			
	// Types
	enum SkywatcherCommand {
		SetAxisPositionCmd = 'E',
		Initialize = 'F',
		SetMotionMode = 'G',
		SetGotoTargetIncrement = 'H',
		SetBreakPointIncrement = 'M',
		SetGotoTarget = 'S',
		SetStepPeriod = 'I',
		SetLongGotoStepPeriod = 'T',
		SetBreakStep = 'U',
		StartMotion = 'J',
		NotInstantAxisStop = 'K',
		InstantAxisStop = 'L',
		ActivateMotor = 'B',
		SetSwitch = 'O',
		SetGuideRate = 'P',
		RunBootLoaderMode = 'Q',
		SetPolarScopeLEDBrightness = 'V',

		InquireGridPerRevolution = 'a',
		InquireTimerInterruptFreq = 'b',
		InquireBrakeSteps = 'c',
		InquireGotoTargetPosition = 'h',
		InquireStepPeriod = 'i',
		GetAxisPosition = 'j',
		InquireIncrement = 'k',
		InquireBreakPoint = 'm',
		InquireAxisStatus = 'f',
		InquireHighSpeedRatio = 'g',
		GetStepPeriod = 'D',
		GetHomePosition = 'd',
		InquireMotorBoardVersion = 'e',
		InquirePECPeriod = 's',
		SetDebugFlag = 'z',


		SetFeatureCmd = 'W', // EQ8/AZEQ6/AZEQ5 only
		GetFeatureCmd = 'q', // EQ8/AZEQ6/AZEQ5 only

		NUMBER_OF_SkywatcherCommand
	};
	
	// From the latest INDI release - commands to deal with features.
	enum SkywatcherSetFeatureCmd {
		START_PPEC_TRAINING_CMD = 0x00, STOP_PPEC_TRAINING_CMD = 0x01,
		TURN_PPEC_ON_CMD = 0x02, TURN_PPEC_OFF_CMD = 0X03,
		ENCODER_ON_CMD = 0x04, ENCODER_OFF_CMD = 0x05,
		DISABLE_FULL_CURRENT_LOW_SPEED_CMD = 0x0006, ENABLE_FULL_CURRENT_LOW_SPEED_CMD = 0x0106,
		RESET_HOME_INDEXER_CMD = 0x08
	};
	
	enum SkywatcherGetFeatureCmd {
		INDEX_POSITION = 0x00,
		INQUIRE_STATUS = 0x01
	};
	
	enum SkywatcherAxis {
		Axis1 = '1',       // RA/AZ
		Axis2 = '2',       // DE/ALT
		NUMBER_OF_SKYWATCHERAXIS
	};
	
	enum SkywatcherAxisName {
		RA = 0,
		DEC = 1,
		NUMBER_OF_SKYWATCHERAXISNAMES
	};
	
	enum SkywatcherDirection { FORWARD = 0, BACKWARD = 1 };
	enum SkywatcherMotionMode { STOPPED = 0, SLEW = 1, GOTO = 2};
	enum SkywatcherSpeedMode { LOWSPEED = 0, HIGHSPEED = 1 };
	enum SkywatcherInitialised {INITIALISED=0, NOTINITIALISED = 1};
	typedef struct SkywatcherAxisStatus { SkywatcherDirection direction; SkywatcherMotionMode motionmode; SkywatcherSpeedMode speedmode; SkywatcherInitialised initialised; } SkywatcherAxisStatus;

	int SendSkywatcherCommand(SkywatcherCommand cmd, SkywatcherAxis Axis, char *cmdArgs, char *response, int maxlen);
	int SendSkywatcherCommandInnerLoop(SkywatcherCommand cmd, SkywatcherAxis Axis, char *cmdArgs, char *response, int maxlen);

	int ReadMountData(void);              // Read the initial mount data
	bool IsNotGoto;
	bool IsBeyondThePole;                    // Is the mount west of the pier?
	int SetTrackingRateAxis(SkywatcherAxis Axis1, double Rate, unsigned long Steps360, unsigned long InteruptFrequency, unsigned long HighspeedRatio);
	int InquireMountAxisStepPositions(void);
	int StartTargetSlew(SkywatcherAxis Axis, long CurrentStep, long TargetStep, long StepsPer360, long MaxStep);
	int StopAxesandWait(void);
	int StopAxisandWait(SkywatcherAxis Axis);
	int GetAxisStatus(SkywatcherAxis Axis, SkywatcherAxisStatus &AxisStatus);
	void HAandDECfromEncoderValues(unsigned long RAEncoder, unsigned long DEEncoder, double &dHA, double &dDec);
	void EncoderValuesfromHAanDEC(double dHa, double dDec, unsigned long &RAEncoder, unsigned long &DEEncoder, bool bUseBTP);
	
	
	// Official Skywatcher Protocol
	// See http://code.google.com/p/skywatcher/wiki/SkyWatcherProtocol
	// Constants
	static const char SkywatcherLeadingChar = ':';
	static const char SkywatcherTrailingChar = 0x0d;
	unsigned long minperiods[2];
	SkywatcherAxisStatus AxisStatus[2];
	
	// Convenience functions from INDI EQMOD which convert from/to a hex string <=>long value
	unsigned long Revu24str2long(char *); // Converts 6 character HEX number encoded as string to a long value
	void long2Revu8str(unsigned long, char *); // Converts long number to 2 character string
	unsigned long Highstr2long(char *);   // Converts 2 character HEX number encoded as string to a long value
	void long2Revu24str(unsigned long, char *); // Converts long number to character string
	long abs(long value); //Utility function since not in math.h in Unix
	
	// Connection Data
	char m_cPortname[SKYWATCHER_CHAR_BUFFER];	  // Serial Port Name
	char m_cIPAddress[SKYWATCHER_CHAR_BUFFER];	  // WiFi IP Address
	int  m_iWiFiPortNumber;                           // WiFi Port Number
	bool m_bWiFi;                                     // Are we using WiFi or not?

	// Lets try this for all builds
//#if defined SB_LINUX_BUILD || defined SB_WIN_BUILD
	// WIFI UDP Socet Data
	int m_isockfd;     // Socket id
	int m_iserverlen;  // Length of server address
	struct sockaddr_in m_serveraddr; // Struct for server address
//#endif

#ifdef SB_WIN_BUILD
	WSADATA wsadata;
#endif

	// CStopWatch        m_cmdDelayTimer;
    
#ifdef SKYW_DEBUG
	char m_sLogfilePath[SKYWATCHER_CHAR_BUFFER];
	// timestamp for logs
	char *timestamp;
	time_t ltime;
	FILE *LogFile;      // LogFile
#endif

};

