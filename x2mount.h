#pragma once
#include "../../licensedinterfaces/mountdriverinterface.h"

//Optional interfaces, uncomment and implement as required.
#include "../../licensedinterfaces/mount/slewtointerface.h"
#include "../../licensedinterfaces/mount/syncmountinterface.h"
#include "../../licensedinterfaces/mount/asymmetricalequatorialinterface.h"
#include "../../licensedinterfaces/mount/openloopmoveinterface.h"
#include "../../licensedinterfaces/mount/needsrefractioninterface.h"
//#include "../../licensedinterfaces/mount/linkfromuithreadinterface.h"
#include "../../licensedinterfaces/mount/trackingratesinterface.h"
#include "../../licensedinterfaces/parkinterface.h"
#include "../../licensedinterfaces/unparkinterface.h"
#include "../../licensedinterfaces/modalsettingsdialoginterface.h"
#include "../../licensedinterfaces/x2guiinterface.h"

// Forward declare the interfaces that the this driver is "given" by TheSkyX
class SerXInterface;
class TheSkyXFacadeForDriversInterface;
class NeedsRefractionInterface;
class SleeperInterface;
class BasicIniUtilInterface;
class LoggerInterface;
class MutexInterface;
class AbstractSyncMount;
class AsymmetricalEquatorialInterface;
class TickCountInterface;

// Include files for Skywatcher mount
#include "Skywatcher.h"

#define PARENT_KEY			"SkyWatcherMount"
#define CHILD_KEY_PORT_NAME "PortName"
#define CHILD_KEY_ALIGNMENT_CLOCK_POSITION "ClockPosition"
#define CHILD_KEY_ALIGNMENT_DEC "AlignmentDec"
#define CHILD_KEY_ALIGNMENT_HA "AlignmentHA"
#define CHILD_KEY_GUIDERATE "GuideRate"
#define CHILD_KEY_SLEWDELAY "SlewDelay"
#define MAX_PORT_NAME_SIZE 120

#define NSLEWSPEEDS 7
#define MAXSLEWNAMESIZE 20
#define NGUIDESPEEDS 4

#include <cstdio>

// #define HEQ5_DEBUG    // Define this to have log files

#ifdef HEQ5_DEBUG
#if defined(SB_WIN_BUILD)
#define HEQ5_LOGFILENAME "C:\\Users\\Colin\\Documents\\X2MountLogfile.txt"
#elif defined(SB_LINUX_BUILD)
#define HEQ5_LOGFILENAME "/tmp/Logfile.txt"
#elif defined (SB_MAC_BUILD)
#define HEQ5_LOGFILENAME "/tmp/Logfile.txt"
#endif
#endif

#if defined(SB_WIN_BUILD)
#define DEFAULT_COMPORT					"COM3"
#elif defined(SB_LINUX_BUILD)
#define DEFAULT_COMPORT					"/dev/mount"
#elif defined (SB_MAC_BUILD)
#define DEFAULT_COMPORT					"/dev/cu.KeySerial1"
#endif


