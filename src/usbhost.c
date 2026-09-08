/********************************** (C) COPYRIGHT *******************************
* File Name          : USBHOST.C
* Author             : WCH
* Version            : V1.1
* Date               : 2018/02/28
* Description        : CH554 USB Host interface function
*******************************************************************************/

#include <ch554.h>
#include <debug.h>
#include "usbhost.h"
#include "small_print.h"
#include <string.h>

extern __xdata __at (0x0380) uint8_t  RxBuffer[ MAX_PACKET_SIZE ];
extern __xdata __at (0x03C0) uint8_t  TxBuffer[ MAX_PACKET_SIZE ];

#include <ch554_usb.h>

__xdata uint8_t  Com_Buffer[ COM_BUF_SIZE ];      //Define a user temporary buffer, which is used
                                                  // to process descriptors during enumeration, and
                                                  // can also be used as a normal temporary buffer
                                                  // at the end of enumeration
/*******************************************************************************
* Function Name  : DisableRootHubPort( )
* Description    : Close the HUB port
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void  DisableRootHubPort( )
{
    ThisUsbDev.DeviceStatus = ROOT_DEV_DISCONNECT;
    ThisUsbDev.DeviceAddress = 0x00;
}
/*******************************************************************************
* Function Name  : AnalyzeRootHub(void)
* Description    : Analyze the ROOT-HUB status and handle the plugging and unplugging events of the ROOT-HUB port
                   If the device is unplugged, call the DisableRootHubPort() function in the function to close the port, insert the event, and set the status bit of the corresponding port
* Input          : None
* Output         : None
* Return         : Return ERR_SUCCESS for no condition, return ERR_USB_CONNECT for new connection detected, and return ERR_USB_DISCON for disconnection detected
*******************************************************************************/
uint8_t   AnalyzeRootHub( void )
{
    uint8_t    s;
    s = ERR_SUCCESS;
    if ( USB_MIS_ST & bUMS_DEV_ATTACH ) {                                        // Device exists
        if ( ThisUsbDev.DeviceStatus == ROOT_DEV_DISCONNECT                        // Device plugged in
            || ( UHOST_CTRL & bUH_PORT_EN ) == 0x00 ) {                              //A device is detected to be plugged in, but it has not been allowed, indicating that it has just been plugged in
            DisableRootHubPort( );                                                   // Close the port
//        ThisUsbDev.DeviceSpeed = USB_HUB_ST & bUHS_DM_LEVEL ? 0 : 1;
            ThisUsbDev.DeviceStatus = ROOT_DEV_CONNECTED;                            //Set the connection flag
#if DE_PRINTF
            printstr( "USB dev in\n" );
#endif
            s = ERR_USB_CONNECT;
        }
    }
    else if ( ThisUsbDev.DeviceStatus >= ROOT_DEV_CONNECTED ) {                  //Device unplug detected
        DisableRootHubPort( );                                                     // Close the port
#if DE_PRINTF
        printstr( "USB dev out\n" );
#endif
        if ( s == ERR_SUCCESS ) s = ERR_USB_DISCON;
    }
//    UIF_DETECT = 0;                                                            // Clear interrupt flag
    return( s );
}
/*******************************************************************************
* Function Name  : SetHostUsbAddr
* Description    : Set the USB device address currently operated by the USB host
* Input          : uint8_t addr
* Output         : None
* Return         : None
*******************************************************************************/
void    SetHostUsbAddr( uint8_t addr )
{
    USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | addr & 0x7F;
}

/*******************************************************************************
* Function Name  : SetUsbSpeed
* Description    : Set current USB speed
* Input          : uint8_t FullSpeed
* Output         : None
* Return         : None
*******************************************************************************/
void    SetUsbSpeed( uint8_t FullSpeed )
{
    if ( FullSpeed )                                                           // full speed
    {
        USB_CTRL &= ~ bUC_LOW_SPEED;                                           // full speed
        UH_SETUP &= ~ bUH_PRE_PID_EN;                                          // Prohibit PRE PID
    }
    else
    {
        USB_CTRL |= bUC_LOW_SPEED;                                             // Low speed
    }
}

