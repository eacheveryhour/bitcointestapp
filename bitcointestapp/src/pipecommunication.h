#ifndef PIPECOMMUNICATION_H
#define PIPECOMMUNICATION_H

#include <irunnable.h>

#include <stdio.h>
#include <string>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <queue>
#include <sstream>
#include <mutex>
#include <algorithm>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <loggerinstances.h>

#define BUFFERSSIZE 1024*1024

static const std::string DELIMETERS = " .,:;/";

//Split our pipe command with different delimeters
std::vector<std::string> Split(const std::string& StringToSplit, const std::string& Delimeters)
{
   std::vector<std::string> FoundTokens;
   std::string OneToken;
   std::istringstream TokenStream(StringToSplit);

   for(auto &Delimeter : Delimeters)
   {
       while (std::getline(TokenStream, OneToken, Delimeter))
       {
          FoundTokens.push_back(OneToken);
       }
   }

   return FoundTokens;
}

static std::string StrToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

class PipeCommand
{
public:

    std::string GetCommand() const
    {
        return _CommandName;
    }

    std::string GetParameter() const
    {
        return _Parameter;
    }

    void SetCommand(const std::string Command)
    {
        _CommandName = Command;
    }

    void SetParameter(const std::string Paramater)
    {
        _Parameter = Paramater;
    }

    //This is not a real deserializer, but using a boost or protobuf here - is just an overkill;
    bool Deserialize(const void *Buffer, size_t Size)
    {
        if(Buffer && Size > 0)
        {
            std::string PipeString(static_cast<const char*>(Buffer), Size - 1);
            std::vector<std::string> TokensFound = Split(PipeString, DELIMETERS);

            if(TokensFound.size() == 1 && IsValid(TokensFound.at(0)))
            {
                SetCommand(TokensFound.at(0));
                return true;
            }

            if(TokensFound.size() == 2 && IsValid(TokensFound.at(0)))
            {
                SetCommand(TokensFound.at(0));
                SetParameter(TokensFound.at(1));
                return true;
            }
        }

        return false;
    }

    bool IsValid(const std::string &Command) const
    {
        return StrToLower(Command) == std::string("generateaddress") || StrToLower(Command) == std::string("getbalance");
    }

private:

    std::string _CommandName;
    std::string _Parameter;
};

class PipeCommunication : public IRunnable
{
public:

    PipeCommunication(std::string &PipeLocation,
                      std::string &InPipeName,
                      std::string &OutPipeName)
        : m_PipeLocation(PipeLocation),
          m_InPipeName(InPipeName),
          m_OutPipeName(OutPipeName)
    {
        InitLogger();
        OpenPipes();
        AllocateBuffers();
        IRunnable::Start();
    }

    PipeCommunication()
    {
        m_InPipeName = "testpipein";
        m_OutPipeName = "testpipeout";
        m_PipeLocation = "/tmp/";

        InitLogger();
        OpenPipes();
        AllocateBuffers();
        IRunnable::Start();
    }

    ~PipeCommunication() override
    {
        ClosePipes();
        FreeBuffers();
        RemovePipeFiles();
        IRunnable::Stop();
    }

    bool SendMessage(const std::string &InMsg)
    {
        std::lock_guard<std::mutex> lock(SendQueueGuard);

        SendindMessagesQueue.push(InMsg + "\n");
        return true;
    }

    bool RecieveMessage(PipeCommand &OutMsg)
    {
        std::lock_guard<std::mutex> lock(RecieveQueueGuard);

        if(RecievedMessagesQueue.size() > 0)
        {
            OutMsg = RecievedMessagesQueue.front();
            RecievedMessagesQueue.pop();
            return true;
        }

        return false;
    }

    //Fast command grabber on std::move semantic, to grab all messages an release mutex fast;
    void GetAllRecieved(std::queue<PipeCommand> &AllCommands)
    {
        std::lock_guard<std::mutex> lock(RecieveQueueGuard);

        if(RecievedMessagesQueue.size() > 0)
        {
            AllCommands = std::move(RecievedMessagesQueue);
        }
    }

