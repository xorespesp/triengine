#pragma once
#include "bit_cast.hh"
#include "logger.hh"
#include "exception.hh"

#include <Windows.h>
#include <filesystem>
#include <array>

namespace utils
{
	static inline HMODULE get_current_module_handle()
	{
		// https://stackoverflow.com/a/6924332
		HMODULE hmod{};
		if (!::GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			utils::bit_cast<LPCWSTR>(&get_current_module_handle),
			&hmod))
		{
			LOG_ERROR("GetModuleHandleExW failed (last err = %lu)", ::GetLastError());
			return nullptr;
		}

		return hmod;
	}

	static inline std::filesystem::path get_current_module_image_path()
	{
		std::array<wchar_t, MAX_PATH> buff{};
		if (!::GetModuleFileNameW(get_current_module_handle(), &buff[0], static_cast<DWORD>(buff.size()))) {
			THROW_EXCEPTION("GetModuleFileNameW failed (last err = %lu)", ::GetLastError());
		}

		return std::filesystem::path{ buff.data() };
	}

	static inline std::filesystem::path expand_env_path(
		const std::wstring& env_path /* use `const std::wstring&` instead of `std::wstring_view` */)
	{
		std::array<wchar_t, MAX_PATH> buff{};
		const auto sz = ::ExpandEnvironmentStringsW(env_path.c_str(), &buff[0], static_cast<DWORD>(buff.size()));
		if (!sz) {
			THROW_EXCEPTION("ExpandEnvironmentStringsW failed (last err = %lu)", ::GetLastError());
		}

		return std::filesystem::path{ buff.data() };
	}

} // namespace