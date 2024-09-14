#pragma once

#include <cassert>
#include <iostream>
#include <mutex>
#include <string_view>

namespace mirai
{
	class Log
	{
	public:
		template<typename... Args>
		static void Debug(Args &&...args)
		{
			Write(Blue, args...);
		}

		template<typename... Args>
		static void Info(Args &&...args)
		{
			Write(DEFAULT, args...);
		}

		template<typename... Args>
		static void Warn(Args &&...args)
		{
			Write(Yellow, args...);
		}

		template<typename... Args>
		static void Error(Args &&...args)
		{
			Write(Red, args...);
		}


	private:
		static std::mutex WriteMutex;

		template<typename... Args>
		static void Write(Args &&...args)
		{
			std::unique_lock<std::mutex> lock(WriteMutex);
			(std::cout << ... << args) << '\n';
		}

		constexpr static std::string_view DEFAULT = "\033[39m";
		constexpr static std::string_view Red = "\033[31m";
		constexpr static std::string_view Yellow = "\033[33m";
		constexpr static std::string_view Blue = "\033[34m";
	};
}