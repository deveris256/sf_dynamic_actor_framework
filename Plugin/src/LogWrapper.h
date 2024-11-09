#pragma once
#include "Utils.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

namespace logger
{
	enum class LogLevel : uint8_t
	{
		kNone = 0,
		kINFO,
		kWARN,
		kERROR
	};

	extern std::shared_ptr<spdlog::logger> g_logger;
	extern std::recursive_mutex			 g_console_logger_mutex;

	// Output to log file next to the plugin
	template <LogLevel _Log_LVL, class... Args>
	void log(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		std::string log_str = std::vformat(a_fmt.get(), std::make_format_args(a_args...));

		if constexpr (_Log_LVL == LogLevel::kINFO)
			g_logger->info(log_str);
		else if constexpr (_Log_LVL == LogLevel::kWARN)
			g_logger->warn(log_str);
		else if constexpr (_Log_LVL == LogLevel::kERROR)
			g_logger->error(log_str);
		else
			g_logger->info(log_str);
	}

	template <LogLevel _Log_LVL>
	void log(const char* a_str)
	{
		if constexpr (_Log_LVL == LogLevel::kINFO)
			g_logger->info(a_str);
		else if constexpr (_Log_LVL == LogLevel::kWARN)
			g_logger->warn(a_str);
		else if constexpr (_Log_LVL == LogLevel::kERROR)
			g_logger->error(a_str);
		else
			g_logger->info(a_str);
	}

	template <class... Args>
	void info(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		log<LogLevel::kINFO>(a_fmt, std::forward<Args>(a_args)...);
	}

	inline void info(const char* a_str)
	{
		log<LogLevel::kINFO>(a_str);
	}

	template <class... Args>
	void warn(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		log<LogLevel::kWARN>(a_fmt, std::forward<Args>(a_args)...);
	}

	inline void warn(const char* a_str)
	{
		log<LogLevel::kWARN>(a_str);
	}

	template <class... Args>
	void error(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		log<LogLevel::kERROR>(a_fmt, std::forward<Args>(a_args)...);
	}

	inline void error(const char* a_str)
	{
		log<LogLevel::kERROR>(a_str);
	}

	// Output to in-game console
	template <LogLevel _Log_LVL, class... Args>
	void c_log(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		auto        time_str = utils::GetCurrentTimeString();
		std::string log_str;

		if constexpr (_Log_LVL == LogLevel::kINFO)
			log_str = "INFO [" + time_str + "]: " + std::vformat(a_fmt.get(), std::make_format_args(a_args...));
		else if constexpr (_Log_LVL == LogLevel::kWARN)
			log_str = "WARN [" + time_str + "]: " + std::vformat(a_fmt.get(), std::make_format_args(a_args...));
		else if constexpr (_Log_LVL == LogLevel::kERROR)
			log_str = "ERROR [" + time_str + "]: " + std::vformat(a_fmt.get(), std::make_format_args(a_args...));
		else
			log_str = std::vformat(a_fmt.get(), std::make_format_args(a_args...));

		{
			std::lock_guard lock(g_console_logger_mutex);
			RE::ConsoleLog::GetSingleton()->PrintLine(log_str.c_str());
		}
	}

	template <LogLevel _Log_LVL>
	void c_log(const char* a_str)
	{
		auto        time_str = utils::GetCurrentTimeString();
		std::string log_str;

		if constexpr (_Log_LVL == LogLevel::kINFO)
			log_str = "INFO [" + time_str + "]: " + a_str;
		else if constexpr (_Log_LVL == LogLevel::kWARN)
			log_str = "WARN [" + time_str + "]: " + a_str;
		else if constexpr (_Log_LVL == LogLevel::kERROR)
			log_str = "ERROR [" + time_str + "]: " + a_str;
		else
			log_str = a_str;

		{
			std::lock_guard lock(g_console_logger_mutex);
			RE::ConsoleLog::GetSingleton()->PrintLine(log_str.c_str());
		}
	}

	template <class... Args>
	void c_info(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		c_log<LogLevel::kINFO>(a_fmt, std::forward<Args>(a_args)...);
	}

	template <class... Args>
	void c_info(const char* a_str)
	{
		c_log<LogLevel::kINFO>(a_str);
	}

	template <class... Args>
	void c_warn(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		c_log<LogLevel::kWARN>(a_fmt, std::forward<Args>(a_args)...);
	}

	template <class... Args>
	void c_warn(const char* a_str)
	{
		c_log<LogLevel::kWARN>(a_str);
	}

	template <class... Args>
	void c_error(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		c_log<LogLevel::kERROR>(a_fmt, std::forward<Args>(a_args)...);
	}

	template <class... Args>
	void c_error(const char* a_str)
	{
		c_log<LogLevel::kERROR>(a_str);
	}

	template <class... Args>
	void c_message(const std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		c_log<LogLevel::kNone>(a_fmt, std::forward<Args>(a_args)...);
	}

	template <class... Args>
	void c_message(const char* a_str)
	{
		c_log<LogLevel::kNone>(a_str);
	}

	void c_printf(const char* fmt, ...);
}