/*******************************************************************************
* Function Name  : ResetRootHubPort( )
* Description    : After the device is detected, reset the bus to prepare for enumerating the device, and set it to default to full speed
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void  ResetRootHubPort( )
{
    UsbDevEndp0Size = DEFAULT_ENDP0_SIZE;                                      //Maximum packet size of endpoint 0 of the USB device
    memset( &ThisUsbDev,0,sizeof(ThisUsbDev));                                 //Empty structure
    SetHostUsbAddr( 0x00 );
    UHOST_CTRL &= ~bUH_PORT_EN;                                                // Turn off the port
    SetUsbSpeed( 1 );                                                          // The default is full speed
    UHOST_CTRL = UHOST_CTRL & ~ bUH_LOW_SPEED | bUH_BUS_RESET;                 // The default is full speed, start to reset
    mDelaymS( 20 );                                                            // Reset time 10mS to 20mS
    UHOST_CTRL = UHOST_CTRL & ~ bUH_BUS_RESET;                                 // End reset
    mDelayuS( 250 );
    UIF_DETECT = 0;                                                            // Clear interrupt flag
}
/*******************************************************************************
* Function Name  : EnableRootHubPort( )
* Description    : Enable the ROOT-HUB port, and the corresponding bUH_PORT_EN is set to 1 to enable the port. The device disconnection may cause the return failure
* Input          : None
* Output         : None
* Return         : Return ERR_SUCCESS to detect a new connection, return ERR_USB_DISCON to indicate no connection
*******************************************************************************/
uint8_t   EnableRootHubPort( )
{
    if ( ThisUsbDev.DeviceStatus < ROOT_DEV_CONNECTED ) ThisUsbDev.DeviceStatus = ROOT_DEV_CONNECTED;
    if ( USB_MIS_ST & bUMS_DEV_ATTACH ) {                                        // Have equipment
        if ( ( UHOST_CTRL & bUH_PORT_EN ) == 0x00 ) {                              // Not yet enabled
            ThisUsbDev.DeviceSpeed = USB_MIS_ST & bUMS_DM_LEVEL ? 0 : 1;
            if ( ThisUsbDev.DeviceSpeed == 0 ) UHOST_CTRL |= bUH_LOW_SPEED;          // Low speed
        }
        USB_CTRL |= bUC_DMA_EN;                                                    // Start the USB host and DMA, and automatically pause before the interrupt flag is cleared
        UH_SETUP = bUH_SOF_EN;
        UHOST_CTRL |= bUH_PORT_EN;                                                 //Enable HUB port
        return( ERR_SUCCESS );
    }
    return( ERR_USB_DISCON );
}

/*******************************************************************************
* Function Name  : SelectHubPort( uint8_t HubPortIndex )
* Description    : Select the HUB port to be operated
* Input          : uint8_t HubPortIndex Select the designated port of the external HUB to operate the designated ROOT-HUB port
* Output         : None
* Return         : None
*******************************************************************************/
void    SelectHubPort( )
{

        //HubLowSpeed = 0;
        SetHostUsbAddr( ThisUsbDev.DeviceAddress );                            // Set the USB address of the device the USB host is currently operating on
        SetUsbSpeed( ThisUsbDev.DeviceSpeed );                                 // Set the speed of the USB device

}

