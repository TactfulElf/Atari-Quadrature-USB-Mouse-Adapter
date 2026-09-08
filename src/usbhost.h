// Return status codes of each sub-procedure
#define ERR_SUCCESS         0x00    // Operation successful
#define ERR_USB_CONNECT     0x15    /* USB device connection detected, already connected */
#define ERR_USB_DISCON      0x16    /* USB device disconnection detected, already disconnected */
#define ERR_USB_BUF_OVER    0x17    /* USB transfer data error or data overflow of the buffer */
#define ERR_USB_DISK_ERR    0x1F    /* USB storage operation failed; during init it may be an unsupported USB storage device, during read/write it may be a corrupted disk or the device has disconnected */
#define ERR_USB_TRANSFER    0x20    /* More error codes such as NAK/STALL are in 0x20~0x2F */
#define ERR_USB_UNSUPPORT   0xFB    /* Unsupported USB device */
#define ERR_USB_UNKNOWN     0xFE    /* Device operation error */
#define ERR_AOA_PROTOCOL    0x41    /* Protocol version error */

/* USB device information table; CH554 supports at most 1 device */
#define ROOT_DEV_DISCONNECT  0
#define ROOT_DEV_CONNECTED   1
#define ROOT_DEV_FAILED      2
#define ROOT_DEV_SUCCESS     3
#define DEV_TYPE_KEYBOARD   ( USB_DEV_CLASS_HID | 0x20 )
#define DEV_TYPE_MOUSE      ( USB_DEV_CLASS_HID | 0x30 )
#define DEV_TYPE_MOUSE2     ( USB_DEV_CLASS_HID | 0x40 )
#define DEV_TYPE_JOYSTICK   ( USB_DEV_CLASS_HID | 0x50 )
#define DEF_AOA_DEVICE       0xF0


/*
Convention: USB device address assignment rules (see USB_DEVICE_ADDR)
Address  Device location
0x02     USB device under the built-in Root-HUB, or an external HUB
0x1x     USB device on port x of an external HUB under the built-in Root-HUB, x is 1~n
*/
#define HUB_MAX_PORTS       4
#define WAIT_USB_TOUT_200US     533   // Was 400 for 200uS@Fsys=12MHz

/* Array size definitions */
#define COM_BUF_SIZE            120   // Can be adjusted to the maximum descriptor size to save memory.

extern __code uint8_t  SetupGetDevDescr[];    /* Get device descriptor */
extern __code uint8_t  SetupGetCfgDescr[];    /* Get configuration descriptor */
extern __code uint8_t  SetupSetUsbAddr[];     /* Set USB address */
extern __code uint8_t  SetupSetUsbConfig[];   /* Set USB configuration */
extern __code uint8_t  SetupSetUsbInterface[];/* Set USB interface configuration */
extern __code uint8_t  SetupClrEndpStall[];   /* Clear endpoint STALL */
#ifndef DISK_BASE_BUF_LEN
extern __code uint8_t  SetupGetHubDescr[];    /* Get HUB descriptor */
extern __code uint8_t  SetupSetHIDIdle[];
extern __code uint8_t  SetupGetHIDDevReport[];/* Get HID device report descriptor */
extern __code uint8_t  XPrinterReport[];      /* Printer class commands */
#endif
extern __xdata uint8_t  UsbDevEndp0Size;       /* Maximum packet size of endpoint 0 of the USB device */

extern __code uint8_t  GetProtocol[];         // AOA get protocol version
extern __code uint8_t  TouchAOAMode[];        // Start accessory mode
extern __code uint8_t  Sendlen[];             /* AOA-related array definitions */
extern __code uint8_t  StringID[];            // String ID, string info related to the phone APP
extern __code uint8_t  SetStringID[];         // Application index string command

#ifndef DISK_BASE_BUF_LEN
typedef struct
{
    uint8_t   DeviceStatus;              // Device status: 0 - no device, 1 - present but not yet initialized, 2 - present but init/enumeration failed, 3 - present and init/enumeration succeeded
    uint8_t   DeviceAddress;             // USB address assigned to the device
    uint8_t   DeviceSpeed;               // 0 = low speed, non-0 = full speed
    uint8_t   DeviceType;                // Device type
    uint16_t  DeviceVID;
    uint16_t  DevicePID;
    uint8_t   GpVar[4];                    // General-purpose variable, holds endpoints
    uint8_t   GpHUBPortNum;                // General-purpose variable; if it is a HUB, indicates the number of HUB ports
} _RootHubDev;

typedef struct
{
    uint8_t   DeviceStatus;             // Device status: 0 - no device, 1 - present but not yet initialized, 2 - present but init/enumeration failed, 3 - present and init/enumeration succeeded
    uint8_t   DeviceAddress;            // USB address assigned to the device
    uint8_t   DeviceSpeed;              // 0 = low speed, non-0 = full speed
    uint8_t   DeviceType;               // Device type
    uint16_t  DeviceVID;
    uint16_t  DevicePID;
    uint8_t   GpVar[4];                    // General-purpose variables
} _DevOnHubPort;                      // Assumption: no more than 1 external HUB; each external HUB has no more than HUB_MAX_PORTS ports (extra ports ignored)

extern __xdata _RootHubDev ThisUsbDev;
//extern __xdata _DevOnHubPort DevOnHubPort[HUB_MAX_PORTS];// Assumption: no more than 1 external HUB; each external HUB has no more than HUB_MAX_PORTS ports (extra ports ignored)
extern uint8_t Set_Port;
#endif


