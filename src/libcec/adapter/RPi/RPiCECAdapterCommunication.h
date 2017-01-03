#pragma once
/*
 * This file is part of the libCEC(R) library.
 *
 * libCEC(R) is Copyright (C) 2011-2015 Pulse-Eight Limited.  All rights reserved.
 * libCEC(R) is an original work, containing original code.
 *
 * libCEC(R) is a trademark of Pulse-Eight Limited.
 *
 * This program is dual-licensed; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 *
 * Alternatively, you can license this library under a commercial license,
 * please contact Pulse-Eight Licensing for more information.
 *
 * For more information contact:
 * Pulse-Eight Licensing       <license@pulse-eight.com>
 *     http://www.pulse-eight.com/
 *     http://www.pulse-eight.net/
 */

#include "env.h"
#if defined(HAVE_RPI_API)

#include "adapter/AdapterCommunication.h"
#include <p8-platform/threads/threads.h>

#define RPI_ADAPTER_VID 0x2708
#define RPI_ADAPTER_PID 0x1001

#define RPI_PROXY

#if !defined(RPI_PROXY)
    extern "C" {
    #include <interface/vmcs_host/vc_cecservice.h>
    #include <interface/vchiq_arm/vchiq_if.h>
    }
#else
    using CEC_DEVICE_TYPE_T = enum CEC_DeviceTypes : char
    {
        CEC_DeviceType_TV = 0, /**<TV only */
        CEC_DeviceType_Rec = 1, /**<Recoding device */
        CEC_DeviceType_Reserved = 2, /**<Reserved */
        CEC_DeviceType_Tuner = 3, /**<STB */
        CEC_DeviceType_Playback = 4, /**<DVD player */
        CEC_DeviceType_Audio = 5, /**<AV receiver */
        CEC_DeviceType_Switch = 6, /**<CEC switch */
        CEC_DeviceType_VidProc = 7, /**<Video processor */

        CEC_DeviceType_Invalid = 0xF, //RESERVED - DO NOT USE
    };

    using CEC_AllDevices_T = enum CEC_AllDevices : char {
        CEC_AllDevices_eTV = 0,            /**<TV only */
        CEC_AllDevices_eRec1,              /**<Address for 1st Recording Device */
        CEC_AllDevices_eRec2,              /**<Address for 2nd Recording Device */
        CEC_AllDevices_eSTB1,              /**<Address for 1st SetTop Box Device */
        CEC_AllDevices_eDVD1,              /**<Address for 1st DVD Device */
        CEC_AllDevices_eAudioSystem,       /**<Address for Audio Device */
        CEC_AllDevices_eSTB2,              /**<Address for 2nd SetTop Box Device */
        CEC_AllDevices_eSTB3,              /**<Address for 3rd SetTop Box Device */
        CEC_AllDevices_eDVD2,              /**<Address for 2nd DVD Device */
        CEC_AllDevices_eRec3,              /**<Address for 3rd Recording Device */
        CEC_AllDevices_eSTB4,              /**<10 Address for 4th Tuner Device */
        CEC_AllDevices_eDVD3,              /**<11 Address for 3rd DVD Device */
        CEC_AllDevices_eRsvd3,             /**<Reserved and cannot be used */
        CEC_AllDevices_eRsvd4,             /**<Reserved and cannot be used */
        CEC_AllDevices_eFreeUse,           /**<Free Address, use for any device */
        CEC_AllDevices_eUnRegistered = 15  /**<UnRegistered Devices */
    };
#endif

#include <memory>
#include <poll.h>

namespace CEC
{
  class CRPiCECAdapterMessageQueue;

  class CRPiCECAdapterCallbackThread : public P8PLATFORM::CThread
  {
  public:
      void* Process() override
      {
          auto const s = ::socket(AF_INET, SOCK_STREAM, 0);
          sockaddr_in sa;
          ::connect(s, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
          while (!IsStopped())
          {
              WSAPOLLFD q;
              pollfd p;
              p.fd = s;
              p.events = POLLIN;
              p.revents = 0;

              auto const pollResult = poll(&p, 1, 500);
              if (pollResult < 0)
                  break;

              if (pollResult == 1)
              {
                  if (p.revents == POLLIN)
                  {

                  }
              }
          }
          return nullptr;
      }
  };

  class CRPiCECAdapterCommunication : public IAdapterCommunication
  {
  public:
    /*!
     * @brief Create a new USB-CEC communication handler.
     * @param callback The callback to use for incoming CEC commands.
     */
    CRPiCECAdapterCommunication(IAdapterCommunicationCallback *callback);
    virtual ~CRPiCECAdapterCommunication(void);

