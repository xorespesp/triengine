#pragma once
#include <cxlib/cxlib_defs.h>
#include <cxlib/utils/bit_cast.hh>
#include <cxlib/utils/debug_panic.hh>

#if defined (_CXLIB_PLATFORM_WIN32)
#  include <Windows.h>
#else // ^^^ _CXLIB_PLATFROM_WIN32 ^^^ / vvv !_CXLIB_PLATFROM_WIN32 vvv
#  error "Not Implemented"
#endif // ^^^ !_CXLIB_PLATFROM_WIN32 ^^^

#include <filesystem>
#include <array>

_CXLIB_NAMESPACE_BEGIN
namespace utils
{
#if defined (_CXLIB_PLATFORM_WIN32)
	namespace
	{
		std::string make_win32_api_error_message(const std::string& function_name, DWORD error_code) {
			return function_name + " failed with error code " + std::to_string(error_code);
		}

		HMODULE get_current_module_handle()
		{
			// https://stackoverflow.com/a/6924332
			HMODULE hmod{};
			if (!::GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				_CXLIB utils::bit_cast<LPCWSTR>(&get_current_module_handle),
				&hmod))
			{
				CXLIB_PANIC("GetModuleHandleExW failed (last err = %lu)", ::GetLastError());
			}

			return hmod;
		}

	} // namespace

	static inline std::filesystem::path get_current_module_image_path()
	{
		const HMODULE hmod = get_current_module_handle();

		std::vector<wchar_t> buff;
		buff.resize(MAX_PATH); // Initial buffer size
		
		DWORD path_len{ 0 };
		for(;;)
		{
			path_len = ::GetModuleFileNameW(hmod, buff.data(), static_cast<DWORD>(buff.size()));
			const DWORD last_ec = ::GetLastError();

			if (path_len == 0) { // API call failed
				throw std::filesystem::filesystem_error{
					make_win32_api_error_message("GetModuleFileNameW", last_ec),
					std::error_code{ static_cast<int>(last_ec), std::system_category() }
				};
			}

			if (path_len < buff.size()) { // Success, buffer was large enough
				break;
			}

			// Buffer was too small (path_len == buff.size()).
			// Double the buffer size and try again.
			// Add a safeguard against extreme buffer sizes / infinite loops.
			constexpr size_t kMaxBufferSize = 32767 * 2; // Max reasonable path buffer size (UNICODE_STRING_MAX_CHARS * 2)
			if (buff.size() >= kMaxBufferSize) { 
				throw std::filesystem::filesystem_error{
					"Buffer size limit exceeded while trying to get module path with GetModuleFileNameW",
					std::make_error_code(std::errc::filename_too_long) // More standard error code
				};
			}

			buff.resize(buff.size() * 2);
		}

		return std::filesystem::path{ buff.data(), buff.data() + path_len };
	}

	static inline std::filesystem::path expand_env_path(
		const std::wstring& env_path /* use `const std::wstring&` instead of `std::wstring_view` */)
	{
		if (env_path.empty()) {
			return std::filesystem::path{}; // Return empty path for empty input.
		}

		// First call to ExpandEnvironmentStringsW to get the required buffer size.
		// NOTE: The returned size includes the null terminator.
		const DWORD required_size = ::ExpandEnvironmentStringsW(env_path.c_str(), nullptr, 0);
		const DWORD last_ec_size_check = ::GetLastError();

		if (required_size == 0) {
			throw std::filesystem::filesystem_error{
				make_win32_api_error_message("ExpandEnvironmentStringsW (size check)", last_ec_size_check),
				std::error_code{ static_cast<int>(last_ec_size_check), std::system_category() }
			};
		}

		std::wstring expanded_path_str;
		expanded_path_str.resize(required_size - 1/* null */); // Allocate buffer of the required size. (exclude null character)

		// Second call to actually expand the string.
		// NOTE: The returned size includes the null terminator.
		const DWORD written_chars = ::ExpandEnvironmentStringsW(env_path.c_str(), expanded_path_str.data(), required_size/* including null */);
		const DWORD last_ec_expand = ::GetLastError();

		if (written_chars == 0 || written_chars > required_size) {
			throw std::filesystem::filesystem_error{
				make_win32_api_error_message("ExpandEnvironmentStringsW", last_ec_expand),
				std::error_code{ static_cast<int>(last_ec_expand), std::system_category() }
			};
		}

		// `written_chars` includes the null terminator.
		// if `written_chars` is 1, it means the expanded string is empty ("").
		// `std::wstring::resize` should be called with the number of characters without the null terminator.
		expanded_path_str.resize(written_chars > 0 ? written_chars - 1 : 0);

		return std::filesystem::path{ expanded_path_str };
	}
#endif // ^^^ _CXLIB_PLATFROM_WIN32 ^^^

} // namespace
_CXLIB_NAMESPACE_END