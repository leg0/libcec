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
#include "RPiCECAdapterCommunication.h"
#include "CECTypeUtils.h"

#if defined(RPI_PROXY)
    using CECSERVICE_CALLBACK_T = void(*)(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4);
    using TVSERVICE_CALLBACK_T = void(*)(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2);

    enum VC_CEC_ERROR_T
    {
        VC_CEC_SUCCESS = 0, /** OK */
        VC_CEC_ERROR_NO_ACK = 1, /** No acknowledgement */
        VC_CEC_ERROR_SHUTDOWN = 2, /** In the process of shutting down */
        VC_CEC_ERROR_BUSY = 3, /** block is busy */
        VC_CEC_ERROR_NO_LA = 4, /** No logical address */
        VC_CEC_ERROR_NO_PA = 5, /** No physical address */
        VC_CEC_ERROR_NO_TOPO = 6, /** No topology */
        VC_CEC_ERROR_INVALID_FOLLOWER = 7, /** Invalid follower */
        VC_CEC_ERROR_INVALID_ARGUMENT = 8  /** Invalid arguments */
    };

    enum VC_HDMI_NOTIFY_T
    {
        VC_HDMI_UNPLUGGED = (1 << 0),  /**<HDMI cable is detached */
        VC_HDMI_ATTACHED = (1 << 1),  /**<HDMI cable is attached but not powered on */
        VC_HDMI_DVI = (1 << 2),  /**<HDMI is on but in DVI mode (no audio) */
        VC_HDMI_HDMI = (1 << 3),  /**<HDMI is on and HDMI mode is active */
        VC_HDMI_HDCP_UNAUTH = (1 << 4),  /**<HDCP authentication is broken (e.g. Ri mismatched) or not active */
        VC_HDMI_HDCP_AUTH = (1 << 5),  /**<HDCP is active */
        VC_HDMI_HDCP_KEY_DOWNLOAD = (1 << 6),  /**<HDCP key download successful/fail */
        VC_HDMI_HDCP_SRM_DOWNLOAD = (1 << 7),  /**<HDCP revocation list download successful/fail */
        VC_HDMI_CHANGING_MODE = (1 << 8),  /**<HDMI is starting to change mode, clock has not yet been set */

    };

    enum VC_CEC_NOTIFY_T
    {
        VC_CEC_NOTIFY_NONE = 0,        //Reserved - NOT TO BE USED
        VC_CEC_TX = (1 << 0), /**<A message has been transmitted */
        VC_CEC_RX = (1 << 1), /**<A message has arrived (only for registered commands) */
        VC_CEC_BUTTON_PRESSED = (1 << 2), /**<<User Control Pressed> */
        VC_CEC_BUTTON_RELEASE = (1 << 3), /**<<User Control Release> */
        VC_CEC_REMOTE_PRESSED = (1 << 4), /**<<Vendor Remote Button Down> */
        VC_CEC_REMOTE_RELEASE = (1 << 5), /**<<Vendor Remote Button Up> */
        VC_CEC_LOGICAL_ADDR = (1 << 6), /**<New logical address allocated or released */
        VC_CEC_TOPOLOGY = (1 << 7), /**<Topology is available */
        VC_CEC_LOGICAL_ADDR_LOST = (1 << 15) /**<Only for passive mode, if the logical address is lost for whatever reason, this will be triggered */
    };

    enum VCHIQ_STATUS_T
    {
        VCHIQ_ERROR = -1,
        VCHIQ_SUCCESS = 0,
        VCHIQ_RETRY = 1
    };

    #define CEC_CB_REASON(x) ((x) & 0xFFFF) /** Get callback reason */
    #define CEC_CB_MSG_LENGTH(x) (((x) >> 16) & 0xFF) /** Get callback parameter length (this includes the header byte) */
    #define CEC_CB_RC(x) (((x) >> 24) & 0xFF) /** Get return value (only for TX callbacks for the moment) */
    #define CEC_CB_INITIATOR(x) (((x) >> 4) & 0xF) /** Get the initiator from first parameter */
    #define CEC_CB_FOLLOWER(x) ((x) & 0xF) /** Get the follower from first parameter */
    #define CEC_CB_OPCODE(x) (((x) >> 8) & 0xFF) /** Get the opcode from first parameter */
    #define CEC_CB_OPERAND1(x) (((x) >> 16) & 0xFF) /** Get the button code from <User Control Pressed> or the first operand of the opcode */
    #define CEC_CB_OPERAND2(x) (((x) >> 24) & 0xFF) /** Get the second operand of opcode */

    #define CEC_VENDOR_ID_BROADCOM   (0x18C086L)
    #define VC_CECSERVICE_VER 1