    /** @name IAdapterCommunication implementation */
    ///{
    bool Open(uint32_t iTimeoutMs = CEC_DEFAULT_CONNECT_TIMEOUT, bool bSkipChecks = false, bool bStartListening = true);
    void Close(void);
    bool IsOpen(void) { return m_bInitialised; };
    std::string GetError(void) const;
    cec_adapter_message_state Write(const cec_command &data, bool &bRetry, uint8_t iLineTimeout, bool bIsReply);

    bool SetLineTimeout(uint8_t UNUSED(iTimeout)) { return true; };
    bool StartBootloader(void) { return false; };
    bool SetLogicalAddresses(const cec_logical_addresses &addresses);
    cec_logical_addresses GetLogicalAddresses(void);
    bool PingAdapter(void) { return m_bInitialised; };
    uint16_t GetFirmwareVersion(void);
    uint32_t GetFirmwareBuildDate(void) { return 0; };
    bool IsRunningLatestFirmware(void) { return true; };
    bool PersistConfiguration(const libcec_configuration & UNUSED(configuration)) { return false; };
    bool GetConfiguration(libcec_configuration & UNUSED(configuration)) { return false; };
    std::string GetPortName(void) { std::string strReturn("RPI"); return strReturn; };
    uint16_t GetPhysicalAddress(void);
    bool SetControlledMode(bool UNUSED(controlled)) { return true; };
    cec_vendor_id GetVendorId(void) { return CEC_VENDOR_BROADCOM; }
    bool SupportsSourceLogicalAddress(const cec_logical_address address) { return address > CECDEVICE_TV && address < CECDEVICE_BROADCAST; }
    cec_adapter_type GetAdapterType(void) { return ADAPTERTYPE_RPI; };
    uint16_t GetAdapterVendorId(void) const { return RPI_ADAPTER_VID; }
    uint16_t GetAdapterProductId(void) const { return RPI_ADAPTER_PID; }
    void SetActiveSource(bool UNUSED(bSetTo), bool UNUSED(bClientUnregistered)) {}
    ///}

    bool IsInitialised(void);
    void OnDataReceived(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4);
    void OnTVServiceCallback(uint32_t reason, uint32_t p0, uint32_t p1);

    void InitHost();

  private:

#if defined(RPI_PROXY)
    
    SOCKET s;
    sockaddr_in to;
    SOCKET callbackSocket;

    using CECSERVICE_CALLBACK_T = void(*)(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4);
    using TVSERVICE_CALLBACK_T = void(*)(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2);
    void bcm_host_init();
    void bcm_host_deinit();

    int32_t vc_cec_set_passive(bool enabled);
    void vc_cec_register_callback(CECSERVICE_CALLBACK_T callback, void *callback_data);
    int32_t vc_cec_set_logical_address(CEC_AllDevices_T logical_address, CEC_DEVICE_TYPE_T device_type, uint32_t vendor_id);
    int32_t vc_cec_get_physical_address(uint16_t* physical_address);
    int32_t vc_cec_release_logical_address();
    uint16_t vc_cec_get_service_version();

    void vc_tv_register_callback(TVSERVICE_CALLBACK_T callback, void* callback_data);
    void vc_tv_unregister_callback(TVSERVICE_CALLBACK_T callback);
#endif

    cec_logical_address GetLogicalAddress(void);
    bool UnregisterLogicalAddress(void);
    bool RegisterLogicalAddress(const cec_logical_address address, uint32_t iTimeoutMs = CEC_DEFAULT_CONNECT_TIMEOUT);
    void SetDisableCallback(const bool disable);

    bool m_bInitialised;   /**< true when the connection is initialised, false otherwise */
    std::string m_strError; /**< current error message */
    std::unique_ptr<CRPiCECAdapterMessageQueue> m_queue;
    cec_logical_address         m_logicalAddress;

    bool                          m_bLogicalAddressChanged;
    P8PLATFORM::CCondition<bool>  m_logicalAddressCondition;
    P8PLATFORM::CMutex            m_mutex;
    //VCHI_INSTANCE_T               m_vchi_instance;
    //VCHI_CONNECTION_T *           m_vchi_connection;
    cec_logical_address           m_previousLogicalAddress;
    bool                          m_bLogicalAddressRegistered;

    bool                          m_bDisableCallbacks;
  };
};

#endif