//extern __xdata uint8_t  RxBuffer[];                   // IN, must even address
//extern __xdata uint8_t  TxBuffer[];                   // OUT, must even address
extern __xdata uint8_t  Com_Buffer[];
extern __bit     FoundNewDev;
extern __bit     HubLowSpeed;                  // low speed device under a HUB needs special handling

#define pSetupReq   ((PXUSB_SETUP_REQ)TxBuffer)


void    DisableRootHubPort( );                        // Disable ROOT-HUB port; the hardware has already closed it automatically, this only clears some structural state
uint8_t   AnalyzeRootHub( void );                       // Analyze ROOT-HUB state, handle device plug/unplug events on the ROOT-HUB port
// ERR_SUCCESS returned if nothing happened; ERR_USB_CONNECT if a new connection was detected; ERR_USB_DISCON if a disconnection was detected
void    SetHostUsbAddr( uint8_t addr );                 // Set the USB address of the device the USB host is currently operating on
void    SetUsbSpeed( uint8_t FullSpeed );               // Set the current USB speed
void    ResetRootHubPort( );                          // After a device is detected, reset the bus on the corresponding port to prepare for enumeration; default is set to full speed
uint8_t   EnableRootHubPort( );                         // Enable the ROOT-HUB port, set the corresponding bUH_PORT_EN to 1 to open the port; device disconnection may cause the return to fail
void    SelectHubPort( );// If HubPortIndex=0 select the specified ROOT-HUB port to operate on, otherwise select the specified port of the external HUB on the specified ROOT-HUB port
uint8_t   WaitUSB_Interrupt( void );                    // Wait for USB interrupt
// CH554 transfer transaction: input the destination endpoint address/PID token, sync flag, and total NAK retry time in 20uS units (0 = no retry, 0xFFFF = infinite retry); returns 0 on success, retries on timeout/error
uint8_t   USBHostTransact( uint8_t endp_pid, uint8_t tog, uint16_t timeout );  // endp_pid: upper 4 bits are the token_pid token, lower 4 bits are the endpoint address
uint8_t   HostCtrlTransfer( __xdata uint8_t *DataBuf, uint8_t *RetLen );  // Perform a control transfer; the 8-byte request code is in pSetupReq; DataBuf is an optional transmit/receive buffer
// If data needs to be received and sent, DataBuf must point to a valid buffer to hold the following data; the total length actually sent/received successfully is returned and saved in the byte variable pointed to by ReqLen
void    CopySetupReqPkg( __code uint8_t *pReqPkt );            // Copy the control transfer request packet
uint8_t   CtrlGetDeviceDescr( void );                    // Get device descriptor, returned in TxBuffer
uint8_t   CtrlGetConfigDescr( void );                    // Get configuration descriptor, returned in TxBuffer
uint8_t   CtrlSetUsbAddress( uint8_t addr );               // Set USB device address
uint8_t   CtrlSetUsbConfig( uint8_t cfg );                 // Set USB device configuration
uint8_t   CtrlClearEndpStall( uint8_t endp );              // Clear endpoint STALL

#ifndef DISK_BASE_BUF_LEN
uint8_t   CtrlSetUsbInterface( uint8_t cfg );              // Set USB device interface
uint8_t   CtrlGetHIDDeviceReport( uint8_t infc );          // HID class commands: SET_IDLE and GET_REPORT
uint8_t   CtrlGetHubDescr( void );                       // Get HUB descriptor, returned in TxBuffer
uint8_t   HubGetPortStatus( uint8_t HubPortIndex );        // Query HUB port status, returned in TxBuffer
uint8_t   HubSetPortFeature( uint8_t HubPortIndex, uint8_t FeatureSelt );  // Set HUB port feature
uint8_t   HubClearPortFeature( uint8_t HubPortIndex, uint8_t FeatureSelt );  // Clear HUB port feature
uint8_t   CtrlGetXPrinterReport1( void ) ;               // Printer class commands
uint8_t   AnalyzeHidIntEndp( __xdata uint8_t *buf);           // Analyze the HID interrupt endpoint address from the descriptor
uint8_t   AnalyzeBulkEndp( __xdata uint8_t *buf) ;           // Analyze the bulk endpoint
uint8_t   TouchStartAOA( void );                         // Try AOA start
uint8_t   EnumAllRootDevice( void );                     // Enumerate the USB devices on all ROOT-HUB ports
uint8_t   InitDevOnHub(uint8_t HubPortIndex );             // Initialize and enumerate the second-level USB device behind an external HUB
uint8_t   EnumHubPort( );                                // Enumerate the ports of the external HUB hub on the specified ROOT-HUB port; check each port for connect/remove events and initialize the second-level USB device
uint8_t   EnumAllHubPort( void );                        // Enumerate the second-level USB devices behind external HUBs on all ROOT-HUB ports
uint16_t  SearchTypeDevice( uint8_t type );                // Search the ROOT-HUB and each external HUB port for the port number of a device of the given type; 0xFFFF is returned if not found
                                                       // Upper 8 bits of output = ROOT-HUB port number, lower 8 bits = external HUB port number; lower 8 bits of 0 means the device is directly on a ROOT-HUB port
uint8_t SETorOFFNumLock(uint8_t *buf);
uint8_t SetBootProto(uint8_t intf);
#endif

uint8_t   InitRootDevice( void );                        // Initialize the USB device on the specified ROOT-HUB port
void    InitUSB_Host( void );                          // Initialize the USB host