/*******************************************************************************
* Function Name  : WaitUSB_Interrupt
* Description    : Wait for USB interrupt
* Input          : None
* Output         : None
* Return         : returns ERR_SUCCESS if data received or sent successfully
                    ERR_USB_UNKNOWN if data received or sent failed
*******************************************************************************/
uint8_t WaitUSB_Interrupt( void )
{
    uint16_t  i;
    for ( i = WAIT_USB_TOUT_200US; i != 0 && UIF_TRANSFER == 0; i -- ){;}
    return( UIF_TRANSFER ? ERR_SUCCESS : ERR_USB_UNKNOWN );
}
/*******************************************************************************
* Function Name  : USBHostTransact
* Description    : CH554 transfer transaction: input the destination endpoint address/PID token, sync flag, and total NAK retry time in 20uS units (0 = no retry, 0xFFFF = infinite retry); returns 0 on success, retries on timeout/error
                    This sub-procedure is written for readability; in practical use the code should be optimized for execution speed
* Input          : uint8_t endp_pid token and address  endp_pid: upper 4 bits are the token_pid token, lower 4 bits are the endpoint address
                    uint8_t tog      sync flag
                    uint16_t timeout timeout value
* Output         : None
* Return         : ERR_USB_UNKNOWN timeout, possible hardware abnormality
                    ERR_USB_DISCON  device disconnected
                    ERR_USB_CONNECT device connected
                    ERR_SUCCESS     transfer completed
*******************************************************************************/
uint8_t   USBHostTransact( uint8_t endp_pid, uint8_t tog, uint16_t timeout )
{
    uint8_t    TransRetry;
//#define    TransRetry    UEP0_T_LEN                                                   // Save memory
    uint8_t    s, r;
    uint16_t    i;
    UH_RX_CTRL = UH_TX_CTRL = tog;
    TransRetry = 0;

    do {
        UH_EP_PID = endp_pid;                                                      // Specify the token PID and destination endpoint number
        UIF_TRANSFER = 0;                                                          // Allow transfer
//  s = WaitUSB_Interrupt( );
        for ( i = WAIT_USB_TOUT_200US; i != 0 && UIF_TRANSFER == 0; i -- );
        UH_EP_PID = 0x00;                                                          // Stop USB transfer
//    if ( s != ERR_SUCCESS ) return( s );  // Interrupt timeout, may be a hardware abnormality
        if ( UIF_TRANSFER == 0 ) return( ERR_USB_UNKNOWN );
        if ( UIF_DETECT ) {                                                        // USB device plug/unplug event
//            mDelayuS( 200 );                                                       // wait for transfer to complete
            UIF_DETECT = 0;                                                          // clear interrupt flag
            s = AnalyzeRootHub( );                                                   // analyze ROOT-HUB state
            if ( s == ERR_USB_CONNECT ) FoundNewDev = 1;
            if ( ThisUsbDev.DeviceStatus == ROOT_DEV_DISCONNECT ) return( ERR_USB_DISCON );// USB device disconnection event
            if ( ThisUsbDev.DeviceStatus == ROOT_DEV_CONNECTED ) return( ERR_USB_CONNECT );// USB device connection event
            mDelayuS( 200 );  // wait for transfer to complete
        }
        if ( UIF_TRANSFER ) {  // transfer completed
            if ( U_TOG_OK ) return( ERR_SUCCESS );
            r = USB_INT_ST & MASK_UIS_H_RES;  // USB device response status
            if ( r == USB_PID_STALL ) return( r | ERR_USB_TRANSFER );
            if ( r == USB_PID_NAK ) {
                if ( timeout == 0 ) return( r | ERR_USB_TRANSFER );
                if ( timeout < 0xFFFF ) timeout --;
                -- TransRetry;
            }
            else switch ( endp_pid >> 4 ) {
                case USB_PID_SETUP:
                case USB_PID_OUT:
//                    if ( U_TOG_OK ) return( ERR_SUCCESS );
//                    if ( r == USB_PID_ACK ) return( ERR_SUCCESS );
//                    if ( r == USB_PID_STALL || r == USB_PID_NAK ) return( r | ERR_USB_TRANSFER );
                    if ( r ) return( r | ERR_USB_TRANSFER );  // not a timeout/error, unexpected response
                    break;  // retry on timeout
                case USB_PID_IN:
//                    if ( U_TOG_OK ) return( ERR_SUCCESS );
//                    if ( tog ? r == USB_PID_DATA1 : r == USB_PID_DATA0 ) return( ERR_SUCCESS );
//                    if ( r == USB_PID_STALL || r == USB_PID_NAK ) return( r | ERR_USB_TRANSFER );
                    if ( r == USB_PID_DATA0 || r == USB_PID_DATA1 ) {  // if not in sync, discard and retry
                    }  // retry when not in sync
                    else if ( r ) return( r | ERR_USB_TRANSFER );  // not a timeout/error, unexpected response
                    break;  // retry on timeout
                default:
                    return( ERR_USB_UNKNOWN );  // impossible case
                    break;
            }
        }
        else {  // other interrupt, a case that should not happen
            USB_INT_FG = 0xFF;  /* clear interrupt flag */
        }
        mDelayuS( 15 );
    } while ( ++ TransRetry < 3 );
    return( ERR_USB_TRANSFER );  // response timeout
}
/*******************************************************************************
* Function Name  : HostCtrlTransfer
* Description    : Perform a control transfer; the 8-byte request code is in pSetupReq; DataBuf is an optional transmit/receive buffer
* Input          : P__xdata uint8_t DataBuf if data needs to be received and sent, DataBuf must point to a valid buffer to hold the following data
                    Puint8_t RetLen  the total length actually sent/received successfully is saved in the byte variable pointed to by RetLen
* Output         : None
* Return         : ERR_USB_BUF_OVER error in the IN status stage
                    ERR_SUCCESS     data exchange successful
                    other error status
*******************************************************************************/
uint8_t HostCtrlTransfer( __xdata uint8_t *DataBuf, uint8_t *RetLen )
{
    uint16_t  RemLen  = 0;
    uint8_t   s, RxLen, RxCnt, TxCnt;
    __xdata uint8_t  *pBuf;
    uint8_t  *pLen;
    pBuf = DataBuf;
    pLen = RetLen;
    mDelayuS( 200 );
    if ( pLen )
    {
        *pLen = 0;                                                              // total length actually sent/received successfully
    }
    UH_TX_LEN = sizeof( USB_SETUP_REQ );
    s = USBHostTransact( (uint8_t)(USB_PID_SETUP << 4 | 0x00), 0x00, 10000 );          // SETUP stage, 200mS timeout
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    UH_RX_CTRL = UH_TX_CTRL = bUH_R_TOG | bUH_R_AUTO_TOG | bUH_T_TOG | bUH_T_AUTO_TOG;// default DATA1
    UH_TX_LEN = 0x01;                                                           // default no data so the status stage is IN
    RemLen = (pSetupReq -> wLengthH << 8)|( pSetupReq -> wLengthL);
    if ( RemLen && pBuf )                                                       // data needs to be sent/received
    {
        if ( pSetupReq -> bRequestType & USB_REQ_TYP_IN )                       // receive
        {
            while ( RemLen )
            {
                mDelayuS( 200 );
                s = USBHostTransact( (uint8_t)(USB_PID_IN << 4 | 0x00), UH_RX_CTRL, 200000/20 );// IN data
                if ( s != ERR_SUCCESS )
                {
                    return( s );
                }
                RxLen = USB_RX_LEN < RemLen ? USB_RX_LEN : RemLen;
                RemLen -= RxLen;
                if ( pLen )
                {
                    *pLen += RxLen;                                              // total length actually sent/received successfully
                }
//              memcpy( pBuf, RxBuffer, RxLen );
//              pBuf += RxLen;
                for ( RxCnt = 0; RxCnt != RxLen; RxCnt ++ )
                {
                    *pBuf = RxBuffer[ RxCnt ];
                    pBuf ++;
                }
                if ( USB_RX_LEN == 0 || ( USB_RX_LEN & ( UsbDevEndp0Size - 1 ) ) )
                {
                    break;                                                       // short packet
                }
            }
            UH_TX_LEN = 0x00;                                                    // status stage is OUT
        }
        else                                                                     // transmit
        {
            while ( RemLen )
            {
                mDelayuS( 200 );
                UH_TX_LEN = RemLen >= UsbDevEndp0Size ? UsbDevEndp0Size : RemLen;
//              memcpy( TxBuffer, pBuf, UH_TX_LEN );
//              pBuf += UH_TX_LEN;
                if(pBuf[1] == 0x09)                                              // HID class command handling
                {
                    Set_Port = Set_Port^1;
                    *pBuf = Set_Port;
#if DE_PRINTF
                    printstr("SET_PORT  ");printx2(*pBuf);printx2(Set_Port);printlf();
#endif
                }
                for ( TxCnt = 0; TxCnt != UH_TX_LEN; TxCnt ++ )
                {
                    TxBuffer[ TxCnt ] = *pBuf;
                    pBuf ++;
                }
                s = USBHostTransact( USB_PID_OUT << 4 | 0x00, UH_TX_CTRL, 200000/20 );// OUT data
                if ( s != ERR_SUCCESS )
                {
                    return( s );
                }
                RemLen -= UH_TX_LEN;
                if ( pLen )
                {
                    *pLen += UH_TX_LEN;                                           // total length actually sent/received successfully
                }
            }
//          UH_TX_LEN = 0x01;                                                     // status stage is IN
        }
    }
    mDelayuS( 200 );
    s = USBHostTransact( ( UH_TX_LEN ? USB_PID_IN << 4 | 0x00: USB_PID_OUT << 4 | 0x00 ), bUH_R_TOG | bUH_T_TOG, 200000/20 );  // STATUS stage
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    if ( UH_TX_LEN == 0 )
    {
        return( ERR_SUCCESS );                                                    // status OUT
    }
    if ( USB_RX_LEN == 0 )
    {
        return( ERR_SUCCESS );                                                    // status IN, check the data length returned in the IN status
    }
    return( ERR_USB_BUF_OVER );                                                   // IN status stage error
}
/*******************************************************************************
* Function Name  : CopySetupReqPkg
* Description    : Copy the control transfer request packet
* Input          : P__code uint8_t pReqPkt control request packet address
* Output         : None
* Return         : None
*******************************************************************************/
void CopySetupReqPkg( __code uint8_t *pReqPkt )                                        // copy the control transfer request packet
{
    uint8_t   i;
        for ( i = 0; i != sizeof( USB_SETUP_REQ ); i ++ )
        {
            ((__xdata uint8_t *)pSetupReq)[ i ] = *pReqPkt;
            pReqPkt++;
        }
}
/*******************************************************************************
* Function Name  : CtrlGetDeviceDescr
* Description    : Get device descriptor, returned in TxBuffer
* Input          : None
* Output         : None
* Return         : ERR_USB_BUF_OVER descriptor length error
                    ERR_SUCCESS      success
                    other
*******************************************************************************/
uint8_t   CtrlGetDeviceDescr( void )
{
    uint8_t   s;
    uint8_t   len;
    UsbDevEndp0Size = DEFAULT_ENDP0_SIZE;
    CopySetupReqPkg( SetupGetDevDescr );
    s = HostCtrlTransfer( Com_Buffer, (uint8_t *)&len );                                      // perform a control transfer
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    UsbDevEndp0Size = ( (PXUSB_DEV_DESCR)Com_Buffer ) -> bMaxPacketSize0;          // maximum packet size of endpoint 0; this is a simplified handling, normally the first 8 bytes should be fetched and UsbDevEndp0Size updated immediately before continuing
    if ( len < ( (PUSB_SETUP_REQ)SetupGetDevDescr ) -> wLengthL )
    {
        return( ERR_USB_BUF_OVER );                                              // descriptor length error
    }
    return( ERR_SUCCESS );
}
/*******************************************************************************
* Function Name  : CtrlGetConfigDescr
* Description    : Get configuration descriptor, returned in TxBuffer
* Input          : None
* Output         : None
* Return         : ERR_USB_BUF_OVER descriptor length error
                    ERR_SUCCESS      success
                    other
*******************************************************************************/
uint8_t CtrlGetConfigDescr( void )
{
    uint8_t   s,len;
    CopySetupReqPkg( SetupGetCfgDescr );
    s = HostCtrlTransfer( Com_Buffer, (uint8_t *)&len );                                      // perform a control transfer
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }

    len = ( (PXUSB_CFG_DESCR)Com_Buffer ) -> wTotalLengthL;
    CopySetupReqPkg( SetupGetCfgDescr );
    pSetupReq -> wLengthL = len;                                                 // total length of the full configuration descriptor
    s = HostCtrlTransfer( Com_Buffer, (uint8_t *)&len );                                // perform a control transfer
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    return( ERR_SUCCESS );
}
/*******************************************************************************
* Function Name  : CtrlSetUsbAddress
* Description    : Set USB device address
* Input          : uint8_t addr device address
* Output         : None
* Return         : ERR_SUCCESS      success
                    other
*******************************************************************************/
uint8_t CtrlSetUsbAddress( uint8_t addr )
{
    uint8_t   s;
    CopySetupReqPkg( SetupSetUsbAddr );
    pSetupReq -> wValueL = addr;                                                // USB device address
    s = HostCtrlTransfer( NULL, NULL );                                         // perform a control transfer
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    SetHostUsbAddr( addr );                                                     // set the USB address of the device the USB host is currently operating on
    mDelaymS( 10 );                                                             // wait for the USB device to finish the operation
    return( ERR_SUCCESS );
}
/*******************************************************************************
* Function Name  : CtrlSetUsbConfig
* Description    : Set USB device configuration
* Input          : uint8_t cfg       configuration value
* Output         : None
* Return         : ERR_SUCCESS      success
                    other
*******************************************************************************/
uint8_t   CtrlSetUsbConfig( uint8_t cfg )
{
    CopySetupReqPkg( SetupSetUsbConfig );
    pSetupReq -> wValueL = cfg;                                                // USB device configuration
    return( HostCtrlTransfer( NULL, NULL ) );                                  // perform a control transfer
}
/*******************************************************************************
* Function Name  : CtrlClearEndpStall
* Description    : Clear endpoint STALL
* Input          : uint8_t endp       endpoint address
* Output         : None
* Return         : ERR_SUCCESS      success
                    other
*******************************************************************************/
uint8_t   CtrlClearEndpStall( uint8_t endp )
{
    CopySetupReqPkg( SetupClrEndpStall );                                      // clear endpoint errors
    pSetupReq -> wIndexL = endp;                                               // endpoint address
    return( HostCtrlTransfer( NULL, NULL ) );                                  // perform a control transfer
}

