#pragma once
#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <stdio.h>
#include <Windows.h>

class Logger
{
	wchar_t m_szLogFilePath[MAX_PATH];
	FILE* m_pLogFile;
public:
	static inline Logger *Get()
	{
		static Logger logger;

		return &logger;
	}

	Logger()
	{
		m_szLogFilePath[0] = 0;
		m_pLogFile = nullptr;
	}

	void open(const wchar_t* szLogFilePath)
	{
		wcscpy_s(m_szLogFilePath, szLogFilePath);
		_wfopen_s(&m_pLogFile, m_szLogFilePath, L"w");
	}

	void reopen()
	{
		if (m_szLogFilePath[0] != 0)
			_wfreopen_s(&m_pLogFile, m_szLogFilePath, L"w", m_pLogFile);
	}

	void close()
	{
		if (m_pLogFile)
		{
			fclose(m_pLogFile);
			m_pLogFile = nullptr;
		}
	}

	void log(const char *level, const wchar_t* szFormat, ...)
	{
		if (!m_pLogFile)
			return;

		va_list args;
		va_start(args, szFormat);

		SYSTEMTIME st;
		GetLocalTime(&st);

		fwprintf_s(m_pLogFile, L"[%02d:%02d:%02d.%03d] [%-8S] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);
		vfwprintf_s(m_pLogFile, szFormat, args);
		fwprintf_s(m_pLogFile, L"\n");

		va_end(args);

		fflush(m_pLogFile);
	}

	void log(const char* level, const wchar_t* szFormat, va_list args)
	{
		if (!m_pLogFile)
			return;

		SYSTEMTIME st;
		GetLocalTime(&st);

		fwprintf_s(m_pLogFile, L"[%02d:%02d:%02d.%03d] [%-8S] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);
		vfwprintf_s(m_pLogFile, szFormat, args);
		fwprintf_s(m_pLogFile, L"\n");

		fflush(m_pLogFile);
	}

	void log(const char *level, const char* szFormat, ...)
	{
		if (!m_pLogFile)
			return;

		va_list args;
		va_start(args, szFormat);

		SYSTEMTIME st;
		GetLocalTime(&st);

		fprintf_s(m_pLogFile, "[%02d:%02d:%02d.%03d] [%-8s] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);
		vfprintf_s(m_pLogFile, szFormat, args);
		fprintf_s(m_pLogFile, "\n");

		va_end(args);

		fflush(m_pLogFile);
	}

	void log(const char* level, const char* szFormat, va_list args)
	{
		if (!m_pLogFile)
			return;

		SYSTEMTIME st;
		GetLocalTime(&st);

		fprintf_s(m_pLogFile, "[%02d:%02d:%02d.%03d] [%-8s] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);
		vfprintf_s(m_pLogFile, szFormat, args);
		fprintf_s(m_pLogFile, "\n");

		fflush(m_pLogFile);
	}
};

#endif