    //File decriptor ping, kind of lol
    bool CheckConnectionIsAlive()
    {
         return (fcntl(FD_1, F_GETFD) != -1 || errno != EBADF) && (fcntl(FD_2, F_GETFD) != -1 || errno != EBADF);
    }

    void Run() override
    {
        while(true)
        {
            while (ssize_t ReadBytes = read(FD_1, RecieveBuffer, BUFFERSSIZE))
            {
                PipeCommand InputCommand;

                if(ReadBytes > 0 && InputCommand.Deserialize(RecieveBuffer, ReadBytes))
                {
                    std::lock_guard<std::mutex> lock(RecieveQueueGuard);

                    RecievedMessagesQueue.push(InputCommand);
                    memset(RecieveBuffer, 0, BUFFERSSIZE);
                }

                break;
            };

            while (SendindMessagesQueue.size() > 0)
            {
                const std::string MessageToSend = SendindMessagesQueue.front();
                ssize_t BytesWrite = write(FD_2, static_cast<const void*>(MessageToSend.c_str()), MessageToSend.size());

                if(BytesWrite == MessageToSend.size())
                {
                    std::lock_guard<std::mutex> lock(SendQueueGuard);

                    SendindMessagesQueue.pop();
                }
            };

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:

    void OpenPipes()
    {
        errno = 0;

        RemovePipeFiles();

        int Result = mkfifo((m_PipeLocation + m_InPipeName).c_str(), S_IRUSR| S_IWUSR);

        if(Result < 0)
        {
            PLOG_FATAL_(PipeLogger) << "Failed make input pipe. Pipe: " << m_PipeLocation + m_InPipeName << std::endl << std::strerror(errno);
        }

        FD_1 = open((m_PipeLocation + m_InPipeName).c_str(), O_RDWR | O_NONBLOCK);

        if(errno != 0)
        {
            PLOG_FATAL_(PipeLogger) << "Failed open input pipe. Pipe: " << m_PipeLocation + m_InPipeName << std::endl << std::strerror(errno);
        }

        Result = mkfifo((m_PipeLocation + m_OutPipeName).c_str(), S_IRUSR| S_IWUSR);

        if(Result < 0)
        {
            PLOG_FATAL_(PipeLogger) << "Failed make output pipe. Pipe: " << m_PipeLocation + m_InPipeName << std::endl << std::strerror(errno);
        }

        FD_2 = open((m_PipeLocation + m_OutPipeName).c_str(), O_RDWR | O_NONBLOCK);

        if(errno != 0)
        {
            PLOG_FATAL_(PipeLogger) << "Failed open output pipe. Pipe: " << m_PipeLocation + m_InPipeName << std::endl << std::strerror(errno);
        }

        PLOG_VERBOSE_(PipeLogger) << "Pipes configured succesfully!";
    }

    void InitLogger()
    {
        plog::init<PipeLogger>(GLOBAL_LOG_SEVERITY, "pipe.log");
    }

    void ClosePipes() const
    {
        close(FD_1);
        close(FD_2);
    }

    void RemovePipeFiles()
    {
        remove((m_PipeLocation + m_InPipeName).c_str());
        remove((m_PipeLocation + m_OutPipeName).c_str());
    }

    void AllocateBuffers()
    {
        RecieveBuffer = malloc(BUFFERSSIZE);
    }

    void FreeBuffers()
    {
        if(RecieveBuffer) free(RecieveBuffer);
    }

private:

    int FD_1 = -1, FD_2 = -1;

    std::string m_PipeLocation = "";
    std::string m_InPipeName = "";
    std::string m_OutPipeName = "";

    std::queue<PipeCommand> RecievedMessagesQueue;
    std::queue<std::string> SendindMessagesQueue;

    void *RecieveBuffer = nullptr;

    std::mutex  RecieveQueueGuard, SendQueueGuard;
};

#endif // PIPECOMMUNICATION_H