/*******************************************************************************
* Function Name  : CtrlSetUsbInterface
* Description    : Set USB device interface
* Input          : uint8_t cfg       configuration value
* Output         : None
* Return         : ERR_SUCCESS      success
                    other
*******************************************************************************/
uint8_t   CtrlSetUsbInterface( uint8_t cfg )
{
    CopySetupReqPkg( SetupSetUsbInterface );
    pSetupReq -> wValueL = cfg;                                                 // USB device configuration
    return( HostCtrlTransfer( NULL, NULL ) );                             // perform a control transfer
}

/*******************************************************************************
* Function Name  : CtrlGetHIDDeviceReport
* Description    : Get HID device report descriptor, returned in TxBuffer
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS success
                    other        error
*******************************************************************************/
uint8_t   CtrlGetHIDDeviceReport( uint8_t infc )
{
    uint8_t   s;
    uint8_t   len;

    CopySetupReqPkg( SetupSetHIDIdle );
    TxBuffer[4] = infc;
    s = HostCtrlTransfer( Com_Buffer, (uint8_t *)&len );                                    // perform a control transfer
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }

    CopySetupReqPkg( SetupGetHIDDevReport );
    TxBuffer[4] = infc;
    s = HostCtrlTransfer( Com_Buffer, (uint8_t *)&len );                                    // perform a control transfer
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }

    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : AnalyzeHidIntEndp
