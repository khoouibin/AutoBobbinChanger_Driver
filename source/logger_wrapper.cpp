#include "logger_wrapper.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <algorithm>
#include <stdarg.h>
#include <cstdlib>
#include "bl_return_code.h"
#include <mutex>
#include <unistd.h>
#include <cstdio>

static std::mutex gmtxLogCall;
static std::mutex gmtxLogQueue;
LoggerWrapper goDriverLogger("DriverLogger");

const std::string GetCurrentDateTimeForFileName()
{
    char buffer[26];
    int millisec;
    struct tm *tm_info;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    millisec = lrint(tv.tv_usec / 1000.0);
    if (millisec >= 1000)
    {
        millisec -= 1000;
        tv.tv_sec++;
    }

    tm_info = localtime(&tv.tv_sec);

    strftime(buffer, 26, "%Y_%m_%d_%H_%M", tm_info);
    std::string str_dt = std::string(buffer);

    return str_dt;
}

const std::string currentTimestamp()
{
    char buffer[26];
    int millisec;
    struct tm *tm_info;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    /*    millisec = lrint(tv.tv_usec / 1000.0);
    if (millisec >= 1000)
    {
    millisec -= 1000;
    tv.tv_sec++;
    }
*/
    tm_info = localtime(&tv.tv_sec);

    strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
    std::string str_dt = std::string(buffer);

    char szTimeStamp[64];
    snprintf(szTimeStamp, sizeof(szTimeStamp), "%s.%06ld", buffer, tv.tv_usec);
    // std::string str_msec = std::to_string(millisec);
    return szTimeStamp;
}