/*!
\brief The X2Mount example.

\ingroup Example

Use this example to write an X2Mount driver.
*/
class X2Mount : public MountDriverInterface 
						//Optional interfaces, uncomment and implement as required.
						,public SyncMountInterface 
						,public SlewToInterface
                        ,public AsymmetricalEquatorialInterface
						,public OpenLoopMoveInterface
						// ,public NeedsRefractionInterface    // Already inherited.
						//,public LinkFromUIThreadInterface,
						,public TrackingRatesInterface 
						,public ParkInterface
						,public UnparkInterface
						,public ModalSettingsDialogInterface, public X2GUIEventInterface
{
public:
	/*!Standard X2 constructor*/
	X2Mount(const char* pszDriverSelection,
				const int& nInstanceIndex,
				SerXInterface					* pSerX, 
				TheSkyXFacadeForDriversInterface	* pTheSkyX, 
				SleeperInterface					* pSleeper,
				BasicIniUtilInterface			* pIniUtil,
				LoggerInterface					* pLogger,
				MutexInterface					* pIOMutex,
				TickCountInterface				* pTickCount);

	~X2Mount();

// Operations
public:
	
	/*!\name DriverRootInterface Implementation
	 See DriverRootInterface.*/
	//@{
	virtual DeviceType							deviceType(void)							  {return DriverRootInterface::DT_MOUNT;}
	virtual int									queryAbstraction(const char* pszName, void** ppVal) ;
	//@}
	
	/* See LinkInterface.*/
	//@{
	virtual int									establishLink(void)						;
	virtual int									terminateLink(void)						;
	virtual bool								isLinked(void) const					;
	virtual bool								isEstablishLinkAbortable(void) const	;
	//@}
	
	/*!\name DriverInfoInterface Implementation
	 See DriverInfoInterface.*/
	//@{
	virtual void								driverInfoDetailedInfo(BasicStringInterface& str) const;
	virtual double								driverInfoVersion(void) const				;
	//@}
	
	/*!\name HardwareInfoInterface Implementation
	 See HardwareInfoInterface.*/
	//@{
	virtual void deviceInfoNameShort(BasicStringInterface& str) const				;
	virtual void deviceInfoNameLong(BasicStringInterface& str) const				;
	virtual void deviceInfoDetailedDescription(BasicStringInterface& str) const	;
	virtual void deviceInfoFirmwareVersion(BasicStringInterface& str)				;
	virtual void deviceInfoModel(BasicStringInterface& str)						;
	//@}
	
	virtual int									raDec(double& ra, double& dec, const bool& bCached = false)					;
	virtual int									abort(void)																	;
	
	//Optional interfaces, uncomment and implement as required.
	
	//SyncMountInterface
	virtual int syncMount(const double& ra, const double& dec)									;
	virtual bool isSynced()																		;
	
	//SlewToInterface
	virtual int								startSlewTo(const double& dRa, const double& dDec)	;
	virtual int								isCompleteSlewTo(bool& bComplete) const				;
	virtual int								endSlewTo(void)										;
	
	//AsymmetricalEquatorialInterface
	virtual bool knowsBeyondThePole() { return true; }
	virtual int beyondThePole(bool& bYes);

	// Leave the following functions as virtual since we don't use them - the defaults are fine.
	virtual double flipHourAngle();
	virtual int gemLimits(double& dHoursEast, double& dHoursWest);
	
	//OpenLoopMoveInterface
	virtual int								startOpenLoopMove(const MountDriverInterface::MoveDir& Dir, const int& nRateIndex);
	virtual int								endOpenLoopMove(void)															;
	virtual bool							allowDiagonalMoves() {return true;}
	virtual int								rateCountOpenLoopMove(void) const												;
	virtual int								rateNameFromIndexOpenLoopMove(const int& nZeroBasedIndex, char* pszOut, const int& nOutMaxSize);
	virtual int								rateIndexOpenLoopMove(void);
	
	//NeedsRefractionInterface
	virtual bool							needsRefactionAdjustments(void);
	
	//LinkFromUIThreadInterface
	
	//TrackingRatesInterface
	virtual int setTrackingRates( const bool& bTrackingOn, const bool& bIgnoreRates, const double& dRaRateArcSecPerSec, const double& dDecRateArcSecPerSec);
	virtual int trackingRates( bool& bTrackingOn, double& dRaRateArcSecPerSec, double& dDecRateArcSecPerSec);
	
	/* Parking Interface */
	virtual bool							isParked(void);
	virtual int								startPark(const double& dAz, const double& dAlt);
	virtual int								isCompletePark(bool& bComplete) const;
	virtual int								endPark(void);
	
	
	/* Unparking Interface */
	int								startUnpark(void);
	/*!Called to monitor the unpark process.  \param bComplete Set to true if the unpark is complete, otherwise set to false.*/
	int								isCompleteUnpark(bool& bComplete) const;
	/*!Called once the unpark is complete.  This is called once for every corresponding startUnpark() allowing software implementations of unpark.*/
	int								endUnpark(void);
	
	
	// GUI Interface
	virtual int initModalSettingsDialog(void) { return 0; }
	virtual int execModalSettingsDialog(void);
	void uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent); // Process a UI event
	
	
	// Implementation
private:
	// Sky Interfaces
	SerXInterface 							*GetSerX() {return m_pSerX; }
	TheSkyXFacadeForDriversInterface		*GetTheSkyXFacadeForMounts() {return m_pTheSkyXForMounts;}
	SleeperInterface						*GetSleeper() {return m_pSleeper; }
	BasicIniUtilInterface					*GetSimpleIniUtil() {return m_pIniUtil; }
	LoggerInterface							*GetLogger() {return m_pLogger; }
	MutexInterface							*GetMutex()  {return m_pIOMutex;}
	TickCountInterface						*GetTickCountInterface() {return m_pTickCount;}
	
	// Variables to store Sky X interfaces
	int m_nPrivateMulitInstanceIndex;
	SerXInterface*							m_pSerX;
	TheSkyXFacadeForDriversInterface* 		m_pTheSkyXForMounts;
	SleeperInterface*						m_pSleeper;
	BasicIniUtilInterface*					m_pIniUtil;
	LoggerInterface*						m_pLogger;
	MutexInterface*							m_pIOMutex;
	TickCountInterface*						m_pTickCount;
	
	// Variables for Skywatcher Driver
	Skywatcher SkyW;
	
	bool m_bSynced;
	bool m_bParked;
	bool m_wasslewing;
	
	int m_HomePolarisClock;
	double m_HomeAlignmentHA;
	double m_HomeAlignmentDEC;
	bool m_bPolarisHomeAlignmentSet;
	char m_PortName[MAX_PORT_NAME_SIZE];
	bool m_bPolarisAlignmentSlew;
	int m_iST4GuideRateIndex;
	int m_iPostSlewDelay;
	
	int m_CurrentRateIndex;
	char SlewSpeedNames[NSLEWSPEEDS][MAXSLEWNAMESIZE];
	double SlewSpeeds[NSLEWSPEEDS];
	char GuideSpeedNames[NGUIDESPEEDS][MAXSLEWNAMESIZE];
	
	
#ifdef HEQ5_DEBUG
	FILE *LogFile;	  // LogFile
#endif
	
	
};
