#include "LogWrapper.h"

namespace logger
{
	std::shared_ptr<spdlog::logger> g_logger = spdlog::basic_logger_mt("logger", utils::GetPluginLogFile().data(), true);

	std::recursive_mutex g_console_logger_mutex;

	void c_printf(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		{
			std::lock_guard<std::recursive_mutex> lock(g_console_logger_mutex);
			RE::ConsoleLog::GetSingleton()->Print(fmt, args);
		}
		va_end(args);
	}
}