* Description    : Analyze the HID interrupt endpoint address from the descriptors; if HubPortIndex is 0 save to ROOTHUB, if non-zero save to the HUB sub-struct
* Input          : P__xdata uint8_t buf : address of the data buffer to be analyzed HubPortIndex: 0 means root HUB, non-0 means the port number under an external HUB
* Output         : None
* Return         : number of endpoints
*******************************************************************************/
uint8_t   AnalyzeHidIntEndp( __xdata uint8_t *buf)
{
    uint8_t   i, s, l;
    s = 0;

    memset( ThisUsbDev.GpVar,0,sizeof(ThisUsbDev.GpVar) );                     // clear the array

    for ( i = 0; i < ( (PXUSB_CFG_DESCR)buf ) -> wTotalLengthL; i += l )       // search for interrupt endpoint descriptors, skipping the configuration descriptor and interface descriptor
    {
        if ( ( (PXUSB_ENDP_DESCR)(buf+i) ) -> bDescriptorType == USB_DESCR_TYP_ENDP  // is an endpoint descriptor
                && ( ( (PXUSB_ENDP_DESCR)(buf+i) ) -> bmAttributes & USB_ENDP_TYPE_MASK ) == USB_ENDP_TYPE_INTER// is an interrupt endpoint
                && ( ( (PXUSB_ENDP_DESCR)(buf+i) ) -> bEndpointAddress & USB_ENDP_DIR_MASK ) )// is an IN endpoint
        {           // save the interrupt endpoint address, bit 7 is used as the sync flag, cleared to 0
        ThisUsbDev.GpVar[s] = ( (PXUSB_ENDP_DESCR)(buf+i) ) -> bEndpointAddress & USB_ENDP_ADDR_MASK;// the interrupt endpoint address, wMaxPacketSize and bInterval can also be saved as needed
#if DE_PRINTF
            printhex2(ThisUsbDev.GpVar[s]);
#endif
            s++;
            if(s >= 4) break;    //Only analyze 4 endpoints

        }
        l = ( (PXUSB_ENDP_DESCR)(buf+i) ) -> bLength;                          // current descriptor length, skip
        if ( l > 16 )
        {
            break;
        }
    }
#if DE_PRINTF
    printlf();
#endif
    return( s );
}

