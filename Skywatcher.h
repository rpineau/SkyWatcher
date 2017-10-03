#pragma once
#include <cstdio>
#include <time.h>

#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/mountdriverinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"

// #define SKYW_DEBUG 1   // define this to have log files

#ifdef SKYW_DEBUG
#if defined(SB_WIN_BUILD)
#define SKYW_LOGFILENAME "C:\\SkyLog.txt"
#elif defined(SB_LINUX_BUILD)
#define SKYW_LOGFILENAME "/tmp/SkyLog.txt"
#elif defined(SB_MAC_BUILD)
#define SKYW_LOGFILENAME "/tmp/SkyLog.txt"
#endif
#endif

// Defines below from INDI EQMOD
#define SKYWATCHER_DRIVER_VERSION 1.14;
#define SKYWATCHER_MAX_CMD        16
#define SKYWATCHER_MAX_TRIES      3
#define SKYWATCHER_CHAR_BUFFER   1024
#define SKYWATCHER_MAX_TIMEOUT 500

#define SKYWATCHER_SIDEREAL_DAY 86164.09053083288
#define SKYWATCHER_SIDEREAL_SPEED 15.04106864

#define SKYWATCHER_LOWSPEED_RATE 128      // Times Sidereal Speed
#define SKYWATCHER_HIGHSPEED_RATE 800     // Times Siderdeal Speed
#define SKYWATCHER_MINSLEW_RATE  0.0001   // Times Sidereal Speed
#define SKYWATCHER_MAXSLEW_RATE  800      // Times Sidereal Speed

#define SKYWATCHER_BREAKSTEPS 3500

#define SKYWATCHER_MAX_GOTO_ITERATIONS 5  // How many times to attempt an iterative goto
#define SKYWATCHER_GOTO_ERROR          5 // Permitted arcseconds of error in goto target


// Next turns string charcter representing a HEX code into a number
#define HEX(c) (((c) < 'A')?((c)-'0'):((c) - 'A') + 10)

// Define Class for Skywatcher
class Skywatcher
{
public:
	Skywatcher(SerXInterface *pSerX, SleeperInterface *pSleeper, TheSkyXFacadeForDriversInterface *pTSX);
	~Skywatcher();
	
	int Connect(char *portName);
	int Disconnect();
	bool isConnected() const { return m_bLinked; }
	char *GetMCVersionName() { return MCVersionName;  }
	char *GetMountName() { return MountName;  }
	int StartSlewTo(const double& dRa, const double& dDec);
	int SyncTo(const double& dRa, const double& dDec);
	int GetMountHAandDec(double& dHa, double &dDec);
	bool GetIsNotGoto() const { return !m_bGotoInProgress; }
	bool GetIsParkingComplete() const { return !m_bGotoInProgress; }
	bool GetIsBeyondThePole() const { return IsBeyondThePole; }
	bool GetIsPolarAlignInProgress() const { return !IsNotGoto; }
	int Abort(void);
	int SetTrackingRates(const bool& bTrackingOn, const bool& bIgnoreRates, const double& dRaRateArcSecPerSec, const double& dDecRateArcSecPerSec);
	int GetAxesStatus(void);
	int StartPark(void);
	int StartOpenSlew(const MountDriverInterface::MoveDir & 	Dir, double rate);
	int PolarAlignment(double dHAHome, double dDecHome, int HomeIndex, double HaPolaris, double HAOctansSigma);
	int SetST4GuideRate(int m_ST4GuideRateIndex);
	
	
private:
	SerXInterface *m_pSerX;                       // Serial X interface to use to connect to device
	SleeperInterface *m_pSleeper;				  // Sleeper interface for Sky X
	TheSkyXFacadeForDriversInterface *m_pTSX;	  // Pointer to interface to allow calculation of LST etc.
	
	bool m_bLinked;                               // Connected to the mount?
	int m_ST4GuideRate;	                          // Guide Rate
	bool NorthHemisphere;					      // Located in the Northern Hemisphere?
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
	int m_iGotoIterations;					      // Iterations to goto target
	bool m_bGotoInProgress;						  // Is GOTO in progress?
	double m_dDeltaHASteps;						  // Error from previous slew when at low speed slew
	bool m_bParkInProgress;						  // Is a park in progress?
	
	// Types
	enum SkywatcherCommand {
		Initialize = 'F',
		InquireMotorBoardVersion = 'e',
		InquireGridPerRevolution = 'a',
		InquireTimerInterruptFreq = 'b',
		InquireHighSpeedRatio = 'g',
		InquirePECPeriod = 's',
		InstantAxisStop = 'L',
		NotInstantAxisStop = 'K',
		SetAxisPositionCmd = 'E',
		GetAxisPosition = 'j',
		InquireAxisStatus = 'f',
		SetSwitch = 'O',
		SetMotionMode = 'G',
		SetGotoTargetIncrement = 'H',
		SetBreakPointIncrement = 'M',
		SetGotoTarget = 'S',
		SetBreakStep = 'U',
		SetStepPeriod = 'I',
		StartMotion = 'J',
		GetStepPeriod = 'D', // See Merlin protocol http://www.papywizard.org/wiki/DevelopGuide
		ActivateMotor = 'B', // See eq6direct implementation http://pierre.nerzic.free.fr/INDI/
		SetGuideRate = 'P',  // See EQASCOM driver
		GetHomePosition = 'd',    // Get Home position encoder count (default at startup)
		SetFeatureCmd = 'W', // EQ8/AZEQ6/AZEQ5 only
		GetFeatureCmd = 'q', // EQ8/AZEQ6/AZEQ5 only
		InquireAuxEncoder = 'd', // EQ8/AZEQ6/AZEQ5 only
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
	enum SkywatcherError { NO_ERROR, ER_1, ER_2, ER_3 };
	
	int SendSkywatcherCommand(SkywatcherCommand cmd, SkywatcherAxis Axis, char *cmdArgs, char *response, int maxlen);
	int SendSkywatcherCommandInnerLoop(SkywatcherCommand cmd, SkywatcherAxis Axis, char *cmdArgs, char *response, int maxlen);
	int ResetMotions(void);
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
	void EncoderValuesfromHAanDEC(double dHa, double dDec, unsigned long &RAEncoder, unsigned long &DEEncoder);
	
	
	// Official Skywatcher Protocol
	// See http://code.google.com/p/skywatcher/wiki/SkyWatcherProtocol
	// Constants
	static const char SkywatcherLeadingChar = ':';
	static const char SkywatcherTrailingChar = 0x0d;
	unsigned long minperiods[2];
	SkywatcherAxisStatus AxisStatus[2];
	
	// Convenience functions from INDI EQMOD which convert from/to a hex string <=>long value
	unsigned long Revu24str2long(char *); // Converts 4 character HEX number encoded as string to a long value
	unsigned long Highstr2long(char *);   // Converts 2 character HEX number encoded as string to a long value
	void long2Revu24str(unsigned long, char *); // Converts long number to character string
	long abs(long value); //Utility function since not in math.h in Unix
	
	
#ifdef SKYW_DEBUG
	// timestamp for logs
	time_t ltime;
	FILE *Logfile;	  // LogFile
#endif
	
};

