#include "serial-cmdhandler.h"
#include "app-settings.h"
#include "utils.h"
#include <string.h>
#include <deque>
//------------------------------------------------------------------------------
#define SERIAL_CMD_HANDLER_DELIM ","
#define SERIAL_CMD_HANDLER_TERM1 '\n' // LF
#define SERIAL_CMD_HANDLER_TERM2 '\r' // CR

//------------------------------------------------------------------------------
SerialCmdHandler::SerialCmdHandler(Commander &cmdr)
    : CommandHandler(SERIAL_CMD_HANDLER_DELIM, SERIAL_CMD_HANDLER_TERM1, SERIAL_CMD_HANDLER_TERM2), m_cmdr(cmdr)
{
    // Setup callbacks for SerialCommand commands
    setCommands();
    for (auto &cmd : m_commands)
    {
        addCommand(cmd.first.c_str(), cmd.second);
    }
    addCommand("[A", std::bind(&SerialCmdHandler::bufScroll, this));
    setDefaultHandler(std::bind(&SerialCmdHandler::processCmdUnrecognized, this));
    cmdMenu();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::process()
{
    processSerial(Serial);
}

//------------------------------------------------------------------------------
void SerialCmdHandler::setCommands()
{
    m_commands = SerialCmdMap{
        {"h", std::bind(&SerialCmdHandler::cmdMenu, this)},
        {"ch", std::bind(&SerialCmdHandler::processCmdUsbChannel, this)},
        {"pwr", std::bind(&SerialCmdHandler::processCmdPower, this)},
        {"wf", std::bind(&SerialCmdHandler::processCmdWifi, this)},
        {"inf", std::bind(&SerialCmdHandler::processCmdInfo, this)},
        {"r", std::bind(&SerialCmdHandler::processCmdReset, this)},
        {"n", std::bind(&SerialCmdHandler::processCmdSetName, this)}
        /*{"es", std::bind(&SerialCmdHandler::bufScroll, this)}*/

    };
}

//------------------------------------------------------------------------------
void SerialCmdHandler::cmdMenu(void)
{
    err(">>>>>>>>>>>>>>>>>>>> USBMUX(POWER-RELAYS) by luk6xff (2022) <<<<<<<<<<<<<<<<<<<<");
    err("");
    err("Options:");
    err(" h                    Print the help message");
    err(" pwr,id,[x,][r,(y)]   Set power relay state:");
    err("                          id-RelayID number(0,1,2...) ");
    err("                          x-Off/On(0-1); r-reset; y-reset timeout value");
    err(" inf                  Print device information data");
    err(" n,name               Change name of the USBMUX:");
    err("                          name-new name string value presented in inf command");
    err("                              -maximum 20 characters");
    err(" r                    Reboot the device");
    err(">>>>>>>>>>>>>>>>>>>> USBMUX(POWER-RELAYS) by luk6xff (2022) <<<<<<<<<<<<<<<<<<<<");
    err("\n");
    AddBuforMemory();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::processCmdUsbChannel()
{

    CmdSetUsbChannelMsg msg;

    err("USBMUX Channel command not supported!");
    return;

    // Read command
    if (compareCheckStringArg("d"))
    {
        // Drop 'd' command
        readStringArg();
        msg.disableChannels = true;
    }
    else
    {
        uint8_t chNum = readIntArg();
        if (argOk)
        {
            inf("USBMUX Channel number: %d\r\n", chNum);
            msg.channelNumber = (UsbMuxDriver::UsbChannelNumber)chNum;
        }
        else
        {
            err("No USBMUX Channel number argument");
            return;
        }

        bool usbIdState = readBoolArg();
        if (argOk)
        {
            inf("USBMUX usb_id state: %d\r\n", usbIdState);
            msg.usbIdState = (UsbMuxDriver::UsbIdState)usbIdState;
        }
        else
        {
            err("No USBMUX usb_id second argument");
            return;
        }
    }

    m_cmdr.processCmdMsg(msg);
    AddBuforMemory();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::processCmdPower()
{
    CmdSetPwrRelayMsg msg;

    // Read RelayID
    const uint8_t relayId = readIntArg();
    if (!argOk)
    {
        err("No PowerRelayID applied!");
        return;
    }
    msg.relayId = relayId;

    // Is it a Reset PwrRelay command ?
    if (compareCheckStringArg("r"))
    {
        // Drop 'r' command
        readStringArg();
        msg.reset = true;

        uint32_t timeout = readIntArg();
        if (argOk)
        {
            const uint32_t maxTimeout = 30000;
            if (timeout > maxTimeout)
            {
                inf("PowerRelay[id:%d] reset timeout changed to max value: %d[ms]\r\n", relayId, timeout);
                timeout = maxTimeout;
            }
            msg.resetTimeoutMs = timeout;
            inf("PowerRelay[id:%d] reset timeout: %d[ms]\r\n", relayId, timeout);
        }
        else
        {
            inf("No PowerRelay[id:%d] timeout argument applied", relayId);
        }
    }
    else
    {
        const bool pwrRelayState = readIntArg();
        if (argOk)
        {
            msg.relayState = (pwrRelayState == false) ? PowerRelay::RelayState::RELAY_OFF : PowerRelay::RelayState::RELAY_ON;
            inf("PowerRelay[id:%d] state SET to: %s\r\n", relayId, pwrRelayState == false ? "RELAY_OFF" : "RELAY_ON");
        }
        else
        {
            wrn("No PowerRelay argument applied");
            return;
        }
    }

    m_cmdr.processCmdMsg(msg);
    AddBuforMemory();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::processCmdWifi()
{
    CmdSetWifiConfigMsg msg;

    const uint8_t wifiId = readIntArg();
    if (argOk)
    {
        msg.wifiId = wifiId;
        String ssid = readStringArg();
        if (argOk)
        {
            msg.wifiSsid = ssid;
            String pass = readStringArg();
            if (argOk)
            {
                msg.wifiPass = pass;
            }
            else
            {
                err("No WifiPass argument applied");
                return;
            }
        }
        else
        {
            err("No WifiSsid argument applied");
            return;
        }
    }
    else
    {
        err("No WifiId argument applied");
        return;
    }
    inf("New Wifi AP data will be stored - channel:%d, ssid:%s, pass:%s",
        msg.wifiId, msg.wifiSsid.c_str(), msg.wifiPass.c_str());
    m_cmdr.processCmdMsg(msg);
    AddBuforMemory();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::processCmdInfo()
{
    CmdDeviceInfoMsg msg;
    m_cmdr.processCmdMsg(msg);
    AddBuforMemory();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::processCmdSetName()
{
    // read name
    String name = readStringArg();
    if (name.length() > 19)
    {
        name = name.substring(0, NAME_LEN);
    }
    inf("Matched Name: %s", name.c_str());
    CmdDeviceSetNameMsg msg(name);
    m_cmdr.processCmdMsg(msg);
    AddBuforMemory();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::processCmdReset()
{
    CmdDeviceResetMsg msg;
    m_cmdr.processCmdMsg(msg);
    AddBuforMemory();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::processCmdUnrecognized()
{
    wrn("Non recognized USBMUX command");
    AddBuforMemory();
}

//------------------------------------------------------------------------------
void SerialCmdHandler::bufScroll(void)
{
    if (deque_index == deque_test.size()){
        deque_index = 0;
    }
    err("ostatnia komenda:");
    err(deque_test.at(deque_index));
    deque_index ++;
}



void SerialCmdHandler::AddBuforMemory()
{
    char* msg_cp = new char[sizeof(buffer)];
    memcpy(msg_cp, buffer, COMMANDHANDLER_BUFFER);
    deque_test.push_front(msg_cp);
}