LoggerWrapper::LoggerWrapper(const std::string &strName)
{
#ifdef _DEBUG
    m_bFileDebug = false;
    m_bFileInfo = true;
    m_bFileWarning = true;
    m_bFileError = true;
    m_bDisplayDebug = true;
    m_bDisplayInfo = true;
    m_bDisplayWarning = true;
    m_bDisplayError = true;
#else
    m_bFileDebug = false;
    m_bFileInfo = true;
    m_bFileWarning = true;
    m_bFileError = true;
    m_bDisplayDebug = false;
    m_bDisplayInfo = false;
    m_bDisplayWarning = true;
    m_bDisplayError = true;
#endif

    // 預設是將 Log 發送給 Host, 不儲存在本地端.
    m_bRemoteLogger = true;
    m_bLocalLogger = false;

    m_strLocalLogPath = "LOG/UHF/";
    m_strModuleName = "RTC_Driver";

    m_iRemotePort = 9091;
    // name a local log file.
    // m_strLocalLogPathFilename = m_strLocalLogPath + GetCurrentDateTimeForFileName() + "_rtc_driver.log";
    m_strLocalLogPathFilename = m_strLocalLogPath + "rtc_driver.log";
    // build local log file folder.
    mkdir("LOG", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("LOG/UHF", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    m_bIsAlive = true;
}

// bFileDebug: 是否要將 Debug Log 傳送給 Host, 或儲存在本地端.
// bFileInfo: 是否要將 Info Log 傳送給 Host, 或儲存在本地端.
// bFileWarning: 是否要將 Warning Log 傳送給 Host, 或儲存在本地端.
// bFileError: 是否要將 Error Log 傳送給 Host, 或儲存在本地端.
// bDisplayDebug: Driver是否要將 Debug Log 顯示在 Terminal 上.
// bDisplayInfo: Driver是否要將 Info Log 顯示在 Terminal 上.
// bDisplayWarning: Driver是否要將 Warning Log 顯示在 Terminal 上.
// bDisplayError: Driver是否要將 ErrorLog 顯示在 Terminal 上.
void LoggerWrapper::SetLogLevel(bool bFileDebug, bool bFileInfo, bool bFileWarning, bool bFileError, bool bDisplayDebug, bool bDisplayInfo, bool bDisplayWarning, bool bDisplayError)
{
    m_bFileDebug = bFileDebug;
    m_bFileInfo = bFileInfo;
    m_bFileWarning = bFileWarning;
    m_bFileError = bFileError;
    m_bDisplayDebug = bDisplayDebug;
    m_bDisplayInfo = bDisplayInfo;
    m_bDisplayWarning = bDisplayWarning;
    m_bDisplayError = bDisplayError;
}

int LoggerWrapper::Log(string strLevel, string strMessage)
{
    return Log(strLevel, strMessage.c_str());
}

// 參數： strLevel=DEBUG, INFO, WARNING, ERROR.
//       szFormat=第二個參數起為可變數量參數, 用法同 printf. 會組合出參數的訊息.
// 傳回值：0＝OK.
//		  小於0＝錯誤碼.
// 說明：指定 Dry Run 速度檔位.
//      依據 m_bRemoteLogger 和 m_bLocalLogger 來決定要把 Log 送給 Host 或是存在本地端.
//      但如果發送給 Host 失敗, 則設定 m_bLocalLogger＝true 儲存在本地端.
int LoggerWrapper::Log(string strLevel, const char *szFormat, ...)
{
    // 依據 m_bGlobalLogger 和 m_bLocalLogger 來決定要把 Log 送給 Host 或是存在本地端.
    // 但如果發送給 Host 失敗, 則設定 m_bLocalLogger＝true.
    int32_t iReturn;
    char szMessage[_MAX_LOG_BYTE];
    va_list arg_ptr;

    while (!gmtxLogCall.try_lock())
    {
        usleep(500);
    }

    // 可變長度參數不能直接傳遞(雖然有方法, 但是太麻煩了), 所以這裡先組合成字串.
    va_start(arg_ptr, szFormat);
    vsprintf(szMessage, szFormat, arg_ptr);
    va_end(arg_ptr);

#ifdef _DEBUG
    if (strlen(szMessage) >= (_MAX_LOG_BYTE - 1))
    {
        DisplayAndQueueLog("ERROR", "Message is too long to log.");
        gmtxLogCall.unlock();
        return -1;
    }
#endif

    DisplayAndQueueLog(strLevel, szMessage);

    gmtxLogCall.unlock();
    return 0;
}

// 參數： strLevel=DEBUG, INFO, WARNING, ERROR.
//       szFormat=第二個參數起為可變數量參數, 用法同 printf. 會組合出參數的訊息.
// 傳回值：0＝OK.
//		  小於0＝錯誤碼.
// 說明： 依據 SetLogLevel() 的設定, 將 Log 資料顯示到 Terminal 上, 或是放到 m_LogQueue 裡.
//       放到 m_LogQueue 裡的 Log 隨後會在 runTask() 裡, 依據 m_bRemoteLogger 和 m_bLocalLogger 來決定要發送給 Host 或儲存在本地端.
// int32_t LoggerWrapper::DisplayAndQueueLog( string strLevel, const char* szFormat, ...)
int32_t LoggerWrapper::DisplayAndQueueLog(string strLevel, string strMessage)
{
    int32_t iReturn = 0;

    std::string strTimestamp = currentTimestamp();
    std::string strLevellower = strLevel;
    std::transform(strLevellower.begin(), strLevellower.end(), strLevellower.begin(), ::tolower);

    bool bFileLog = false;
    bool bDisplayLog = false;

    if (strLevellower == "debug")
    {
        bFileLog = m_bFileDebug ? true : false;
        bDisplayLog = m_bDisplayDebug ? true : false;
    }
    else
    {
        if (strLevellower == "info")
        {
            bFileLog = m_bFileInfo ? true : false;
            bDisplayLog = m_bDisplayInfo ? true : false;
        }
        else
        {
            if (strLevellower == "warning")
            {
                bFileLog = m_bFileWarning ? true : false;
                bDisplayLog = m_bDisplayWarning ? true : false;
            }
            else
            {
                if (strLevellower == "error")
                {
                    bFileLog = m_bFileError ? true : false;
                    bDisplayLog = m_bDisplayError ? true : false;
                }
                else
                {
                    printf("WARNING!! Unknown log level.\n");
                }
            }
        }
    }

    if (bDisplayLog)
    {
        printf("%s [%s]: [%s]%s\n", strTimestamp.c_str(), strLevellower.c_str(), m_strModuleName.c_str(), strMessage.c_str());
    }

    if (bFileLog && (m_bRemoteLogger || m_bLocalLogger))
    {
        LOG_DATA_t tLogData;
        tLogData.strLevel = strLevellower;
        tLogData.strMessage = strMessage;
        tLogData.strTimestamp = strTimestamp;

        while (!gmtxLogQueue.try_lock())
        {
            usleep(500);
        }
        m_LogQueue.push(tLogData);
        gmtxLogQueue.unlock();

        m_evStoreLog.set();
    }

    return 0;
}

void LoggerWrapper::stop()
{
    m_bIsAlive = false;
    m_evStoreLog.set();
}

// 在主程式 main() 裡以 tm.start(&goDriverLogger) 啟動.
// _MAX_LOCAL_LOG_BYTE 是最大 Log 檔案大小限制, 如果 Log 檔案超過這個大小, 會進行刪除較舊 Log 保留較新 Log 的動作, 以縮減 Log 檔案大小.
#define _MAX_LOCAL_LOG_BYTE 5000000
void LoggerWrapper::run()
{
    int count = 0;
    int iReturn;
    LOG_DATA_t tLogData;
    static int iWriteFileCount = 0;

    //printf("Start LoggerWrapper::runTask()\n");
    while ((m_bRemoteLogger || m_bLocalLogger || m_LogQueue.size() > 0))
    {
        if (m_LogQueue.size() == 0)
        {
            if (m_bIsAlive == false)
                break;
            else
                m_evStoreLog.wait();
        }
        else
        {
            while (!gmtxLogQueue.try_lock())
            {
                usleep(500);
            }
            tLogData = m_LogQueue.front();
            m_LogQueue.pop();
            gmtxLogQueue.unlock();

            // 發送給 Host.
            if (m_bRemoteLogger)
            {
                try
                {
                    std::shared_ptr<TTransport> socket(new TSocket("localhost", m_iRemotePort));
                    std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
                    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
                    LoggerClient client(protocol);

                    transport->open();
                    iReturn = client.orisolLog(tLogData.strLevel, m_strModuleName, tLogData.strMessage);
                    transport->close();
                }
                catch (TException &tx)
                {
                    Log("INFO", "Fail to send log to Host, port=%d. Save log in %s.", m_iRemotePort, m_strLocalLogPathFilename.c_str());
                    // 如果發送給 Host 失敗, 就改儲存在本地端.
                    m_bRemoteLogger = false;
                    m_bLocalLogger = true;
                }
            }
            // 儲存在本地端.
            if (m_bLocalLogger)
            {
                string strLogMessage;
                strLogMessage = tLogData.strTimestamp + " [" + tLogData.strLevel + "]:" + " [" + m_strModuleName + "] " + tLogData.strMessage;

                try
                {
                    std::ofstream ofs;
                    iWriteFileCount++;
                    // 儲存一行Log.
                    ofs.open(m_strLocalLogPathFilename, std::ofstream::out | std::ofstream::app);
                    ofs << strLogMessage << std::endl;
                    ofs.close();
                    // 每隔一段次數檢查一下 Log 檔案大小是否達到上限.
                    if (iWriteFileCount >= 30000)
                    {
                        iWriteFileCount = 0;

                        std::ifstream fsSource;
                        std::ofstream fsTarget;

                        fsSource.open(m_strLocalLogPathFilename, std::ifstream::in);
                        fsSource.seekg(0, fsSource.end);
                        size_t iFileSize = fsSource.tellg();
                        if (iFileSize > _MAX_LOCAL_LOG_BYTE)
                        {
                            string strTempLocalLogPathFilename = m_strLocalLogPath + "temp.log";
                            // 如果 Log 檔案大小達到上限, 則只留下原有 Log 的"最大檔案大小限制"的一半.
                            fsSource.seekg(-(_MAX_LOCAL_LOG_BYTE / 2), fsSource.end);
                            // 由於移動檔案指標是用 byte 在算, 不一定能移動到一行 Log 的開頭, 通常是移動到一行 Log 的中間某位置, 但是不想留下半行的 Log,
                            // 所以做一個讀取一行 Log 的動作, 就能把檔案指標移到下一行的開頭.
                            string strOneLineLog;
                            getline(fsSource, strOneLineLog);
                            // 把要留下的 Log 存到暫時的 Log 檔.
                            fsTarget.open(strTempLocalLogPathFilename, std::ifstream::out | std::ifstream::trunc);
                            while (getline(fsSource, strOneLineLog))
                            {
                                fsTarget << strOneLineLog << std::endl;
                            }
                            fsTarget.close();
                            fsSource.close();
                            remove(m_strLocalLogPathFilename.c_str());                                      // 刪除原有的 Log 檔.
                            rename(strTempLocalLogPathFilename.c_str(), m_strLocalLogPathFilename.c_str()); // 把暫時的 Log 檔名改為正式的 Log 檔名.
                        }
                        else
                        {
                            fsSource.close();
                        }
                    }
                }
                catch (TException &tx)
                {
                }
            }
        }
    }
    printf("Exit LoggerWrapper::runTask()\n");
}
#undef _MAX_LOCAL_LOG_BYTE
/*
void LoggerWrapper::Close()
{
    m_bRemoteLogger = false;
    m_bLocalLogger = false;

    m_evStoreLog.set();
//    usleep(500);
}
*/
Poco::Thread goDriverLoggerThread;
void InitLoggerWrapper()
{
    goDriverLoggerThread.start(goDriverLogger);
}

void StopLoggerWrapper()
{
    goDriverLogger.stop();
    goDriverLoggerThread.join();
}