/*******************************************************************************
* Function Name  : AnalyzeBulkEndp
* Description    : Analyze the bulk endpoints; GpVar[0], GpVar[1] store the upload endpoints, GpVar[2], GpVar[3] store the download endpoints
* Input          : buf: address of the data buffer to be analyzed   HubPortIndex: 0 means root HUB, non-0 means the port number under an external HUB
* Output         : None
* Return         : 0
*******************************************************************************/
uint8_t   AnalyzeBulkEndp( __xdata uint8_t *buf)
{
    uint8_t   i, s1,s2, l;
    s1 = 0;s2 = 2;

    memset( ThisUsbDev.GpVar,0,sizeof(ThisUsbDev.GpVar) );                     // clear the array

    for ( i = 0; i < ( (PXUSB_CFG_DESCR)buf ) -> wTotalLengthL; i += l )       // search for interrupt endpoint descriptors, skipping the configuration descriptor and interface descriptor
    {
        if ( (( (PXUSB_ENDP_DESCR)(buf+i) ) -> bDescriptorType == USB_DESCR_TYP_ENDP)     // is an endpoint descriptor
                && ((( (PXUSB_ENDP_DESCR)(buf+i) ) -> bmAttributes & USB_ENDP_TYPE_MASK ) == USB_ENDP_TYPE_BULK))  // is an interrupt endpoint

        {
            if(( (PXUSB_ENDP_DESCR)(buf+i) ) -> bEndpointAddress & USB_ENDP_DIR_MASK )
                ThisUsbDev.GpVar[s1++] = ( (PXUSB_ENDP_DESCR)(buf+i) ) -> bEndpointAddress & USB_ENDP_ADDR_MASK;
            else
                ThisUsbDev.GpVar[s2++] = ( (PXUSB_ENDP_DESCR)(buf+i) ) -> bEndpointAddress & USB_ENDP_ADDR_MASK;

            if(s1 == 2) s1 = 1;
            if(s2 == 4) s2 = 3;
        }
        l = ( (PXUSB_ENDP_DESCR)(buf+i) ) -> bLength;                          // current descriptor length, skip
        if ( l > 16 )
        {
            break;
        }
    }
    return( 0 );
}

