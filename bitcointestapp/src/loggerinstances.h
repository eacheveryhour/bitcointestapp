#ifndef LOGGERINSTANCES_H
#define LOGGERINSTANCES_H

#define PLOG_OMIT_LOG_DEFINES
#include <plog/Log.h>

static plog::Severity GLOBAL_LOG_SEVERITY = plog::verbose;

static void ConfigureLoggerSeverity(plog::Severity NewSeverity)
{
    GLOBAL_LOG_SEVERITY = NewSeverity;
}

enum
{
    MainLogger = 1,
    DBLogger,
    HttpLogger,
    PipeLogger
};

#endif // LOGGERINSTANCES_H