#else
    extern "C" {
    #include <bcm_host.h>
    }
#endif

#include "CECTypeUtils.h"
#include "LibCEC.h"
#include "RPiCECAdapterMessageQueue.h"


using namespace CEC;
using namespace CEC::CCECTypeUtils;
using namespace P8PLATFORM;

#define LIB_CEC m_callback->GetLib()

static bool g_bHostInited = false;

// callback for the RPi CEC service
void rpi_cec_callback(void *callback_data, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4)
{
  if (callback_data)
    static_cast<CRPiCECAdapterCommunication *>(callback_data)->OnDataReceived(p0, p1, p2, p3, p4);
}

// callback for the TV service
void rpi_tv_callback(void *callback_data, uint32_t reason, uint32_t p0, uint32_t p1)
{
  if (callback_data)
    static_cast<CRPiCECAdapterCommunication *>(callback_data)->OnTVServiceCallback(reason, p0, p1);
}

CRPiCECAdapterCommunication::CRPiCECAdapterCommunication(IAdapterCommunicationCallback *callback) :
    IAdapterCommunication(callback),
    m_bInitialised(false),
    m_logicalAddress(CECDEVICE_UNKNOWN),
    m_bLogicalAddressChanged(false),
    m_previousLogicalAddress(CECDEVICE_FREEUSE),
    m_bLogicalAddressRegistered(false),
    m_bDisableCallbacks(false)
{
  m_queue.reset(new CRPiCECAdapterMessageQueue(this));

  // recvfrom from this socket should run in separate thread
  callbackSocket = socket(AF_INET, SOCK_STREAM, 0);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in sa; // TODO: get the real rpi address from config.
  connect(s, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
}

CRPiCECAdapterCommunication::~CRPiCECAdapterCommunication(void)
{
  m_queue.reset();
  UnregisterLogicalAddress();
  Close();
  vc_cec_set_passive(false);
}

const char *ToString(const VC_CEC_ERROR_T error)
{
  switch(error)
  {
  case VC_CEC_SUCCESS:
    return "success";
  case VC_CEC_ERROR_NO_ACK:
    return "no ack";
  case VC_CEC_ERROR_SHUTDOWN:
    return "shutdown";
  case VC_CEC_ERROR_BUSY:
    return "device is busy";
  case VC_CEC_ERROR_NO_LA:
    return "no logical address";
  case VC_CEC_ERROR_NO_PA:
    return "no physical address";
  case VC_CEC_ERROR_NO_TOPO:
    return "no topology";
  case VC_CEC_ERROR_INVALID_FOLLOWER:
    return "invalid follower";
  case VC_CEC_ERROR_INVALID_ARGUMENT:
    return "invalid arg";
  default:
    return "unknown";
  }
}

bool CRPiCECAdapterCommunication::IsInitialised(void)
{
  CLockObject lock(m_mutex);
  return m_bInitialised;
}

void CRPiCECAdapterCommunication::OnTVServiceCallback(uint32_t reason, uint32_t UNUSED(p0), uint32_t UNUSED(p1))
{
  switch(reason)
  {
  case VC_HDMI_ATTACHED:
  {
    uint16_t iNewAddress = GetPhysicalAddress();
    m_callback->HandlePhysicalAddressChanged(iNewAddress);
    break;
  }
  case VC_HDMI_UNPLUGGED:
  case VC_HDMI_DVI:
  case VC_HDMI_HDMI:
  case VC_HDMI_HDCP_UNAUTH:
  case VC_HDMI_HDCP_AUTH:
  case VC_HDMI_HDCP_KEY_DOWNLOAD:
  case VC_HDMI_HDCP_SRM_DOWNLOAD:
  default:
     break;
  }
}

static char* memcpy2(char* dst, void const* src, size_t sz)
{
    return static_cast<char*>(memcpy(dst, src, sz)) + sz;
}

void CRPiCECAdapterCommunication::OnDataReceived(uint32_t header, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)
{
  VC_CEC_NOTIFY_T reason = (VC_CEC_NOTIFY_T)CEC_CB_REASON(header);

#ifdef CEC_DEBUGGING
  LIB_CEC->AddLog(CEC_LOG_DEBUG, "received data: header:%08X p0:%08X p1:%08X p2:%08X p3:%08X reason:%x", header, p0, p1, p2, p3, reason);
#endif

  switch (reason)
  {
  case VC_CEC_RX:
    // CEC data received
    {
      char payload[4 * sizeof(uint32_t)] = { 0 };
      auto p = payload;
      if (CEC_CB_MSG_LENGTH(header) > 0)
      {
          auto tmp = p0 >> 8;
          p = memcpy2(p, &tmp, sizeof(uint32_t) - 1);
          p = memcpy2(p, &p1, sizeof(uint32_t));
          p = memcpy2(p, &p2, sizeof(uint32_t));
          p = memcpy2(p, &p3, sizeof(uint32_t));
      }

      // translate to a cec_command
      cec_command command;
      cec_command::Format(command,
          (cec_logical_address)CEC_CB_INITIATOR(p0),
          (cec_logical_address)CEC_CB_FOLLOWER(p0),
          (cec_opcode)CEC_CB_OPCODE(p0));

      // copy parameters
      for (uint8_t iPtr = 1; iPtr < CEC_CB_MSG_LENGTH(header)-1; iPtr++)
        command.PushBack(payload[iPtr]);

      // send to libCEC
      m_callback->OnCommandReceived(command);
    }
    break;
  case VC_CEC_TX:
    {
      // handle response to a command that was sent earlier
      m_queue->MessageReceived((cec_opcode)CEC_CB_OPCODE(p0), (cec_logical_address)CEC_CB_INITIATOR(p0), (cec_logical_address)CEC_CB_FOLLOWER(p0), CEC_CB_RC(header));
    }
    break;
  case VC_CEC_BUTTON_PRESSED:
  case VC_CEC_REMOTE_PRESSED:
    {
      // translate into a cec_command
      cec_command command;
      cec_command::Format(command,
                          (cec_logical_address)CEC_CB_INITIATOR(p0),
                          (cec_logical_address)CEC_CB_FOLLOWER(p0),
                          reason == VC_CEC_BUTTON_PRESSED ? CEC_OPCODE_USER_CONTROL_PRESSED : CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN);
      command.parameters.PushBack((uint8_t)CEC_CB_OPERAND1(p0));

      // send to libCEC
      m_callback->OnCommandReceived(command);
    }
    break;
  case VC_CEC_BUTTON_RELEASE:
  case VC_CEC_REMOTE_RELEASE:
    {
      // translate into a cec_command
      cec_command command;
      cec_command::Format(command,
                          (cec_logical_address)CEC_CB_INITIATOR(p0),
                          (cec_logical_address)CEC_CB_FOLLOWER(p0),
                          reason == VC_CEC_BUTTON_PRESSED ? CEC_OPCODE_USER_CONTROL_RELEASE : CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP);
      command.parameters.PushBack((uint8_t)CEC_CB_OPERAND1(p0));

      // send to libCEC
      m_callback->OnCommandReceived(command);
    }
    break;
  case VC_CEC_LOGICAL_ADDR:
    {
      CLockObject lock(m_mutex);
      m_previousLogicalAddress = m_logicalAddress;
      if (CEC_CB_RC(header) == VCHIQ_SUCCESS)
      {
        m_bLogicalAddressChanged = true;
        m_logicalAddress = (cec_logical_address)(p0 & 0xF);
        m_bLogicalAddressRegistered = true;
        LIB_CEC->AddLog(CEC_LOG_DEBUG, "logical address changed to %s (%x)", ToString(m_logicalAddress), m_logicalAddress);
      }
      else
      {
        m_logicalAddress = CECDEVICE_FREEUSE;
        LIB_CEC->AddLog(CEC_LOG_DEBUG, "failed to change the logical address, reset to %s (%x)", ToString(m_logicalAddress), m_logicalAddress);
      }
      m_logicalAddressCondition.Signal();
    }
    break;
  case VC_CEC_LOGICAL_ADDR_LOST:
    {
      LIB_CEC->AddLog(CEC_LOG_DEBUG, "logical %s (%x) address lost", ToString(m_logicalAddress), m_logicalAddress);
      // the logical address was taken by another device
      cec_logical_address previousAddress = m_logicalAddress == CECDEVICE_FREEUSE ? m_previousLogicalAddress : m_logicalAddress;
      m_logicalAddress = CECDEVICE_UNKNOWN;

      // notify libCEC that we lost our LA when the connection was initialised
      bool bNotify(false);
      {
        CLockObject lock(m_mutex);
        bNotify = m_bInitialised && m_bLogicalAddressRegistered;
      }
      if (bNotify)
        m_callback->HandleLogicalAddressLost(previousAddress);
    }
    break;
  case VC_CEC_TOPOLOGY:
    break;
  default:
    LIB_CEC->AddLog(CEC_LOG_DEBUG, "ignoring unknown reason %x", reason);
    break;
  }
}

bool CRPiCECAdapterCommunication::Open(uint32_t iTimeoutMs /* = CEC_DEFAULT_CONNECT_TIMEOUT */, bool UNUSED(bSkipChecks) /* = false */, bool bStartListening)
{
  Close();

  InitHost();

  if (bStartListening)
  {
    // enable passive mode
    vc_cec_set_passive(true);

    // register the callbacks
    vc_cec_register_callback(rpi_cec_callback, this);
    vc_tv_register_callback(rpi_tv_callback, this);

    // register LA "freeuse"
    if (RegisterLogicalAddress(CECDEVICE_FREEUSE, iTimeoutMs))
    {
      LIB_CEC->AddLog(CEC_LOG_DEBUG, "%s - vc_cec initialised", __FUNCTION__);
      CLockObject lock(m_mutex);
      m_bInitialised = true;
    }
    else
    {
      LIB_CEC->AddLog(CEC_LOG_ERROR, "%s - vc_cec could not be initialised", __FUNCTION__);
      return false;
    }
  }

  return true;
}

uint16_t CRPiCECAdapterCommunication::GetPhysicalAddress(void)
{
  uint16_t iPA(CEC_INVALID_PHYSICAL_ADDRESS);
  if (!IsInitialised())
    return iPA;

  if (vc_cec_get_physical_address(&iPA) == VCHIQ_SUCCESS)
    LIB_CEC->AddLog(CEC_LOG_DEBUG, "%s - physical address = %04x", __FUNCTION__, iPA);
  else
  {
    LIB_CEC->AddLog(CEC_LOG_WARNING, "%s - failed to get the physical address", __FUNCTION__);
    iPA = CEC_INVALID_PHYSICAL_ADDRESS;
  }

  return iPA;
}

void CRPiCECAdapterCommunication::Close(void)
{
  if (m_bInitialised) {
    vc_tv_unregister_callback(rpi_tv_callback);
    m_bInitialised = false;
  }

  if (!g_bHostInited)
  {
    g_bHostInited = false;
    bcm_host_deinit();
  }
}

std::string CRPiCECAdapterCommunication::GetError(void) const
{
  return m_strError;
}

void CRPiCECAdapterCommunication::SetDisableCallback(const bool disable)
{
  CLockObject lock(m_mutex);
  m_bDisableCallbacks = disable;
}

cec_adapter_message_state CRPiCECAdapterCommunication::Write(const cec_command &data, bool &bRetry, uint8_t iLineTimeout, bool bIsReply)
{
  VC_CEC_ERROR_T vcAnswer;
  uint32_t iTimeout = (data.transmit_timeout ? data.transmit_timeout : iLineTimeout*1000);
  cec_adapter_message_state rc;

  // to send a real POLL (dest & source LA the same - eg 11), VC
  // needs us to be in passivemode(we are) and with no actual LA
  // registered
  // libCEC sends 'true' POLLs only when at LA choosing process.
  // any other POLLing of devices happens with regular 'empty'
  // msg (just header, no OPCODE) with actual LA as source to X.
  // for us this means, that libCEC already registered tmp LA
  // (0xf, 0xe respectively) before it calls us for LA POLLing.
  //
  // that means - unregistering any A from adapter, _while_
  // ignoring callbacks (and especially not reporting the
  // subsequent actions generated from VC layer - like
  // LA change to 0xf ...)
  //
  // calling vc_cec_release_logical_address() over and over is
  // fine.
  // once libCEC gets NACK on tested A, it calls RegisterLogicalAddress()
  // on it's own - so we don't need to take care of re-registering
  if (!data.opcode_set && data.initiator == data.destination)
  {
    SetDisableCallback(true);

    vc_cec_release_logical_address();
    // accept nothing else than NACK or ACK, repeat until this happens
    while (ADAPTER_MESSAGE_STATE_WAITING_TO_BE_SENT ==
          (rc = m_queue->Write(data, bRetry, iTimeout, bIsReply, vcAnswer)));

    SetDisableCallback(false);
    return rc;
  }

  rc = m_queue->Write(data, bRetry, iTimeout, bIsReply, vcAnswer);
#ifdef CEC_DEBUGGING
  LIB_CEC->AddLog(CEC_LOG_DEBUG, "sending data: result %s", ToString(vcAnswer));
#endif
  return rc;
}

uint16_t CRPiCECAdapterCommunication::GetFirmwareVersion(void)
{
#if defined(RPI_PROXY)
    return vc_cec_get_service_version();
#else
    return VC_CECSERVICE_VER;
#endif
}

cec_logical_address CRPiCECAdapterCommunication::GetLogicalAddress(void)
{
  CLockObject lock(m_mutex);

  return m_logicalAddress;
}

bool CRPiCECAdapterCommunication::UnregisterLogicalAddress(void)
{
  CLockObject lock(m_mutex);
  if (!m_bInitialised)
    return true;

  LIB_CEC->AddLog(CEC_LOG_DEBUG, "%s - releasing previous logical address", __FUNCTION__);
  {
    CLockObject lock(m_mutex);
    m_bLogicalAddressRegistered = false;
    m_bLogicalAddressChanged    = false;
  }

  vc_cec_release_logical_address();

  return m_logicalAddressCondition.Wait(m_mutex, m_bLogicalAddressChanged);
}

bool CRPiCECAdapterCommunication::RegisterLogicalAddress(const cec_logical_address address, uint32_t iTimeoutMs)
{
  {
    CLockObject lock(m_mutex);
    if ((m_logicalAddress == address) && m_bLogicalAddressRegistered)
      return true;
  }

  m_bLogicalAddressChanged = false;

  // register the new LA
  int iRetval = vc_cec_set_logical_address((CEC_AllDevices_T)address, (CEC_DEVICE_TYPE_T)CCECTypeUtils::GetType(address), CEC_VENDOR_ID_BROADCOM);
  if (iRetval != VCHIQ_SUCCESS)
  {
    LIB_CEC->AddLog(CEC_LOG_ERROR, "%s - vc_cec_set_logical_address(%X) returned %s (%d)", __FUNCTION__, address, ToString((VC_CEC_ERROR_T)iRetval), iRetval);
    UnregisterLogicalAddress();
  }
  else if (m_logicalAddressCondition.Wait(m_mutex, m_bLogicalAddressChanged, iTimeoutMs))
  {
    return true;
  }
  return false;
}

cec_logical_addresses CRPiCECAdapterCommunication::GetLogicalAddresses(void)
{
  cec_logical_addresses addresses;
  addresses.Clear();
  if (m_bLogicalAddressRegistered)
    addresses.primary = GetLogicalAddress();

  return addresses;
}

bool CRPiCECAdapterCommunication::SetLogicalAddresses(const cec_logical_addresses &addresses)
{
  // the current generation RPi only supports 1 LA, so just ensure that the primary address is registered
  return SupportsSourceLogicalAddress(addresses.primary) &&
      RegisterLogicalAddress(addresses.primary);
}

void CRPiCECAdapterCommunication::InitHost()
{
  if (!g_bHostInited)
  {
    g_bHostInited = true;
    bcm_host_init();
  }
}

#if defined(RPI_PROXY)
// Deserialize integer value from network byte order.
template <typename T>
static T deserialize(char const* p)
{
    T res = 0;
    for (auto q = reinterpret_cast<unsigned char const*>(p), end = q + sizeof(T); q != end; ++q)
    {
        res <<= 8;
        res |= *q;
    }

    return res;
}

enum FunctionId : char
{
    id_bcm_host_init = 0x01,
    id_bcm_host_deinit = 0x02,
    id_vc_cec_set_passive = 0x03,
    id_vc_cec_register_callback = 0x04,
    id_vc_cec_set_logical_address = 0x05,
    id_vc_cec_get_physical_address = 0x06,
    id_vc_cec_release_logical_address = 0x07,
    id_vc_cec_get_service_version = 0x08,
    id_vc_tv_register_callback = 0x09,
    id_vc_tv_unregister_callback = 0x0a
};

constexpr char const ResponseIdMask = '\x80';

#define B0(u32) static_cast<char>((u32 >> 24) & 0xFF)
#define B1(u32) static_cast<char>((u32 >> 16) & 0xFF)
#define B2(u32) static_cast<char>((u32 >>  8) & 0xFF)
#define B3(u32) static_cast<char>((u32 >>  0) & 0xFF)
#define SERIALIZE_U32(x) B0(x),B1(x),B2(x),B3(x)

static bool isResponseOf(FunctionId id, char responseId)
{
    return responseId == (ResponseIdMask | id);
}

template <typename R>
static std::pair<bool, R> recv_return_value(SOCKET s, FunctionId id)
{
    char response[1 + sizeof(R)];
    auto const expectedResponseSize = sizeof(response);
    auto const recvResult = ::recvfrom(s, response, expectedResponseSize, 0, nullptr, nullptr);
    if (recvResult != expectedResponseSize)
        return{ false, 0 };

    if (!isResponseOf(id, response[0]))
        return{ false, 0 };

    return{ true, deserialize<R>(&response[1]) };
}

void CRPiCECAdapterCommunication::bcm_host_init()
{
    char const cmd[] = { id_bcm_host_init };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
}

void CRPiCECAdapterCommunication::bcm_host_deinit()
{
    char const cmd[] = { id_bcm_host_deinit };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
}

int32_t CRPiCECAdapterCommunication::vc_cec_set_passive(bool enabled)
{
    char const cmd[] = { id_vc_cec_set_passive, enabled };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));

    // s is "connected" UDP socket, so we only get data from peer we're connected to.
    auto const res = recv_return_value<int32_t>(s, id_vc_cec_set_passive);
    return res.first ? res.second : -1;
}