/*******************************************************************************
* Function Name  : InitRootDevice
* Description    : Initialize the USB device on the specified ROOT-HUB port
* Input          : uint8_t RootHubIndex specified port, built-in HUB port number 0/1
* Output         : None
* Return         :
*******************************************************************************/
uint8_t InitRootDevice( void )
{
    uint8_t   t,i, s, cfg, dv_cls, if_cls,ifc, if_cls2;
    uint8_t touchaoatm = 0;
    t = 0;
#if DE_PRINTF
    printstr( "Reset USB Port\n");
#endif
USBDevEnum:
    for(i=0;i<t;i++)
    {
        mDelaymS( 100 );
        if(t>10) return( s );
    }
    ResetRootHubPort( );                                                    // after a device is detected, reset the USB bus of the corresponding port
    for ( i = 0, s = 0; i < 100; i ++ )                                     // wait for the USB device to reconnect after reset, 100mS timeout
    {
        mDelaymS( 1 );
        if ( EnableRootHubPort( ) == ERR_SUCCESS )                          // enable the ROOT-HUB port
        {
            i = 0;
            s ++;                                                           // count while waiting for the USB device connection to stabilize
            if ( s > (20+t) )
            {
                break;                                                      // already stably connected for 15mS
            }
        }
    }
    if ( i )                                                                 // The device is not connected after reset
    {
        DisableRootHubPort( );
#if DE_PRINTF
        printstr( "Disable usb port because of disconnect\n" );
#endif
//         return( ERR_USB_DISCON );
    }
    SelectHubPort( );
#if DE_PRINTF
    printstr( "GetDevDescr: " );
#endif
    mDelaymS(t);
    s = CtrlGetDeviceDescr( );                                               // Get device descriptor
    if ( s == ERR_SUCCESS )
    {
#if DE_PRINTF
        for ( i = 0; i < ( (PUSB_SETUP_REQ)SetupGetDevDescr ) -> wLengthL; i ++ )
        {
            printx2(Com_Buffer[i]);
        }
        printlf();                                                       // Show descriptor
#endif
        ThisUsbDev.DeviceVID = (((uint16_t)((PXUSB_DEV_DESCR)Com_Buffer)->idVendorH)<<8 ) + ((PXUSB_DEV_DESCR)Com_Buffer)->idVendorL; // save VID/PID information
        ThisUsbDev.DevicePID = (((uint16_t)((PXUSB_DEV_DESCR)Com_Buffer)->idProductH)<<8 ) + ((PXUSB_DEV_DESCR)Com_Buffer)->idProductL;
        dv_cls = ( (PXUSB_DEV_DESCR)Com_Buffer ) -> bDeviceClass;               // device class code
        s = CtrlSetUsbAddress( ( (PUSB_SETUP_REQ)SetupSetUsbAddr ) -> wValueL );// set USB device address; adding RootHubIndex ensures the two HUB ports get different addresses
        if ( s == ERR_SUCCESS )
        {
            ThisUsbDev.DeviceAddress = ( (PUSB_SETUP_REQ)SetupSetUsbAddr ) -> wValueL;  // save the USB address
#if DE_PRINTF
            printstr( "GetCfgDescr: " );
#endif
            s = CtrlGetConfigDescr( );                                        // get configuration descriptor
            if ( s == ERR_SUCCESS )
            {
                cfg = ( (PXUSB_CFG_DESCR)Com_Buffer ) -> bConfigurationValue;
                ifc = ( (PXUSB_CFG_DESCR)Com_Buffer ) -> bNumInterfaces;
#if DE_PRINTF
                for ( i = 0; i < ( (PXUSB_CFG_DESCR)Com_Buffer ) -> wTotalLengthL; i ++ )
                {
                    printx2(Com_Buffer[i]);
                }
                printlf();
#endif
                                                                              // analyze the configuration descriptor, get endpoint data / each endpoint address / each endpoint size etc., update variables endp_addr and endp_size etc.
                if_cls = ( (PXUSB_CFG_DESCR_LONG)Com_Buffer ) -> itf_descr.bInterfaceClass;  // interface class code
                if_cls2 = Com_Buffer[41];

                if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_HID) && (( (PXUSB_CFG_DESCR_LONG)Com_Buffer ) -> itf_descr.bInterfaceSubClass <= 0x01) )// is an HID class device, keyboard/mouse etc.
                {
                    s = AnalyzeHidIntEndp( Com_Buffer);                    // analyze the HID interrupt endpoint address from the descriptors
#if DE_PRINTF
                    printstr( "AnalyzeHidIntEndp ");printhex2(s);printlf();
#endif
                    if_cls = ( (PXUSB_CFG_DESCR_LONG)Com_Buffer ) -> itf_descr.bInterfaceProtocol;
#if DE_PRINTF
                    printstr( "CtrlSetUsbConfig ");printhex2(cfg);
                               printstr(" class ");printhex2(if_cls);printlf();
#endif
                    s = CtrlSetUsbConfig( cfg );                          // set USB device configuration
                    if ( s == ERR_SUCCESS )
                    {
#if DE_PRINTF
                        printstr( "GetHIDReport: " );
#endif
                        for(dv_cls=0;dv_cls<ifc;dv_cls++)                   // ifc = nbinterfaces
                        {
                            s = CtrlGetHIDDeviceReport(dv_cls);                    // get the report descriptor
                            if(s == ERR_SUCCESS)
                            {
#if DE_PRINTF
                                for ( i = 0; i < 64; i++ )
                                {
                                    printx2(Com_Buffer[i]);
                                }
                                printlf();
#endif
                            }
                        }
                        //Set_Idle( );
                                                                         // need to save the endpoint info for the main program to perform USB transfers
                        ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                        if ( if_cls == 1 )
                        {
                            ThisUsbDev.DeviceType = DEV_TYPE_KEYBOARD;
                                                                         // further initialization, e.g. the keyboard indicator LED etc.
                            if(ifc > 1)
                            {
#if DE_PRINTF
                                printstr( "USB_DEV_CLASS_HID Ready\r\n" );
#endif
                                ThisUsbDev.DeviceType = USB_DEV_CLASS_HID;// Composite HID device
                                if ( if_cls2 == 2 )
                                    { ThisUsbDev.DeviceType = DEV_TYPE_MOUSE2;
                                      //ThisUsbDev.GpVar[0]=ThisUsbDev.GpVar[1];

                                      SetBootProto(0);      // Keyboard proto
                                      SetBootProto(1);
#if DE_PRINTF
                                      printstr( "MOUSE Interface : 2\r\n" );
#endif
                                    }

                            }
#if DE_PRINTF
                            printstr( "USB-Keyboard Ready\r\n" );
#endif
                            SetUsbSpeed( 1 );                            // the default is full speed

                            return( ERR_SUCCESS );
                        }
                        else if ( if_cls == 2 )
                        {
                            ThisUsbDev.DeviceType = DEV_TYPE_MOUSE;
                                                                         // to query the mouse status later, the descriptor should be analyzed to obtain the address, length and other information of the interrupt port
                            if(ifc > 1)
                            {
#if DE_PRINTF
                                printstr( "USB_DEV_CLASS_HID Ready\n" );
#endif
                                //ThisUsbDev.DeviceType = USB_DEV_CLASS_HID;// Composite HID device
                            }
                            SetBootProto(0);
#if DE_PRINTF
                            printstr( "USB-Mouse Ready\n" );
#endif
                            SetUsbSpeed( 1 );                            // The default is full speed

                            return( ERR_SUCCESS );
                        }
                        else if ( if_cls == 0 )
                        {
                            ThisUsbDev.DeviceType = DEV_TYPE_JOYSTICK;
                                                                         //In order to query the mouse status later, the descriptor should be analyzed to obtain the address, length and other information of the interrupt port
                            if(ifc > 1)
                            {
#if DE_PRINTF
                                printstr( "USB_DEV_CLASS_HID Ready\n" );
#endif
                                ThisUsbDev.DeviceType = USB_DEV_CLASS_HID;//Composite HID equipment
                            }
#if DE_PRINTF
                            printstr( "USB-Joy Ready\n" );
#endif
                            SetUsbSpeed( 1 );                            // The default is full speed

                            return( ERR_SUCCESS );
                        }
                        s = ERR_USB_UNSUPPORT;
                    }
                }
                else                                                                 // other device
                {
#if DE_PRINTF
                    printstr( "dv_cls ");printhex2(dv_cls); printlf();
                    printstr( "if_cls ");printhex2(if_cls ); printlf();
                    printstr( "if_subcls ");printhex2( ( (PXUSB_CFG_DESCR_LONG)Com_Buffer ) -> itf_descr.bInterfaceSubClass );    printlf();
#endif
                    AnalyzeBulkEndp(Com_Buffer);                                  // analyze the bulk endpoints
#if DE_PRINTF
                    for(i=0;i!=4;i++)
                    {
                        printx2(ThisUsbDev.GpVar[i]);
                    }
                    printlf();
#endif
                    s = CtrlSetUsbConfig( cfg );                                     // set USB device configuration
                    if ( s == ERR_SUCCESS )
                    {
#if DE_PRINTF
                        printx2(ThisUsbDev.DeviceVID); printx2(ThisUsbDev.DevicePID); printlf();
#endif
                    }
                }
            }
        }
    }