void CRPiCECAdapterCommunication::vc_cec_register_callback(CECSERVICE_CALLBACK_T callback, void *callback_data)
{
    char const cmd[] = { id_vc_cec_register_callback };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
}

int32_t CRPiCECAdapterCommunication::vc_cec_set_logical_address(CEC_AllDevices_T logical_address, CEC_DEVICE_TYPE_T device_type, uint32_t vendor_id)
{
    char const cmd[] = { id_vc_cec_set_logical_address, logical_address, device_type, SERIALIZE_U32(vendor_id) };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));

    auto const res = recv_return_value<int32_t>(s, id_vc_cec_set_logical_address);
    return res.first ? res.second : -1;
}

int32_t CRPiCECAdapterCommunication::vc_cec_get_physical_address(uint16_t* physical_address)
{
    char const cmd[] = { id_vc_cec_get_physical_address };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));

    char response[1+sizeof(int32_t) + sizeof(uint16_t)];
    auto const recvResult = ::recvfrom(s, response, sizeof(response), 0, nullptr, nullptr);
    if (recvResult != sizeof(response))
        return -1;

    if (!isResponseOf(id_vc_cec_get_physical_address, response[0]))
        return -1;

    *physical_address = deserialize<uint16_t>(&response[5]);
    return deserialize<int32_t>(&response[1]);
}

int32_t CRPiCECAdapterCommunication::vc_cec_release_logical_address()
{
    char const cmd[] = { id_vc_cec_release_logical_address };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));

    auto const res = recv_return_value<int32_t>(s, id_vc_cec_release_logical_address);
    return res.first ? res.second : -1;
}

uint16_t CRPiCECAdapterCommunication::vc_cec_get_service_version()
{
    char const cmd[] = { id_vc_cec_get_service_version };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));

    auto const res = recv_return_value<uint16_t>(s, id_vc_cec_get_service_version);
    return res.first ? res.second : -1;
}

void CRPiCECAdapterCommunication::vc_tv_register_callback(TVSERVICE_CALLBACK_T callback, void* callback_data)
{
    char const cmd[] = { id_vc_tv_register_callback };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
}

void CRPiCECAdapterCommunication::vc_tv_unregister_callback(TVSERVICE_CALLBACK_T callback)
{
    char const cmd[] = { id_vc_tv_unregister_callback };
    ::sendto(s, cmd, sizeof(cmd), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
}

#endif // RPI_PROXY

#endif