#if DE_PRINTF
    printstr( "InitRootDev Err = ");printhex2(s);printlf();
#endif
    ThisUsbDev.DeviceStatus = ROOT_DEV_FAILED;
    SetUsbSpeed( 1 );                                                                 // the default is full speed
    t++;
    goto USBDevEnum;
}
/*******************************************************************************
* Function Name  : EnumAllRootDevice
* Description    : Enumerate USB devices of all ROOT-HUB ports
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
uint8_t   EnumAllRootDevice( void )
{
    __idata uint8_t   s;
#if DE_PRINTF
    printstr( "EnumUSBDev\n" );
#endif
    if ( ThisUsbDev.DeviceStatus == ROOT_DEV_CONNECTED )            // The device has just been plugged in and has not been initialized
    {
        s = InitRootDevice( );                                      // Initialize/enumerate the USB devices of the specified HUB port
        if ( s != ERR_SUCCESS )
        {
            return( s );
        }
    }
    return( ERR_SUCCESS );
}

/*******************************************************************************
*Function Name: SearchTypeDevice
*Description: Search for the port number where the specified type of device is located on each port of ROOT-HUB and external HUB. If the output port number is 0xFFFF, it will not be found
*Input: uint8_t type search device type
*Output: None
*Return: The high 8 bits of the output are the ROOT-HUB port number, the low 8 bits are the port number of the external HUB, and the low 8 bits are 0, the device is directly on the ROOT-HUB port
                   Of course, you can also search according to the PID of the USB manufacturer's VID product (record the VID and PID of each device in advance), and specify the search serial number
*******************************************************************************/
uint16_t  SearchTypeDevice( uint8_t type )
{
    uint8_t  RootHubIndex;                                                          // CH554 has only one USB port, RootHubIndex = 0, only the low 8 bits of the return value need to be checked

    RootHubIndex = 0;

    if ( (ThisUsbDev.DeviceType == type) && (ThisUsbDev.DeviceStatus >= ROOT_DEV_SUCCESS) )
    {
        return( (uint16_t)RootHubIndex << 8 );                                      // type matches and enumeration succeeded, on the ROOT-HUB port
    }

    return( 0xFFFF );
}

uint8_t SetBootProto(uint8_t intf)
{
    uint8_t get[]= {0xA1,0x03,0x00,0x00,0x00,0x00,0x01,0x00};
    uint8_t set[]= {0x21,0x0b,0x00,0x00,0x00,0x00,0x00,0x00};
    uint8_t report[]= {0x21,0x09,0x00,0x02,0x00,0x00,0x01,0x00};

    uint8_t len,s;

    for ( s = 0; s != sizeof( get ); s ++ )
    {
        ((__xdata uint8_t *)pSetupReq)[ s ] = get[s];
    }
    ((__xdata uint8_t *)pSetupReq)[ 4 ]=intf;
    s = HostCtrlTransfer( Com_Buffer, &len );

    if ( s != ERR_SUCCESS )
    {
        return( s );
    }

#if DE_PRINTF
                        printstr("GetProto :"); printx2(Com_Buffer[0]); printlf();
#endif

    if (Com_Buffer[0]!=0) {
    for ( s = 0; s != sizeof( set ); s ++ )
    {
        ((__xdata uint8_t *)pSetupReq)[ s ] = set[s];
    }
    ((__xdata uint8_t *)pSetupReq)[ 4 ]=intf;
    s = HostCtrlTransfer( Com_Buffer, &len );
#if DE_PRINTF
                        printstr("SetProto : Boot"); printlf();
#endif
    }

#if 0
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }

    len=1; Com_Buffer[0]=0;
    for ( s = 0; s != sizeof( report ); s ++ )
    {
        ((__xdata uint8_t *)pSetupReq)[ s ] = report[s];
    }
    s = HostCtrlTransfer( Com_Buffer, &len );
#endif

    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : InitUSB_Host
* Description    : Initialize the USB host
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void  InitUSB_Host( void )
{
    uint8_t   i;
    IE_USB = 0;
//  LED_CFG = 1;
//  LED_RUN = 0;
    USB_CTRL = bUC_HOST_MODE;                                                    // set the mode first
    UHOST_CTRL &= ~bUH_PD_DIS;                                                   // enable host pull-down
    USB_DEV_AD = 0x00;
    UH_EP_MOD = bUH_EP_TX_EN | bUH_EP_RX_EN ;
    UH_RX_DMA = (uint16_t)RxBuffer;
    UH_TX_DMA = (uint16_t)TxBuffer;
    UH_RX_CTRL = 0x00;
    UH_TX_CTRL = 0x00;
    USB_CTRL = bUC_HOST_MODE | bUC_INT_BUSY;// | bUC_DMA_EN;                     // start the USB host and DMA, auto-pause until the interrupt flag is cleared
//  UHUB0_CTRL = 0x00;
//  UHUB1_CTRL = 0x00;
//  UH_SETUP = bUH_SOF_EN;
    USB_INT_FG = 0xFF;                                                           // clear interrupt flag
    for ( i = 0; i != 2; i ++ )
    {
        DisableRootHubPort( );                                                   // clear
    }
    USB_INT_EN = bUIE_TRANSFER | bUIE_DETECT;
//  IE_USB = 1;                                                                  // poll mode
}
